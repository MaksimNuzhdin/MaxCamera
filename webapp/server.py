"""Authenticated fixed-camera API. Runs only under the isolated launcher."""
import asyncio
import hashlib
import hmac
import json
import logging
import os
import secrets
import socket
import threading
import time
from collections import defaultdict,deque
from contextlib import asynccontextmanager
from pathlib import Path
from urllib.parse import urlsplit
import uvicorn
from fastapi import FastAPI, Request, HTTPException
from fastapi.responses import FileResponse, JSONResponse, Response
from fastapi.staticfiles import StaticFiles
from camera import Camera,CameraError
from catalog import public_catalog,FEATURE_MAP
from security import lock_network

BASE=Path(__file__).parent
sessions={};attempts=defaultdict(deque);mutation_times=defaultdict(deque);session_lock=threading.RLock()
PASSWORD_SALT=b'MaxCameraWeb-login-v1'
PASSWORD_HASH=hashlib.scrypt(os.environ.get('MAXCAM_PASSWORD','admin').encode(),salt=PASSWORD_SALT,n=16384,r=8,p=1)
cam=None

def authenticated(request):
    sid=request.cookies.get('maxcam_session','')
    with session_lock:
        s=sessions.get(sid)
        if not s or s['expires']<time.monotonic():
            sessions.pop(sid,None);raise HTTPException(401,'Your session has expired. Please sign in.')
        return sid,s

def check_origin(request):
    origin=request.headers.get('origin')
    if not origin or urlsplit(origin).netloc!=request.headers.get('host'):raise HTTPException(403,'Request origin rejected')

@asynccontextmanager
async def lifespan(app):
    yield
    if cam:cam.release()

app=FastAPI(docs_url=None,redoc_url=None,openapi_url=None,lifespan=lifespan)

@app.middleware('http')
async def guard(request,call_next):
    try:
        if request.method not in ('GET','POST'):return JSONResponse({'detail':'Method not allowed'},405)
        if int(request.headers.get('content-length','0'))>4096:return JSONResponse({'detail':'Request too large'},413)
    except ValueError:return JSONResponse({'detail':'Invalid length'},400)
    response=await call_next(request)
    for name,value in {'Content-Security-Policy':"default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' blob:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'",'X-Content-Type-Options':'nosniff','Referrer-Policy':'no-referrer','Permissions-Policy':'camera=(), microphone=(), geolocation=()','Cache-Control':'no-store','X-Frame-Options':'DENY'}.items():response.headers[name]=value
    return response

async def body(request):
    raw=bytearray()
    async for chunk in request.stream():
        raw.extend(chunk)
        if len(raw)>4096:raise HTTPException(413,'Request too large')
    try:
        result=json.loads(raw)
        if not isinstance(result,dict):raise ValueError()
        return result
    except (ValueError,UnicodeError):raise HTTPException(400,'Invalid JSON object')

@app.exception_handler(CameraError)
async def camera_error(request,exc):return JSONResponse({'detail':str(exc)},409)

@app.get('/')
def index():return FileResponse(BASE/'static/index.html')

@app.get('/guide')
def guide(request:Request):
    authenticated(request);return FileResponse(BASE/'guide.html')

@app.get('/healthz')
def health():return {'status':'ok','isolation':'network-namespace+seccomp' if os.environ.get('MAXCAM_ISOLATED')=='1' else 'test'}

@app.post('/api/login')
async def login(request:Request):
    check_origin(request);data=await body(request)
    # Global plus per-peer limits do not trust attacker-controlled proxy headers.
    peer=request.client.host if request.client else 'unknown';now=time.monotonic()
    with session_lock:
        for key,limit in [('global',30),(peer,10)]:
            q=attempts[key]
            while q and now-q[0]>300:q.popleft()
            if len(q)>=limit:raise HTTPException(429,'Too many sign-in attempts. Try again in five minutes.')
        attempts['global'].append(now);attempts[peer].append(now)
    username=data.get('username');password=data.get('password')
    if not isinstance(password,str) or len(password)>256 or not isinstance(username,str):raise HTTPException(401,'Invalid username or password')
    candidate=await asyncio.to_thread(hashlib.scrypt,password.encode(),salt=PASSWORD_SALT,n=16384,r=8,p=1)
    if not (hmac.compare_digest(candidate,PASSWORD_HASH) and hmac.compare_digest(username,'admin')):raise HTTPException(401,'Invalid username or password')
    sid=secrets.token_urlsafe(32);csrf=secrets.token_urlsafe(32)
    with session_lock:
        for key in list(sessions):
            if sessions[key]['expires']<now:sessions.pop(key,None)
        if len(sessions)>=20:raise HTTPException(429,'Too many active sessions')
        sessions[sid]={'csrf':csrf,'expires':now+3600}
    secure=request.url.scheme=='https' or os.environ.get('MAXCAM_SECURE_COOKIE')=='1'
    response=JSONResponse({'csrf':csrf});response.set_cookie('maxcam_session',sid,httponly=True,secure=secure,samesite='strict',max_age=3600,path='/')
    return response

