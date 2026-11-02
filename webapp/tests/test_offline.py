import ctypes
import os
import socket
import struct
import subprocess
import sys
from pathlib import Path
import pytest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import server
from fastapi.testclient import TestClient
from catalog import FEATURE_MAP
from camera import Camera,CameraError
from imaging import jpeg_frame

@pytest.fixture
def client():
    server.sessions.clear();server.attempts.clear();server.mutation_times.clear()
    with TestClient(server.app) as c:yield c

def login(c):
    r=c.post('/api/login',json={'username':'admin','password':'admin'},headers={'Origin':'http://testserver'})
    assert r.status_code==200
    return {'Origin':'http://testserver','X-CSRF-Token':r.json()['csrf']}

def test_auth_and_csrf(client):
    assert client.get('/api/catalog').status_code==401
    assert client.post('/api/login',json={'username':'admin','password':'admin'}).status_code==403
    assert client.post('/api/login',json={'username':'admin','password':'wrong'},headers={'Origin':'http://testserver'}).status_code==401
    headers=login(client)
    assert client.get('/api/catalog').status_code==200
    assert client.post('/api/release',json={},headers={'Origin':'http://testserver'}).status_code==403
    assert client.post('/api/release',json={},headers={**headers,'Origin':'https://evil.example'}).status_code==403
    assert 'HttpOnly' in client.cookies.get('maxcam_session','') or client.cookies.get('maxcam_session')

def test_headers_and_request_limits(client):
    r=client.get('/')
    assert r.status_code==200 and r.headers['x-frame-options']=='DENY'
    assert "frame-ancestors 'none'" in r.headers['content-security-policy']
    assert client.post('/api/login',content='x'*5000).status_code==413
    assert client.put('/api/login').status_code==405

def test_login_rate_limit(client):
    for _ in range(10):
        assert client.post('/api/login',json={'username':'admin','password':'wrong'},headers={'Origin':'http://testserver'}).status_code==401
    assert client.post('/api/login',json={'username':'admin','password':'admin'},headers={'Origin':'http://testserver'}).status_code==429

def test_sensitive_features_restricted():
    for name in ('UserSetSave','UserSetDefault','FactoryUserSetSave','DeviceReset','SensorFeatureApply'):
        assert not FEATURE_MAP[name]['remote_writable']
    for f in FEATURE_MAP.values():
        if f['group']=='NetworkControl':assert not f['remote_writable']
    assert FEATURE_MAP['ExposureTime']['remote_writable']

def bare_camera():
    cam=Camera.__new__(Camera);cam.lock=__import__('threading').RLock();cam.acquired=True;cam.owner='owner';cam.streaming=False;cam.stopping=False;cam.values={}
    cam.read=lambda a:0;cam.write=lambda a,v:pytest.fail('Invalid input reached camera write')
    return cam

@pytest.mark.parametrize('name,value',[('DeviceReset',1),('Width',-1),('GainFactor',float('nan')),('ExposureAuto',1),('ReverseX',2),('not_a_feature',1),('Width',160.5),('Width','http://10.0.0.138')])
def test_write_allowlist_and_validation(name,value):
    cam=bare_camera()
    with pytest.raises(CameraError):cam.set_feature('owner',name,value)

def test_ownership_and_live_write_lock():
    cam=bare_camera()
    with pytest.raises(CameraError):cam.set_feature('other','GainFactor',1)
    cam.streaming=True
    with pytest.raises(CameraError):cam.set_feature('owner','GainFactor',1)

def test_image_conversions():
    from PIL import Image
    import io
    for pf,data in [(0x01080001,bytes([0,255,128,64])),(0x02180014,bytes([255,0,0]*4)),(0x02100032,bytes([16,128,235,128]*2))]:
        im=Image.open(io.BytesIO(jpeg_frame(data,2,2,pf)));assert im.size==(2,2)
    with pytest.raises(ValueError):jpeg_frame(b'',2,2,0xdeadbeef)

def test_seccomp_denies_new_network_and_retargeting():
    code='''import socket,sys
from security import lock_network
s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.connect(('127.0.0.1',9))
lock_network()
for op in [lambda:socket.socket(socket.AF_INET,socket.SOCK_STREAM),lambda:s.connect(('10.0.0.138',80)),lambda:s.sendto(b'x',('10.0.0.138',3956))]:
 try:op()
 except PermissionError:pass
 else:raise SystemExit('Network restriction failed')
s.close()
'''
    r=subprocess.run([sys.executable,'-c',code],cwd=Path(__file__).resolve().parents[1],capture_output=True,text=True)
    assert r.returncode==0,r.stderr

def test_gvsp_reordered_payload_and_partial_rejection():
    from concurrent.futures import ThreadPoolExecutor
    from imaging import jpeg_frame
    lib=ctypes.CDLL(str(Path(__file__).resolve().parents[1]/'.runtime/receiver.so'))
    lib.receive_frame.argtypes=[ctypes.c_int,ctypes.c_void_p,*([ctypes.c_uint]*5),ctypes.POINTER(ctypes.c_uint)]
    receive=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);receive.bind(('127.0.0.1',0));send=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    buf=ctypes.create_string_buffer(12);bad=ctypes.c_uint()
    def packet(kind,id,payload=b''):return struct.pack('!HHI',0,1,kind<<24|id)+payload
    leader=bytearray(36);struct.pack_into('!III',leader,12,0x02180014,2,2)
    trailer=struct.pack('!II',1,2)
    def run():return lib.receive_frame(receive.fileno(),buf,12,2,2,0x02180014,struct.unpack('=I',socket.inet_aton('127.0.0.1'))[0],ctypes.byref(bad))
    try:
        with ThreadPoolExecutor() as pool:
            future=pool.submit(run)
            for p in [packet(1,0,leader),packet(3,1,b'abcd'),packet(3,3,b'ijkl'),packet(2,4,trailer),packet(3,2,b'efgh')]:send.sendto(p,receive.getsockname())
            assert future.result(timeout=2)==1
        assert buf.raw==b'abcdefghijkl'
        send.sendto(packet(1,0,leader),receive.getsockname());send.sendto(packet(3,1,b'abcd'),receive.getsockname());send.sendto(packet(2,4,trailer),receive.getsockname())
        assert run()==0 and bad.value>=1
    finally:receive.close();send.close()
