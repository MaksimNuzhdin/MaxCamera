"""Explicit short hardware check; never run as part of the offline test suite."""
import json
import os
from pathlib import Path
import time
import httpx

def main():
    origin=os.environ.get('MAXCAM_TEST_URL','http://127.0.0.1:8787')
    c=httpx.Client(base_url=origin,timeout=40,headers={'Origin':origin})
    r=c.post('/api/login',json={'username':'admin','password':os.environ.get('MAXCAM_PASSWORD','admin')});r.raise_for_status();c.headers['X-CSRF-Token']=r.json()['csrf']
    report={};connected=False
    try:
        r=c.post('/api/connect',json={});print('CONNECT',r.status_code,r.text,flush=True);r.raise_for_status();connected=True
        s=c.get('/api/status').json();report['initial']=s
        print('FEATURES',len(s['values']),'FORMAT',s['values']['Width'],s['values']['Height'],hex(s['values']['PixelFormat']),flush=True)
        r=c.post('/api/start',json={});print('START',r.status_code,r.text,flush=True);r.raise_for_status()
        deadline=time.monotonic()+12
        while time.monotonic()<deadline:
            time.sleep(.4);r=c.get('/api/frame')
            if r.status_code==200:
                Path('.runtime/live-test.jpg').write_bytes(r.content);report['jpeg_bytes']=len(r.content)
                if time.monotonic()>deadline-7:print('JPEG',len(r.content),flush=True);break
        else:raise RuntimeError('No preview received')
        report['stream']=c.get('/api/status').json();print('STREAM',json.dumps({k:v for k,v in report['stream'].items() if k!='values'}),flush=True)
        r=c.post('/api/stop',json={});print('STOP',r.status_code,flush=True);r.raise_for_status()
        # A same-value write demonstrates control without altering the optical configuration.
        s=c.get('/api/status').json();name='GainFactor' if s['values']['GainAuto']==0 else 'FanDutyCycle'
        r=c.post('/api/feature',json={'name':name,'value':s['values'][name]});print('NOOP_WRITE',name,r.status_code,r.text,flush=True);r.raise_for_status()
        report['write_feature']=name;report['passed']=True
    finally:
        if connected:
            r=c.post('/api/release',json={});print('RELEASE',r.status_code,r.text,flush=True);report['release_status']=r.status_code
        Path('.runtime/integration.json').write_text(json.dumps(report,indent=2))
        c.post('/api/logout',json={});c.close()
if __name__=='__main__':main()
