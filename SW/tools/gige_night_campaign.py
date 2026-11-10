#!/usr/bin/env python3
"""Run a bounded unattended camera campaign with frozen sources and cleanup checks.

No persistent camera writes, electrical power cycles or automatic JTAG recovery.
"""
import argparse
from datetime import datetime,timezone
import fcntl
import hashlib
import html
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import time
from urllib.request import ProxyHandler,build_opener

ROOT=Path(__file__).resolve().parents[1]


def utc():return datetime.now(timezone.utc).isoformat()


def save(path,data):
    tmp=path.with_suffix('.tmp');tmp.write_text(json.dumps(data,indent=2)+'\n');tmp.replace(path)


def read(path):
    try:return json.loads(path.read_text())
    except (OSError,ValueError):return None


def render(out,state):
    rows=[]
    for row in state['steps']:
        rows.append('<tr>'+''.join('<td>'+html.escape(str(row.get(k,'')))+'</td>' for k in ['name','status','started_utc','finished_utc','returncode'])+'</tr>')
    progress=html.escape(json.dumps(state.get('progress',{}),indent=2))
    summaries=html.escape(json.dumps({r['name']:r.get('summary',{}) for r in state['steps'] if 'summary' in r},indent=2))
    text='''<!doctype html><html lang="en"><meta charset="utf-8"><meta http-equiv="refresh" content="30">
<title>MaxCamera overnight reliability</title><style>body{font:16px system-ui;max-width:1200px;margin:40px auto;padding:0 20px;background:#101923;color:#e4edf3}table{border-collapse:collapse;width:100%}td,th{padding:12px;border-bottom:1px solid #345;text-align:left}pre{white-space:pre-wrap;color:#8bdecb}a{color:#8bdecb}</style>
<h1>MaxCamera overnight reliability</h1><p>'''+html.escape(state['status'])+' | Updated '+html.escape(utc())+'''</p><p>Volatile tests only. Failed and unfinished attempts remain visible. No physical SD boot, optical accuracy or certification claim.</p>
<table><tr><th>Step</th><th>Status</th><th>Started UTC</th><th>Finished UTC</th><th>Exit</th></tr>'''+''.join(rows)+'</table><h2>Current progress</h2><pre>'+progress+'</pre><h2>Completed evidence</h2><pre>'+summaries+'</pre><p><a href="campaign.json">Machine-readable campaign</a></p></html>'
    temp=out/'index.tmp';temp.write_text(text);temp.replace(out/'index.html')


