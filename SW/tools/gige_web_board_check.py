#!/usr/bin/env python3
"""Bounded same-mode GVSP comparison with HTTP traffic; volatile settings only."""
import argparse
import gc
import json
import time
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import Request, build_opener, ProxyHandler
from gige_probe import Control
from stream_led_camera_common import Aravis, idle
import stream_led_camera_common as common
from gige_profile_backup import full_snapshot, full_restore
from gige_udp_capture import capture


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ip',required=True)
    parser.add_argument('--host',default='10.0.0.7')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--seconds',type=int,default=10)
    parser.add_argument('--ai-mask',type=int,choices=(0,1),default=0)
    args = parser.parse_args()
    if not 5<=args.seconds<=60:
        parser.error('Choose a bounded capture of5–60 seconds')
    args.output.mkdir(parents=True,exist_ok=True)
    http = build_opener(ProxyHandler({}))
    def get(path):
        with http.open('http://'+args.ip+path,timeout=5) as response:
            return response.read()
    assert b'MaxCamera' in get('/')
    status=json.loads(get('/api/status'))
    assert status['transport']=='ethernet' and not status['preview']
    wire=Control(args.ip,args.host)
    try:
        discovery,_=wire.command(2,flags=1)
        assert discovery[10:16]==bytes.fromhex('020000000001'),'Unexpected device identity'
        assert wire.read(0xa00)==0 and wire.read(0x10018)==0,'Camera is already owned or acquiring'
    finally:
        wire.sock.close()
    assert status['config']['mask']==0,'Trial requires an idle AI configuration'
    def apply(config):
        request=Request('http://'+args.ip+'/api/config',data=json.dumps(config).encode(),method='POST',
            headers={'X-Camera-Token':status['token'],'Content-Type':'application/json'})
        with http.open(request,timeout=5) as response:
            assert response.status==200
    try:
        changed=dict(status['config'],score=600000 if status['config']['score']!=600000 else 500000)
        apply(changed)
        assert json.loads(get('/api/status'))['config']==changed,'R5 did not apply the web command'
    finally:
        apply(status['config'])
    common.Control=lambda _ip,_host:Control(args.ip,args.host)
    cam=Aravis.Camera.new(args.ip)
    device=cam.get_device()
    original=None;results=[];http_records=[]
    try:
        idle(device);original=full_snapshot(cam)
        selector=device.get_string_feature_value('UserSetSelector')
        (args.output/'runtime-before.json').write_text(json.dumps(original,indent=2)+'\n')
        # Reuse the already qualified bounded FPGA RGB profile; never save it.
        device.set_string_feature_value('UserSetSelector','UserSet7')
        device.execute_command('UserSetLoad');idle(device)
        assert device.get_integer_feature_value('UserSetLastError')==0
        assert cam.get_pixel_format_as_string()=='RGB8' and cam.get_region()[2:]==(640,480)
        device.write_register(0xd08,20)
        device.set_integer_feature_value('MaxAIEnabledFeatures',args.ai_mask)
        for phase in ('baseline','http-load'):
            def traffic():
                deadline=time.monotonic()+args.seconds-1
                # An active acquisition must reject writes at the R5 boundary.
                request=Request('http://'+args.ip+'/api/config',
                    data=json.dumps(status['config']).encode(),method='POST',
                    headers={'X-Camera-Token':status['token'],'Content-Type':'application/json'})
                try:
                    http.open(request,timeout=3)
                    raise AssertionError('HTTP write unexpectedly accepted while streaming')
                except HTTPError as error:
                    assert error.code==409,error.code
                    error.read()
                index=0
                while time.monotonic()<deadline:
                    path=('/api/status','/','/style.css','/app.js')[index%4]
                    start=time.monotonic();data=get(path)
                    http_records.append(dict(path=path,bytes=len(data),milliseconds=1000*(time.monotonic()-start)))
                    if path=='/api/status':
                        live=json.loads(data)
                        assert live['streaming'] and live['owned']
                    index+=1
            record=capture(cam,args.ip,args.output/(phase+'.bin'),args.seconds,
                           on_started=traffic if phase=='http-load' else None)
            ai_snapshot=None
            if args.ai_mask:
                device.execute_command('MaxAIResultLatch')
                try:
                    ai_snapshot={name:device.get_integer_feature_value('MaxAI'+name) for name in
                                 ('ResultSequence','InferenceTimeUs','ErrorCode')}
                    ai_snapshot['ResultValid']=device.get_boolean_feature_value('MaxAIResultValid')
                    assert ai_snapshot['ResultValid'] and ai_snapshot['ResultSequence']>0 and ai_snapshot['InferenceTimeUs']>0 and ai_snapshot['ErrorCode']==0,ai_snapshot
                finally:
                    device.execute_command('MaxAIResultRelease')
            results.append(dict(phase=phase,capture=record,ai=ai_snapshot))
            (args.output/'partial.json').write_text(json.dumps(results,indent=2)+'\n')
            assert record['passed'] and record['counters']['DroppedFrames']==0,record
        assert len(http_records)>=4,'Insufficient HTTP activity during capture'
        result=dict(passed=True,ai_mask=args.ai_mask,scope='RGB640x480 bounded trial; not maximum-mode/endurance qualification',
                    captures=results,http=http_records)
        (args.output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result),flush=True)
    finally:
        try:
            device.set_integer_feature_value('MaxAIEnabledFeatures',0)
            if original is not None:
                full_restore(cam,original)
                device.set_string_feature_value('UserSetSelector',selector)
                (args.output/'runtime-restored.json').write_text(json.dumps(full_snapshot(cam),indent=2)+'\n')
        finally:
            device=None;cam=None;gc.collect();Aravis.shutdown()

if __name__=='__main__':
    main()
