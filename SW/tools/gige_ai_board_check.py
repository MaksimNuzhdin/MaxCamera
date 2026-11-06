#!/usr/bin/env python3
"""Bounded volatile AI/GVSP check; no UserSet save, Flash or identity changes."""
import argparse
import gc
import json
import time
from pathlib import Path
from gige_probe import Control
from gige_feature_catalog import FEATURES
from stream_led_camera_common import Aravis, idle
import stream_led_camera_common as common
from gige_profile_backup import full_snapshot, full_restore
from gige_udp_capture import capture

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--ip',required=True)
parser.add_argument('--host',default='10.0.0.7')
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--seconds',type=int,default=8)
parser.add_argument('--masks',default='0,1,4,255')
args=parser.parse_args();masks=[int(value,0) for value in args.masks.split(',')]
assert masks and len(masks)<=8 and all(0<=value<=255 for value in masks)
args.output.mkdir(parents=True,exist_ok=True)
registers={f['key']:f['address'] for f in FEATURES}
wire=Control(args.ip,args.host)
assert wire.read(0xa00)==0 and wire.read(0x10018)==0,'Camera is already owned or acquiring'
wire.sock.close()
common.Control=lambda _ip,_host: Control(args.ip,args.host)
cam=Aravis.Camera.new(args.ip);device=cam.get_device()
results=[];original=None
try:
    idle(device);original=full_snapshot(cam)
    original_selector=device.get_string_feature_value('UserSetSelector')
    (args.output/'runtime-before.json').write_text(json.dumps(original,indent=2)+'\n')
    device.set_string_feature_value('UserSetSelector','UserSet7')
    device.execute_command('UserSetLoad');idle(device)
    assert device.get_integer_feature_value('UserSetLastError')==0
    assert cam.get_pixel_format_as_string()=='RGB8' and cam.get_region()[2:]==(640,480)
    device.write_register(0xd08,20)
    assert device.get_boolean_feature_value('MaxAIAvailable'),'Linux worker is unavailable'
    supported=device.get_integer_feature_value('MaxAISupportedFeatures')
    assert all(mask & ~supported == 0 for mask in masks),supported
    device.set_integer_feature_value('MaxAISamplePeriodUs',500000)
    for mask in masks:
        device.set_integer_feature_value('MaxAIEnabledFeatures',mask)
        snapshots=[];seen=set()
        def inspect():
            until=time.monotonic()+args.seconds-1
            while time.monotonic()<until:
                device.execute_command('MaxAIResultLatch')
                if device.get_boolean_feature_value('MaxAIResultValid'):
                    seq=device.get_integer_feature_value('MaxAIResultSequence')
                    if seq not in seen:
                        seen.add(seq)
                        count=device.get_integer_feature_value('MaxAIObjectCount')
                        record=dict(sequence=seq,count=count,capture=device.get_integer_feature_value('MaxAICaptureSequence'),
                                    age_us=device.get_integer_feature_value('MaxAIResultAgeUs'),
                                    width=device.get_integer_feature_value('MaxAIResultWidth'),
                                    height=device.get_integer_feature_value('MaxAIResultHeight'),objects=[])
                        assert count<=32 and (record['width'],record['height'])==(640,480)
                        for index in range(count):
                            device.set_integer_feature_value('MaxAIObjectSelector',index)
                            obj={name:device.get_integer_feature_value('MaxAIObject'+name) for name in ('Feature','Class','Score','X','Y','Width','Height','TrackID')}
                            assert obj['X']+obj['Width']<=640 and obj['Y']+obj['Height']<=480
                            assert obj['Score']<=1000000
                            record['objects'].append(obj)
                        snapshots.append(record)
                device.execute_command('MaxAIResultRelease')
                time.sleep(.1)
        measured=capture(cam,args.ip,args.output/f'mask-{mask}.bin',args.seconds,on_started=inspect)
        if mask and not snapshots:
            failure=dict(mask=mask,capture=measured,available=device.get_boolean_feature_value('MaxAIAvailable'),error=device.get_integer_feature_value('MaxAIErrorCode'),inference_us=device.get_integer_feature_value('MaxAIInferenceTimeUs'))
            (args.output/'failure.json').write_text(json.dumps(failure,indent=2)+'\n')
            raise AssertionError('No coherent AI result was returned: '+json.dumps(failure))
        assert measured['passed'] and measured['counters']['DroppedFrames']==0,measured
        results.append(dict(mask=mask,capture=measured,ai_results=snapshots,
                            inference_us=device.get_integer_feature_value('MaxAIInferenceTimeUs'),
                            error=device.get_integer_feature_value('MaxAIErrorCode')))
        print(json.dumps(results[-1]),flush=True)
    (args.output/'result.json').write_text(json.dumps(dict(passed=True,cases=results),indent=2)+'\n')
finally:
    try:
        device.set_integer_feature_value('MaxAIEnabledFeatures',0)
        if original is not None:
            restored=full_restore(cam,original)
            device.set_string_feature_value('UserSetSelector',original_selector)
            (args.output/'runtime-restored.json').write_text(json.dumps(restored,indent=2)+'\n')
    finally:
        device=None;cam=None;gc.collect();Aravis.shutdown()
