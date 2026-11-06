#!/usr/bin/env python3
"""Send an explicit ASCII command to the local Linux UART and retain its response."""
import argparse
import base64
import shlex
import zlib
import hashlib
import fcntl
import os
import select
import termios
import time
from pathlib import Path

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port',default='/dev/ttyUSB0')
action=parser.add_mutually_exclusive_group(required=True)
action.add_argument('--command')
action.add_argument('--upload',type=Path)
parser.add_argument('--destination')
parser.add_argument('--log',type=Path,required=True)
parser.add_argument('--seconds',type=float,default=8)
a=parser.parse_args()
if a.upload:
    if not a.destination or not a.destination.startswith('/'):
        parser.error('--upload requires an absolute --destination')
    code="import sys,base64,hashlib,zlib;d=zlib.decompress(base64.b64decode(''.join(iter(lambda:sys.stdin.readline().strip(),'AI_UPLOAD_END'))));open("+repr(a.destination)+",'wb').write(d);print('AI_UPLOAD_OK',len(d),hashlib.sha256(d).hexdigest())"
    upload_data=a.upload.read_bytes()
    expected=('AI_UPLOAD_OK '+str(len(upload_data))+' '+hashlib.sha256(upload_data).hexdigest()).encode()
    encoded=base64.b64encode(zlib.compress(upload_data)).decode('ascii')
    lines=[encoded[i:i+512] for i in range(0,len(encoded),512)]
    command=('python3 -c '+shlex.quote(code)+'\n'+'\n'.join(lines)+'\nAI_UPLOAD_END\n').encode('ascii')
else:
    command=a.command.encode('ascii')+b'\n' 
fd=os.open(a.port,os.O_RDWR|os.O_NOCTTY|os.O_NONBLOCK)
fcntl.ioctl(fd,termios.TIOCEXCL);old=termios.tcgetattr(fd)
try:
    cfg=termios.tcgetattr(fd);cfg[0]=cfg[1]=cfg[3]=0
    cfg[2]=termios.CS8|termios.CREAD|termios.CLOCAL
    cfg[4]=cfg[5]=termios.B115200
    cfg[6][termios.VMIN]=cfg[6][termios.VTIME]=0
    termios.tcsetattr(fd,termios.TCSANOW,cfg)
    pending=memoryview(command)
    end=time.monotonic()+max(a.seconds,len(command)*10/115200+6)
    received=bytearray()
    with a.log.open('ab',buffering=0) as output:
        while time.monotonic()<end:
            readable,writable,_=select.select([fd],[fd] if pending else [],[],min(.5,max(0,end-time.monotonic())))
            if writable:
                try:pending=pending[os.write(fd,pending[:256]):]
                except BlockingIOError:pass
            if readable:
                data=os.read(fd,8192);output.write(data);received.extend(data)
                print(data.decode('utf-8',errors='replace'),end='',flush=True)
    if pending:raise TimeoutError('UART command was not completely transmitted')
    if a.upload and expected not in received:raise RuntimeError('UART upload hash confirmation was not received')
finally:
    termios.tcsetattr(fd,termios.TCSANOW,old);os.close(fd)
