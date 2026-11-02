#!/usr/bin/env python3
"""Open only fixed sockets, then enter a read-only filesystem and network sandbox."""
import argparse
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--camera',default='10.0.0.3',help='Local operator only; never accepted over HTTP')
p.add_argument('--port',type=int,default=8787)
p.add_argument('--secure-cookie',action='store_true',help='Required when serving through the public HTTPS tunnel')
a=p.parse_args();socket.inet_aton(a.camera)
base=Path(__file__).resolve().parent
if not shutil.which('bwrap'):raise SystemExit('bubblewrap is required; refusing unisolated launch')
# Resolve source route without opening camera ownership.
control=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);control.connect((a.camera,3956))
host=control.getsockname()[0]
receiver=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);receiver.setsockopt(socket.SOL_SOCKET,socket.SO_RCVBUF,32*1024*1024);receiver.bind((host,0))
http=socket.socket(socket.AF_INET,socket.SOCK_STREAM);http.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1);http.bind(('127.0.0.1',a.port));http.listen(64)
fds=[control.fileno(),receiver.fileno(),http.fileno()]
if sorted(fds)!=list(range(3,6)):raise SystemExit('Unexpected inherited socket descriptors; refusing launch')
cmd=['bwrap','--die-with-parent','--unshare-all','--new-session','--cap-drop','ALL','--ro-bind','/usr','/usr','--ro-bind','/lib','/lib','--ro-bind','/lib64','/lib64','--proc','/proc','--dev','/dev','--tmpfs','/tmp','--dir','/etc','--ro-bind','/etc/ld.so.cache','/etc/ld.so.cache','--ro-bind',str(base),'/app','--ro-bind',str(base.parent/'SW'),'/src/SW','--ro-bind',str(Path.home()/'.local/lib/python3.10/site-packages'),'/py-user','--chdir','/app','/usr/bin/python3','/app/server.py']
env={'PATH':'/usr/bin:/bin','HOME':'/tmp','LANG':'C.UTF-8','PYTHONPATH':'/app/.runtime/python:/py-user','PYTHONDONTWRITEBYTECODE':'1','MAXCAM_SOURCE':'/src','MAXCAM_ISOLATED':'1','MAXCAM_CONTROL_FD':str(fds[0]),'MAXCAM_RECEIVER_FD':str(fds[1]),'MAXCAM_HTTP_FD':str(fds[2]),'MAXCAM_CAMERA_IP':a.camera,'MAXCAM_SECURE_COOKIE':'1' if a.secure_cookie else '0','OPENBLAS_NUM_THREADS':'1','OMP_NUM_THREADS':'1'}
if 'MAXCAM_PASSWORD' in os.environ:env['MAXCAM_PASSWORD']=os.environ['MAXCAM_PASSWORD']
print(f'MaxCamera Console: http://127.0.0.1:{a.port}; camera remains unowned until Connect',flush=True)
try:
    child=subprocess.Popen(cmd,env=env,pass_fds=fds)
    control.close();receiver.close();http.close()
    sys.exit(child.wait())
except KeyboardInterrupt:
    child.terminate()
    try:child.wait(timeout=8)
    except subprocess.TimeoutExpired:child.kill();child.wait()
finally:
    control.close();receiver.close();http.close()