@app.get('/api/session')
def session(request:Request):
    _,s=authenticated(request);return {'csrf':s['csrf']}

@app.get('/api/catalog')
def catalog(request:Request):authenticated(request);return public_catalog()

@app.get('/api/status')
def status(request:Request):
    sid,_=authenticated(request);return cam.status(sid)

@app.get('/api/frame')
def frame(request:Request):
    sid,_=authenticated(request)
    with cam.lock:
        cam._check(sid)
        if not cam.streaming or not cam.jpeg:raise HTTPException(404,'Waiting for a complete frame')
        if time.monotonic()-cam.frame_time>3:raise HTTPException(503,'Camera frame is stale')
        return Response(cam.jpeg,media_type='image/jpeg',headers={'X-Frame-Number':str(cam.frames)})

@app.get('/api/settings')
def settings(request:Request):
    sid,_=authenticated(request)
    with cam.lock:
        cam._check(sid)
        return JSONResponse({'schema':1,'settings':{k:v for k,v in cam.values.items() if FEATURE_MAP[k]['remote_writable'] and FEATURE_MAP[k]['kind']!='Command'}},headers={'Content-Disposition':'attachment; filename="maxcamera-settings.json"'})

@app.post('/api/{action}')
async def mutate(action:str,request:Request):
    check_origin(request);sid,s=authenticated(request)
    if not hmac.compare_digest(request.headers.get('x-csrf-token',''),s['csrf']):raise HTTPException(403,'Invalid request token')
    now=time.monotonic()
    with session_lock:
        q=mutation_times[sid]
        while q and now-q[0]>10:q.popleft()
        if len(q)>=40 and action not in ('release','stop','logout'):raise HTTPException(429,'Too many camera operations; wait a moment')
        q.append(now)
    data=await body(request)
    if action=='logout':
        if cam.owner==sid:await asyncio.to_thread(cam.release,sid)
        with session_lock:sessions.pop(sid,None);mutation_times.pop(sid,None)
        response=JSONResponse({'ok':True});response.delete_cookie('maxcam_session');return response
    if action=='connect':await asyncio.to_thread(cam.connect,sid)
    elif action=='release':await asyncio.to_thread(cam.release,sid)
    elif action=='start':await asyncio.to_thread(cam.start,sid)
    elif action=='stop':await asyncio.to_thread(cam.stop,sid)
    elif action=='refresh':await asyncio.to_thread(cam.refresh,sid)
    elif action=='feature':
        if set(data)!={'name','value'} or not isinstance(data['name'],str):raise HTTPException(400,'Expected feature name and value')
        await asyncio.to_thread(cam.set_feature,sid,data['name'],data['value'])
    else:raise HTTPException(404,'Unknown operation')
    return {'ok':True}

app.mount('/static',StaticFiles(directory=BASE/'static'),name='static')

if __name__=='__main__':
    if os.environ.get('MAXCAM_ISOLATED')!='1':raise SystemExit('Use launch.py. Unisolated public service is refused.')
    control=socket.socket(fileno=int(os.environ['MAXCAM_CONTROL_FD']))
    receiver=socket.socket(fileno=int(os.environ['MAXCAM_RECEIVER_FD']))
    lock_network()
    cam=Camera(control,receiver,os.environ['MAXCAM_CAMERA_IP'])
    try:uvicorn.run(app,fd=int(os.environ['MAXCAM_HTTP_FD']),host='127.0.0.1',proxy_headers=False,access_log=False,log_level='info',limit_concurrency=40,timeout_keep_alive=5)
    finally:
        cam.release();control.close();receiver.close()
