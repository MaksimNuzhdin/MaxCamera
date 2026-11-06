#!/usr/bin/env python3
"""Compare quiescent static-pattern R5 DDR pixels read by Linux with GVSP bytes."""
import argparse
import gc
import json
import re
import shlex
import subprocess
import sys
import time
import zlib
from pathlib import Path
from gige_probe import Control
from gige_feature_catalog import FEATURES
from stream_led_camera_common import Aravis,idle
import stream_led_camera_common as common
from gige_profile_backup import full_snapshot,full_restore
from gige_udp_capture import capture

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--ip',required=True);p.add_argument('--host',default='10.0.0.7')
p.add_argument('--output',type=Path,required=True);p.add_argument('--port',default='/dev/ttyUSB0')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
reg={f['key']:f['address'] for f in FEATURES}
wire=Control(a.ip,a.host)
assert wire.read(0xa00)==0 and wire.read(0x10018)==0,'Camera is owned or acquiring'
assert wire.read(8)==0x200 and wire.read(12)==1,'Unexpected camera MAC'
wire.sock.close();common.Control=lambda _ip,_host:Control(a.ip,a.host)
cam=Aravis.Camera.new(a.ip);d=cam.get_device();original=None
try:
    idle(d);original=full_snapshot(cam);selector=d.get_string_feature_value('UserSetSelector')
    d.set_string_feature_value('UserSetSelector','UserSet7');d.execute_command('UserSetLoad');idle(d)
    assert cam.get_region()[2:]==(640,480) and cam.get_pixel_format_as_string()=='RGB8'
    for key,value in [('FPGA_AWB',0),('EXPOSURE_AUTO',0),('GAIN_AUTO',0),('TEST_PATTERN',1)]:d.write_register(reg[key],value)
    d.write_register(0xd08,20);d.set_integer_feature_value('MaxAIEnabledFeatures',16)
    measured=capture(cam,a.ip,a.output/'static-rgb.bin',4)
    assert measured['passed'] and measured['counters']['DroppedFrames']==0,measured
    until=time.monotonic()+5
    while d.get_integer_feature_value('MaxAIWorkerState')==3:
        assert time.monotonic()<until;time.sleep(.02)
    assert d.read_register(0x10018)==0 and not d.get_boolean_feature_value('SensorBusy')
    # Acquisition is stopped: no DMA writer can mutate this diagnostic read.
    # The source may be a different capture of the same fixed color-bar pattern.
    code="import os,sys,json,zlib;sys.path.insert(0,'/opt/maxcamera/ai');from worker import Mailbox;fd=os.open('/dev/mem',os.O_RDWR|os.O_SYNC);m=Mailbox(fd);d=m.read(32,10);b=m.copy_frame(d);print('AI_COPY_RESULT '+json.dumps(dict(sequence=d[0],width=d[3],height=d[4],stride=d[5],pfnc=d[6],bytes=len(b),crc32=zlib.crc32(b)&0xffffffff)))"
    command='python3 -c '+shlex.quote(code)
    log=a.output/'linux-copy-console.log'
    subprocess.run([sys.executable,str(Path(__file__).with_name('ai_linux_console.py')),'--port',a.port,'--command',command,'--log',str(log),'--seconds','4'],check=True,capture_output=True)
    matches=re.findall(r'AI_COPY_RESULT (\{[^\r\n]*\})',log.read_text())
    assert matches,'Linux CRC result was not received'
    actual=json.loads(matches[-1]);expected=zlib.crc32((a.output/'static-rgb.bin').read_bytes())&0xffffffff
    assert actual['bytes']==921600 and actual['stride']==1920 and actual['crc32']==expected,(actual,expected)
    report=dict(passed=True,scope='Static-pattern pixel visibility after acquisition stopped; not simultaneous frame-identity qualification',host_crc32=expected,linux=actual,capture=measured)
    (a.output/'result.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
finally:
    try:
        d.set_integer_feature_value('MaxAIEnabledFeatures',0)
        if original is not None:
            full_restore(cam,original);d.set_string_feature_value('UserSetSelector',selector)
    finally:d=None;cam=None;gc.collect();Aravis.shutdown()
