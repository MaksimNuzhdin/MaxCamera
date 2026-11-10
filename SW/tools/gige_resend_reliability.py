#!/usr/bin/env python3
"""Byte-exact controlled packet-loss recovery while the camera serves HTTP.

Losses are injected only in this receiver. Volatile controls are restored.
"""
import argparse
from collections import Counter
import gc
import hashlib
import json
from pathlib import Path
import socket
import struct
import threading
import time
from urllib.error import HTTPError
from urllib.request import build_opener, ProxyHandler

from gige_reliability_common import counters, native, save
from gige_probe import Control
from gige_profile_backup import full_restore, full_snapshot
import stream_led_camera_common as common
from stream_led_camera_common import Aravis, idle, write


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--ip',default='10.0.0.10');p.add_argument('--host',default='10.0.0.7')
    p.add_argument('--frames',type=int,default=100);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();assert 10<=a.frames<=1000
    a.output.mkdir(parents=True,exist_ok=False)
    common.Control=lambda _ip,_host:Control(a.ip,a.host)
    cam=Aravis.Camera.new(a.ip);d=cam.get_device();original=None;s=None;worker=None
    stop=threading.Event();http_results=[];result=dict(passed=False,scope='Software packet discard with exact original-versus-resent bytes; not kernel-loss or endurance acceptance')
    def traffic():
        http=build_opener(ProxyHandler({}));index=0
        while not stop.is_set():
            path=('/api/status','/','/style.css','/app.js')[index%4];index+=1
            try:
                with http.open('http://'+a.ip+path,timeout=3) as response:data=response.read();code=response.status
            except HTTPError as e:data=e.read();code=e.code
            except (OSError,TimeoutError) as e:data=b'';code=type(e).__name__
            http_results.append(dict(path=path,status=code,bytes=len(data)))
    try:
        idle(d);original=full_snapshot(cam);save(a.output/'runtime-before.json',original)
        result['firmware']=d.get_string_feature_value('DeviceVersion')
        native(cam,'Mono8',320,240,bars=True);write(d,'TEST_ROLL',1)
        write(d,'FRAME_RATE_ENABLE',1);write(d,'SENSOR_FPS',10000);d.write_register(0xd08,50)
        s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind((a.host,0));s.settimeout(.05)
        s.setsockopt(socket.SOL_SOCKET,socket.SO_RCVBUF,32*1024*1024)
        d.write_register(0xd18,int.from_bytes(socket.inet_aton(a.host),'big'))
        d.write_register(0xd00,s.getsockname()[1]);d.write_register(0xd04,1500)
        payload=cam.get_payload();packets=(payload+1463)//1464+2
        lost={0,2,3,packets-2,packets-1};states={};complete=0;resent=0;rid=0
        before=counters(d)
        resend_names=['PacketResendRequests','RetransmittedPackets','PacketResendUnavailable','PacketResendQueueOverflows']
        rb={n:d.get_integer_feature_value(n) for n in resend_names}
        cam.start_acquisition();worker=threading.Thread(target=traffic);worker.start()
        deadline=time.monotonic()+a.frames/10+15
        completed=[]
        while complete<a.frames:
            assert time.monotonic()<deadline,'Incomplete recovery before deadline'
            try:data,peer=s.recvfrom(2048)
            except socket.timeout:continue
            if peer[0]!=a.ip:continue
            status,block,word=struct.unpack_from('!HHI',data);pid=word&0xffffff
            state=states.setdefault(block,dict(original={},received={},done=False,first_host=time.monotonic(),requests=[]))
            if status not in (0,0x100):
                result['unavailable_event']=dict(status=status,block=block,packet_id=pid,frame_age_ms=1000*(time.monotonic()-state['first_host']),requests=state['requests'],completed=complete)
            assert status in (0,0x100),(status,block,pid,result.get('unavailable_event'))
            if status==0:
                assert pid not in state['original'],'Duplicate original packet'
                state['original'][pid]=data
                if pid not in lost:state['received'][pid]=data
                elif pid in state['received']:
                    assert state['received'][pid][2:]==data[2:],'Resent bytes differ from delayed original'
                if pid in (4,packets-1):
                    ranges=[(0,0),(2,3)] if pid==4 else [(packets-2,packets-1)]
                    for first,last in ranges:
                        rid=rid%65535+1
                        state['requests'].append(dict(first=first,last=last,frame_age_ms=1000*(time.monotonic()-state['first_host'])))
                        s.sendto(struct.pack('!BBHHHHHII',0x42,0,0x40,12,rid,0,block,first,last),(a.ip,3956))
            else:
                assert pid in lost,'Unexpected resent packet'
                if pid in state['original']:assert data[2:]==state['original'][pid][2:],'Resent bytes differ'
                state['received'][pid]=data;resent+=1
            if not state['done'] and len(state['received'])==packets and len(state['original'])==packets:
                state['done']=True;complete+=1
                image=b''.join(state['received'][i][8:] for i in range(1,packets-1))
                original_image=b''.join(state['original'][i][8:] for i in range(1,packets-1))
                assert len(image)==payload and image==original_image
                completed.append(dict(block=block,sha256=hashlib.sha256(image).hexdigest(),recovered=len(lost)))
                # Retain bounded state; histories older than two frames are not needed.
                for old in list(states):
                    if old!=block and states[old]['done']:del states[old]
                if complete%10==0:save(a.output/'progress.json',dict(complete=complete,recovered=resent))
        stop.set();worker.join(timeout=4);assert not worker.is_alive()
        cam.stop_acquisition();idle(d)
        ra={n:d.get_integer_feature_value(n)-v for n,v in rb.items()}
        after=counters(d);delta={k:after[k]-v for k,v in before.items()}
        statuses=dict(Counter(str(q['status']) for q in http_results))
        assert resent==a.frames*len(lost) and ra['RetransmittedPackets']==resent,ra
        assert ra['PacketResendUnavailable']==ra['PacketResendQueueOverflows']==0,ra
        assert all(delta[n]==0 for n in ['DROPPED','CSI_ERRORS','RECOVERIES','GEM_ERRORS','RX_DROPS']),delta
        assert len(http_results)>=4 and statuses=={'200':len(http_results)},statuses
        result.update(passed=True,frames=complete,intentional_losses=complete*len(lost),recovered=resent,
                      leader_payload_trailer=True,camera_delta=delta,resend=ra,http_statuses=statuses,frames_verified=completed)
    except BaseException as error:
        result['error']=repr(error);raise
    finally:
        stop.set()
        if worker:worker.join(timeout=4)
        try:
            cam.stop_acquisition();idle(d)
            if original is not None:
                full_restore(cam,original);save(a.output/'runtime-restored.json',full_snapshot(cam));result['restored']=True
        finally:
            if s:s.close()
            save(a.output/'http.json',http_results);save(a.output/'result.json',result)
            d=None;cam=None;gc.collect();Aravis.shutdown()
    print(json.dumps({k:v for k,v in result.items() if k!='frames_verified'}),flush=True)
    return 0


if __name__=='__main__':raise SystemExit(main())