def freeze(out):
    tree=out/'source';(tree/'tools').mkdir(parents=True);(tree/'gige').mkdir()
    names=['gige_night_campaign.py','gige_web_reliability.py','gige_resend_reliability.py','gige_http_concurrency.py','gige_reliability_common.py','configure_fpga_scaler.py','gige_profile_backup.py','gige_probe.py','gige_feature_catalog.py','gige_udp_capture.py','gige_udp_receiver.c','qa_cycle_receiver.c','stream_led_camera_common.py','camera_qa_cycle.py','qa_image_checks.py']
    for name in names:shutil.copy2(ROOT/'tools'/name,tree/'tools'/name)
    shutil.copy2(ROOT/'gige/GigePresetFeatures.inc',tree/'gige/GigePresetFeatures.inc')
    shutil.copytree(ROOT/'camera_validation/camera_validation',tree/'camera_validation/camera_validation',ignore=shutil.ignore_patterns('__pycache__'))
    hashes={str(p.relative_to(tree)):hashlib.sha256(p.read_bytes()).hexdigest() for p in tree.rglob('*') if p.is_file()}
    save(out/'source-sha256.json',hashes);return tree


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--ip',default='10.0.0.10');p.add_argument('--host',default='10.0.0.7')
    p.add_argument('--hours',type=float,default=8);p.add_argument('--wait-for',type=Path)
    p.add_argument('--expected-firmware',default='0.3.19-tcpheadroom-dev');a=p.parse_args()
    assert 1<=a.hours<=10
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    lock=(out/'campaign.lock').open('a');fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    assert not (out/'campaign.json').exists(),'Refusing to overwrite an existing campaign'
    tree=freeze(out);deadline=time.monotonic()+a.hours*3600;stop=False;child=None
    def request_stop(*_):
        nonlocal stop;stop=True
    signal.signal(signal.SIGTERM,request_stop);signal.signal(signal.SIGINT,request_stop)
    signal.signal(signal.SIGHUP,signal.SIG_IGN)
    env=os.environ.copy();env['GI_TYPELIB_PATH']=str(ROOT/'build/aravis/usr/lib/x86_64-linux-gnu/girepository-1.0')
    env['LD_LIBRARY_PATH']=str(ROOT/'build/aravis/usr/lib/x86_64-linux-gnu')
    # Pin only native receiver children; HTTP/GenApi keep their usual affinity.
    allowed=sorted(os.sched_getaffinity(0))
    def cpu_sample():
        result={}
        for line in Path('/proc/stat').read_text().splitlines():
            words=line.split()
            if words and words[0][3:].isdigit():
                v=list(map(int,words[1:]));result[int(words[0][3:])]=(sum(v[:8]),v[3]+v[4])
        return result
    before=cpu_sample();time.sleep(.25);after=cpu_sample()
    cpu=max(allowed,key=lambda n:(after[n][1]-before[n][1])/max(1,after[n][0]-before[n][0]))
    env['MAXCAMERA_RECEIVER_CPU']=str(cpu)
    state=dict(status='WAITING' if a.wait_for else 'RUNNING',pid=os.getpid(),started_utc=utc(),budget_hours=a.hours,
               expected_firmware=a.expected_firmware,source='Frozen source tree; no automatic firmware replacement',steps=[])
    plans=[('resend-http','gige_resend_reliability.py',['--frames','100'],90),
           ('switch100','camera_qa_cycle.py',['--select','stress.mode_switch_restart','--stress-cycles','100'],900),
           ('full-yuv-paced','camera_qa_cycle.py',['--select','transport.full_yuv.paced1'],180),
           ('all-ai-smoke','gige_web_reliability.py',['--seconds','60','--delays','100','--ai-masks','255','--http-clients','4'],240),
           ('hour-face100','gige_web_reliability.py',['--seconds','3600','--delays','100','--ai-masks','1','--http-clients','4'],7380),
           ('hour-face20','gige_web_reliability.py',['--seconds','3600','--delays','20','--ai-masks','1','--http-clients','4'],7380),
           ('hour-all100','gige_web_reliability.py',['--seconds','3600','--delays','100','--ai-masks','255','--http-clients','4'],7380),
           ('half-hour-face100-repeat','gige_web_reliability.py',['--seconds','1800','--delays','100','--ai-masks','1','--http-clients','4'],3780)]
    state['receiver_cpu']=cpu
    state['receiver_affinity_scope']='Pinned logical CPU; no exclusive CPU reservation or kernel buffer-limit change'
    state['steps']=[dict(name=name,status='NOT_RUN',maximum_seconds=duration) for name,_,_,duration in plans]
    artifacts=['build/tcpheadroom-r5/MaxCameraApp.elf','build/tcpheadroom-linux/BOOT.BIN','build/tcpheadroom-linux/rootfs.cpio.gz','diagnostics/fpga-ppc4-v13-20261102/logic-ppc4-v13.bit']
    state['expected_artifacts']={name:{'sha256':hashlib.sha256((ROOT/name).read_bytes()).hexdigest(),'bytes':(ROOT/name).stat().st_size} for name in artifacts if (ROOT/name).is_file()}
    state['artifact_scope']='Expected locally loaded build inputs; firmware/MAC are checked over the wire, not cryptographic attestation'
    def update():save(out/'campaign.json',state);render(out,state)
    def healthy():
        from gige_probe import Control
        wire=Control(a.ip,a.host)
        try:
            packet,_=wire.command(2,flags=1)
            assert packet[10:16]==bytes.fromhex('020000000001'),'Unexpected camera MAC'
            firmware=packet[136:168].split(b'\0')[0].decode('ascii')
            assert firmware==a.expected_firmware,('Unexpected firmware',firmware)
            assert wire.read(0xa00)==0 and wire.read(0x10018)==0,'Camera is owned or acquiring'
        finally:wire.sock.close()
        opener=build_opener(ProxyHandler({}))
        with opener.open('http://'+a.ip+'/api/status',timeout=10) as response:s=json.load(response)
        assert s['available'] and not s['owned'] and not s['streaming'] and s['config']['mask']==0,'Camera is not ready/idle with AI off'
        return {k:s[k] for k in ['available','owned','streaming','supported','error','storage_persistent']}
    def terminate():
        if child and child.poll() is None:
            os.killpg(child.pid,signal.SIGTERM)
            try:child.wait(timeout=45)
            except subprocess.TimeoutExpired:os.killpg(child.pid,signal.SIGKILL);child.wait();state['forced_kill']=True
    update()
    try:
        while a.wait_for:
            prior=read(a.wait_for)
            if prior and prior.get('status') not in ('RUNNING','WAITING'):
                assert prior.get('restored'),'Previous trial restoration unverified';break
            if stop or time.monotonic()>=deadline:raise InterruptedError('Stopped while waiting for prior trial')
            state['progress']={'waiting_for':str(a.wait_for)};update();time.sleep(10)
        state['initial_ready']=healthy();state['status']='RUNNING';update()
        for row,(name,script,options,maximum) in zip(state['steps'],plans):
            if stop or (out/'STOP').exists():state['status']='STOPPED';break
            if time.monotonic()+maximum+60>deadline:
                row['status']='SKIPPED_BUDGET';update();continue
            state['last_ready']=healthy();folder=out/name
            command=[sys.executable,str(tree/'tools'/script),'--ip',a.ip,'--host',a.host,'--output',str(folder),*options]
            row.update(status='RUNNING',started_utc=utc(),command=command);state['current_step']=name;update()
            with (out/(name+'.log')).open('w') as log:
                child=subprocess.Popen(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
                limit=min(deadline,time.monotonic()+maximum);row['pid']=child.pid
                while child.poll() is None:
                    if stop or (out/'STOP').exists() or time.monotonic()>=limit:
                        row['timed_out']=not stop;terminate();break
                    progress_files=list(folder.glob('*/progress.json'))+list(folder.glob('progress.json'))
                    latest=max(progress_files,key=lambda f:f.stat().st_mtime) if progress_files else None
                    state['progress']={'step':name,'file':str(latest) if latest else None,'sample':read(latest) if latest else None}
                    update();time.sleep(10)
                row.update(returncode=child.returncode,finished_utc=utc())
            result=read(folder/'results.json') or read(folder/'result.json') or {}
            row['result_status']=result.get('status',result.get('passed'))
            if script=='gige_web_reliability.py':
                row['summary']=[{'name':r.get('name'),'status':r.get('status'),'good_frames':r.get('capture',{}).get('good'),'bad_frames':r.get('capture',{}).get('bad'),'missing_packets':r.get('capture',{}).get('missing_packets'),'socket_drops':r.get('capture',{}).get('socket_drops'),'fps':r.get('capture',{}).get('delivered_fps'),'http_statuses':r.get('http_statuses'),'ai':r.get('ai')} for r in result.get('results',[])]
            elif script=='gige_resend_reliability.py':row['summary']={k:result.get(k) for k in ['passed','frames','intentional_losses','recovered','resend','http_statuses']}
            else:row['summary']={'counts':result.get('counts'),'cases':[{'id':r.get('id'),'status':r.get('status'),'error':r.get('error')} for r in result.get('results',[])]}
            row['status']='PASS' if child.returncode==0 else 'FAIL'
            restored=result.get('restored',all(r.get('restored') and r.get('owner_after_release')==0 for r in result.get('results',[])) if result.get('results') else False)
            row['restored']=restored;update()
            if not restored:state['status']='STOPPED_RESTORATION_UNVERIFIED';break
            if 'Temperature guard' in json.dumps(result):state['status']='STOPPED_TEMPERATURE_GUARD';break
            state['last_ready']=healthy();child=None
        else:state['status']='COMPLETED' if all(r['status']=='PASS' for r in state['steps']) else 'COMPLETED_WITH_GAPS'
        state['finished_utc']=utc();state['final_ready']=healthy();update()
    except BaseException as error:
        terminate();state.update(status='STOPPED_ERROR',error=repr(error),finished_utc=utc());update();raise
    finally:
        manifest={str(f.relative_to(out)):hashlib.sha256(f.read_bytes()).hexdigest() for f in out.rglob('*') if f.is_file() and f.name not in ['manifest-sha256.json','campaign.lock']}
        save(out/'manifest-sha256.json',manifest)
    return 0 if state['status']=='COMPLETED' else 1


if __name__=='__main__':raise SystemExit(main())
