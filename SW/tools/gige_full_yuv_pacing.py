#!/usr/bin/env python3
"""Measure full-array YUV pacing without changing persistent presets."""
import argparse
import gc
import hashlib
import json
from pathlib import Path
import subprocess
import time
from gige_probe import Control
from gige_profile_backup import full_snapshot,full_restore
from gige_reliability_common import native,save
from gige_udp_capture import capture
import stream_led_camera_common as common
from stream_led_camera_common import Aravis,idle


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ip',default='10.0.0.10');p.add_argument('--host',default='10.0.0.7')
    p.add_argument('--output',type=Path,required=True);p.add_argument('--seconds',type=int,default=10)
    p.add_argument('--delays',type=int,nargs='+',default=[1,5,10,15,20]);a=p.parse_args()
    assert 5<=a.seconds<=60 and all(0<=n<=1000 for n in a.delays)
    a.output.mkdir(parents=True,exist_ok=False);source=a.output/'source';source.mkdir()
    root=Path(__file__).resolve().parents[1]
    for name in ['gige_full_yuv_pacing.py','qa_cycle_receiver.c','gige_reliability_common.py','gige_udp_capture.py']:
        (source/name).write_bytes((root/'tools'/name).read_bytes())
    exe=source/'receiver';subprocess.run(['cc','-O2','-std=c11','-Wall','-Wextra','-Werror',str(source/'qa_cycle_receiver.c'),'-o',str(exe)],check=True)
    common.Control=lambda _ip,_host:Control(a.ip,a.host)
    cam=Aravis.Camera.new(a.ip);d=cam.get_device();original=None
    result={'status':'RUNNING','scope':'Full2592x1944 YUV test bars; transport/FPS only, no optical acceptance','results':[]}
    try:
        idle(d);original=full_snapshot(cam);save(a.output/'runtime-before.json',original)
        assert d.get_integer_feature_value('MaxAIEnabledFeatures')==0
        result['firmware']=d.get_string_feature_value('DeviceVersion');native(cam,'YUV422_8',2592,1944,bars=True)
        for delay in a.delays:
            d.write_register(0xd08,delay);folder=a.output/f'delay{delay}';folder.mkdir();telemetry=[]
            def monitor():
                deadline=time.monotonic()+a.seconds
                while time.monotonic()<deadline:
                    value={'temperature_c':d.get_float_feature_value('DeviceTemperature')};telemetry.append(value)
                    assert -20<value['temperature_c']<85,'Temperature guard: '+json.dumps(value)
                    time.sleep(min(1,max(0,deadline-time.monotonic())))
            measured=capture(cam,a.ip,folder/'last.bin',a.seconds,on_started=monitor,trace=folder/'frames.csv',receiver=exe,packet_trace=folder/'failure-window.csv')
            nominal=measured['resulting_fps']
            passed=measured['passed'] and measured['socket_meminfo_available'] and measured['socket_drops']==0 and measured['counters']['DroppedFrames']==0 and abs(measured['delivered_fps']/nominal-1)<.03
            row={'delay_us':delay,'passed':passed,'capture':measured};result['results'].append(row)
            save(folder/'telemetry.json',telemetry);save(a.output/'results.json',result)
            print(json.dumps({'delay_us':delay,'passed':passed,'good':measured['good'],'bad':measured['bad'],'socket_drops':measured['socket_drops'],'fps':measured['delivered_fps']}),flush=True)
        result['status']='PASS' if all(r['passed'] for r in result['results']) else 'MIXED'
    except BaseException as error:result.update(status='ERROR',error=repr(error));raise
    finally:
        try:
            if original is not None:full_restore(cam,original);save(a.output/'runtime-restored.json',full_snapshot(cam));result['restored']=True
        finally:
            save(a.output/'results.json',result);d=None;cam=None;gc.collect();Aravis.shutdown()
    return 0 if any(r['passed'] for r in result['results']) else 1


if __name__=='__main__':raise SystemExit(main())
