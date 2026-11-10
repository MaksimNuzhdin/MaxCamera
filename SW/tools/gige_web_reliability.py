#!/usr/bin/env python3
"""Compare paced video/AI/HTTP with strict packet and frame-continuity evidence.

Volatile camera controls only. No UserSet saves, Flash writes or physical reset.
"""
import argparse
from collections import Counter
import csv
import gc
import hashlib
from itertools import groupby
import json
from pathlib import Path
import signal
import subprocess
import time
import threading
from gige_http_concurrency import load as concurrent_http_load
from urllib.error import HTTPError
from urllib.request import ProxyHandler, build_opener

from gige_reliability_common import counters, fpga, host_network_counters, save
from gige_probe import Control
from gige_profile_backup import full_restore, full_snapshot
from gige_udp_capture import capture
import stream_led_camera_common as common
from stream_led_camera_common import Aravis, idle, write


def trace_summary(path):
    with path.open() as source:
        rows=list(csv.DictReader(source))
    gaps=[];errors=[]
    for previous,current in zip(rows,rows[1:]):
        a,b=int(previous['block_id']),int(current['block_id'])
        delta=(b-a)%65535
        if delta!=1:gaps.append(dict(previous=a,current=b,delta=delta))
        if int(current['timestamp_ticks'])<=int(previous['timestamp_ticks']):
            errors.append(dict(previous=a,current=b))
    hashes=[r['fnv64'] for r in rows]
    return dict(frames=len(rows),block_discontinuities=gaps,timestamp_errors=errors,
                distinct_hashes=len(set(hashes)),longest_identical_hash_run=max((sum(1 for _ in group) for _,group in groupby(hashes)),default=0),first_block=int(rows[0]['block_id']) if rows else None,
                last_block=int(rows[-1]['block_id']) if rows else None)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--ip',default='10.0.0.10')
    p.add_argument('--host',default='10.0.0.7')
    p.add_argument('--output',required=True,type=Path)
    p.add_argument('--seconds',type=int,default=120)
    p.add_argument('--delays',type=int,nargs='+',default=[20,100])
    p.add_argument('--http-clients',type=int,choices=(1,2,4),default=1)
    p.add_argument('--ai-masks',type=int,nargs='+',default=[0,1])
    a=p.parse_args()
    assert 10<=a.seconds<=3600 and all(0<=n<=1000 for n in a.delays)
    assert all(0<=n<=255 for n in a.ai_masks)
    a.output.mkdir(parents=True,exist_ok=False)
    src=a.output/'source';src.mkdir()
    sources=['gige_web_reliability.py','qa_cycle_receiver.c','gige_udp_capture.py',
             'gige_profile_backup.py','gige_reliability_common.py','stream_led_camera_common.py','gige_http_concurrency.py','configure_fpga_scaler.py']
    hashes={}
    for name in sources:
        data=Path(__file__).with_name(name).read_bytes();(src/name).write_bytes(data)
        hashes[name]=hashlib.sha256(data).hexdigest()
    receiver=src/'receiver'
    subprocess.run(['cc','-O2','-std=c11','-Wall','-Wextra','-Werror',str(src/'qa_cycle_receiver.c'),'-o',str(receiver)],check=True)
    http=build_opener(ProxyHandler({}))
    def get(path):
        with http.open('http://'+a.ip+path,timeout=3) as response:return response.read()
    status=json.loads(get('/api/status'))
    assert not status['owned'] and not status['streaming'] and status['config']['mask']==0
    assert all(n & ~status['supported']==0 for n in a.ai_masks),'Requested unsupported AI bits'
    common.Control=lambda _ip,_host:Control(a.ip,a.host)
    cam=Aravis.Camera.new(a.ip);d=cam.get_device();original=None
    document=dict(status='RUNNING',parameters=vars(a)|{'output':str(a.output)},source_sha256=hashes,
                  scope='Independent receiver without resend; moving test bars, no optical/AI accuracy claim',results=[])
    def interrupted(*_):raise KeyboardInterrupt('Requested stop; restoring camera')
    signal.signal(signal.SIGTERM,interrupted);signal.signal(signal.SIGINT,interrupted)
    try:
        idle(d);original=full_snapshot(cam);save(a.output/'runtime-before.json',original)
        document['firmware']=d.get_string_feature_value('DeviceVersion')
        document['fpga_version']=d.get_integer_feature_value('FpgaProcessingVersion')
        fpga(cam);write(d,'TEST_ROLL',1)
        for delay in a.delays:
            d.write_register(0xd08,delay)
            for mask in a.ai_masks:
                d.set_integer_feature_value('MaxAIEnabledFeatures',mask)
                for phase in ('baseline','http'):
                    name=f'delay{delay}-ai{mask}-{phase}';folder=a.output/name;folder.mkdir()
                    row=dict(name=name,delay_us=delay,ai_mask=mask,phase=phase,status='RUNNING')
                    document['results'].append(row);save(a.output/'results.json',document)
                    if mask:
                        d.execute_command('MaxAIResultLatch')
                        try:row['initial_ai_sequence']=d.get_integer_feature_value('MaxAIResultSequence')
                        finally:d.execute_command('MaxAIResultRelease')
                    before=counters(d);host_before=host_network_counters();telemetry=[];requests=[]
                    def monitor():
                        deadline=time.monotonic()+a.seconds;next_sample=0;index=0;http_thread=None;http_stop=threading.Event()
                        if phase=='http' and a.http_clients>1:
                            def concurrent_load():
                                summary,records=concurrent_http_load(a.ip,a.seconds-3,a.http_clients,stop=http_stop)
                                row['http_concurrency']=summary;requests.extend(records)
                            http_thread=threading.Thread(target=concurrent_load);http_thread.start()
                        try:
                            while time.monotonic()<deadline:
                                now=time.monotonic()
                                if now>=next_sample:
                                    sample=dict(elapsed_s=a.seconds-(deadline-now),camera=counters(d),
                                                temperature_c=d.get_float_feature_value('DeviceTemperature'))
                                    if mask:sample['inference_us']=d.get_integer_feature_value('MaxAIInferenceTimeUs')
                                    telemetry.append(sample);save(folder/'progress.json',sample)
                                    assert -20<sample['temperature_c']<85,'Temperature guard: '+json.dumps(sample)
                                    for key in ['CSI_ERRORS','RECOVERIES','DROPPED','GEM_ERRORS','RX_DROPS']:
                                        assert sample['camera'][key]==before[key],'Camera fault during capture '+key+': '+json.dumps(sample)
                                    next_sample=now+5
                                if phase=='http' and a.http_clients==1 and deadline-now>3:
                                    path=('/api/status','/','/style.css','/app.js')[index%4];index+=1
                                    started=time.monotonic()
                                    try:
                                        data=get(path);code=200;size=len(data)
                                    except HTTPError as e:code=e.code;size=len(e.read())
                                    except (OSError,TimeoutError) as e:code=type(e).__name__;size=0
                                    requests.append(dict(path=path,status=code,bytes=size,milliseconds=(time.monotonic()-started)*1000))
                                else:time.sleep(min(.1,max(0,deadline-time.monotonic())))
                        finally:
                            http_stop.set()
                            if http_thread:
                                http_thread.join(timeout=8)
                                assert not http_thread.is_alive() and 'http_concurrency' in row,'Concurrent load did not finish'
                    try:
                        result=capture(cam,a.ip,folder/'last.bin',a.seconds,on_started=monitor,
                                       trace=folder/'frames.csv',receiver=receiver,packet_trace=folder/'failure-window.csv')
                        row['capture']=result;row['trace']=trace_summary(folder/'frames.csv')
                        after=counters(d);row['camera_delta']={k:after[k]-before[k] for k in before}
                        row['http_statuses']=dict(Counter(str(q['status']) for q in requests))
                        row['http_bytes']=sum(q['bytes'] for q in requests)
                        if mask:
                            d.execute_command('MaxAIResultLatch')
                            try:
                                row['ai']=dict(inference_us=d.get_integer_feature_value('MaxAIInferenceTimeUs'),error=d.get_integer_feature_value('MaxAIErrorCode'),sequence=d.get_integer_feature_value('MaxAIResultSequence'),valid=d.get_boolean_feature_value('MaxAIResultValid'),config_generation=d.get_integer_feature_value('MaxAIConfigGeneration'),result_config_generation=d.get_integer_feature_value('MaxAIResultConfigGeneration'))
                            finally:d.execute_command('MaxAIResultRelease')
                        ok=result['passed'] and result.get('socket_meminfo_available') and not result.get('socket_drops')
                        ok=ok and not row['trace']['block_discontinuities'] and not row['trace']['timestamp_errors']
                        ok=ok and row['trace']['distinct_hashes']>1 and row['trace']['longest_identical_hash_run']<30 and result['good']>=a.seconds*13
                        ok=ok and all(row['camera_delta'][k]==0 for k in ['CSI_ERRORS','RECOVERIES','DROPPED','GEM_ERRORS','RX_DROPS'])
                        if mask:ok=ok and row['ai']['valid'] and row['ai']['error']==0 and row['ai']['sequence']>0 and row['ai']['sequence']!=row['initial_ai_sequence'] and row['ai']['inference_us']>0 and row['ai']['config_generation']==row['ai']['result_config_generation']
                        if phase=='http':ok=ok and len(requests)>=4 and row['http_statuses']=={'200':len(requests)}
                        row['status']='PASS' if ok else 'FAIL'
                    except Exception as e:
                        row.update(status='ERROR',error=repr(e));raise
                    finally:
                        save(folder/'telemetry.json',telemetry);save(folder/'http.json',requests)
                        host_after=host_network_counters();row['host_delta']={k:host_after[k]-v for k,v in host_before.items() if k in host_after}
                        save(a.output/'results.json',document)
                        print(json.dumps({k:row[k] for k in ['name','status']}|{'good':row.get('capture',{}).get('good'),'http':row.get('http_statuses')}),flush=True)
        document['status']='PASS' if all(r['status']=='PASS' for r in document['results']) else 'FAIL'
    except BaseException as error:
        document['status']='INTERRUPTED' if isinstance(error,KeyboardInterrupt) else 'ERROR'
        document['error']=repr(error)
        for row in document['results']:
            if row['status']=='RUNNING':row['status']=document['status']
        raise
    finally:
        try:
            d.set_integer_feature_value('MaxAIEnabledFeatures',0)
            if original is not None:
                full_restore(cam,original);save(a.output/'runtime-restored.json',full_snapshot(cam));document['restored']=True
        except BaseException as error:
            document.update(status='RESTORATION_FAILED',restored=False,restoration_error=repr(error));raise
        finally:
            save(a.output/'results.json',document);d=None;cam=None;gc.collect();Aravis.shutdown()
    return 0 if document['status']=='PASS' else 1


if __name__=='__main__':raise SystemExit(main())
