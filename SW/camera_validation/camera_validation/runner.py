"""Ordered serial runner. Listing and planning never open a network socket."""
import argparse
from collections import Counter
from datetime import datetime, timezone
from fnmatch import fnmatchcase
import hashlib
import ipaddress
import json
from pathlib import Path
import time
import traceback
from .cases import cases, Context, Skip, CleanupError, FEATURES
from .transport import Control
from .report import render, write_manifest
from .functional import functional_cases


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('mode',choices=['list','plan','run','report'])
    p.add_argument('--ip',help='Explicit camera IPv4 address; no broadcast probing')
    p.add_argument('--bind',default='0.0.0.0')
    p.add_argument('--output',type=Path,default=Path('reports/latest'))
    p.add_argument('--suite',choices=['functional','diagnostic','all'],default='functional')
    p.add_argument('--response-source',choices=['scene','pattern'],default='scene',help='Input for gain/exposure/WB response; no silent source fallback')
    p.add_argument('--fixture',type=Path,help='JSON fixture and independently approved expected-image profiles')
    p.add_argument('--test-slot',type=int,choices=range(1,8),help='Existing reserved Flash slot; backed up and restored')
    p.add_argument('--flash-timeout',type=float,default=30)
    p.add_argument('--reset-timeout',type=float,default=60)
    p.add_argument('--settle-frames',type=int,default=3)
    p.add_argument('--image-mae',type=float,default=8)
    p.add_argument('--pixel-error',type=float,default=24)
    p.add_argument('--bad-fraction',type=float,default=.02)
    p.add_argument('--bar-tolerance',type=float,default=50)
    p.add_argument('--select',action='append',default=[],help='Case ID glob; repeatable')
    p.add_argument('--order',type=Path,help='JSON array of exact case IDs; controls order')
    p.add_argument('--timeout',type=float,default=3)
    p.add_argument('--frame-timeout',type=float,default=15)
    p.add_argument('--allow-reorder',action='store_true',help='Reassemble reordered payload/trailer packets within one frame; loss and duplicates still fail')
    p.add_argument('--buffer-packets',action='store_true',help='Drain UDP concurrently into a bounded16MiB queue while processing images')
    p.add_argument('--c-receiver',type=Path,help='Optional compiled QA C receiver for image/JPEG acquisition in a separate process')
    p.add_argument('--frames',type=int,default=3)
    p.add_argument('--packet-delay',type=int,default=100)
    p.add_argument('--black-max',type=float,default=32)
    p.add_argument('--input',type=Path,help='Existing results JSON for report mode')
    p.add_argument('--fail-fast',action='store_true')
    args=p.parse_args(argv)
    if args.mode=='report':
        if not args.input:p.error('--input is required')
        render(json.loads(args.input.read_text()),args.output);write_manifest(args.output);return 0
    if args.frames<2 or args.timeout<=0 or args.frame_timeout<=0 or args.flash_timeout<=0 or args.reset_timeout<=0:
        p.error('Require frames >= 2 and positive timeouts')
    if args.settle_frames<0 or args.image_mae<0 or args.pixel_error<0 or not 0<=args.bad_fraction<=1:
        p.error('Invalid image oracle limits or settling frame count')
    inventory=(functional_cases() if args.suite=='functional' else cases() if args.suite=='diagnostic' else functional_cases()+cases())
    selected=[c for c in inventory if not args.select or any(fnmatchcase(c.id,g) for g in args.select)]
    if args.order:
        ids=json.loads(args.order.read_text());lookup={c.id:c for c in selected}
        if len(ids)!=len(set(ids)) or any(i not in lookup for i in ids):p.error('Order contains duplicate or unknown case IDs')
        selected=[lookup[i] for i in ids]
    if not selected:p.error('Selection is empty')
    if args.mode=='list':
        for c in selected:print(f'{c.id:52} {c.level:16} {c.purpose}')
        return 0
    if args.mode=='run':
        if (args.output/'results.json').exists():p.error('Output already contains results; choose a new run directory')
        if not args.ip:p.error('--ip is required for hardware execution')
        try:
            address=ipaddress.IPv4Address(args.ip)
        except ipaddress.AddressValueError:
            p.error('--ip must be a literal IPv4 address')
        if address.is_multicast or address.is_unspecified or int(address)==0xffffffff:
            p.error('--ip must be a unicast camera address')
    args.output.mkdir(parents=True,exist_ok=True)
    document={'schema_version':1,'mode':'PLAN - NO HARDWARE EXECUTION' if args.mode=='plan' else 'HARDWARE EXECUTION',
              'created':datetime.now(timezone.utc).isoformat(),'parameters':{k:str(v) if isinstance(v,Path) else v for k,v in vars(args).items()},
              'catalog_sha256':hashlib.sha256(Path(__file__).with_name('features.json').read_bytes()).hexdigest(),
              'limitations':['Functional PASS requires expected-pixel, response or persistence assertions; register diagnostics are separate.','No certification claim.','Missing fixtures and reserved Flash slots are reported as SKIP.','DeviceReset is not an electrical power-cycle.'],
              'results':[dict(id=c.id,feature=c.feature,level=c.level,covers=list(c.covers or (c.feature,)),purpose=c.purpose,status='NOT_RUN',detail='Not executed',evidence={}) for c in selected]}
    declared={feature for case in functional_cases() for feature in (case.covers or (case.feature,))}
    document['features_without_functional_case']=[f['name'] for f in FEATURES if f['name'] not in declared]
    control=None;owned=False;fatal=False
    try:
        if args.mode=='run':
            control=Control(args.ip,args.bind,args.timeout)
            # Never Stop or steal ownership from an existing application.
            if control.read(0xa00)!=0 or control.read(0x10018)!=0:
                raise RuntimeError('Camera is owned or acquiring; refusing to interfere')
            control.write(0xa00,2);owned=True
            if control.read(0xa00)!=2:raise RuntimeError('Failed to acquire control')
            ctx=Context(control,args)
            ctx.fixture=json.loads(args.fixture.read_text()) if args.fixture else {}
            ctx.fixture_root=args.fixture.resolve().parent if args.fixture else Path.cwd()
            ctx.idle()
            for case,row in zip(selected,document['results']):
                start=time.monotonic();trace_start=len(control.trace)
                ctx.case_output=args.output/case.id
                ctx.case_output.mkdir(parents=True,exist_ok=True)
                ctx.evidence={}
                try:
                    row['evidence']=case.action(ctx) or {};row['status']='PASS';row['detail']='Assertions satisfied'
                except Skip as exc:
                    row['status']='SKIP';row['detail']=str(exc)
                except Exception as exc:
                    row['status']='ERROR' if isinstance(exc,CleanupError) else 'FAIL'
                    row['detail']=f'{type(exc).__name__}: {exc}'
                    row['traceback']=traceback.format_exc()
                    fatal=isinstance(exc,(CleanupError,TimeoutError))
                control=ctx.c
                if ctx.evidence:
                    row['evidence']=ctx.evidence
                row['artifact_directory']=case.id
                row['duration_seconds']=round(time.monotonic()-start,6)
                row['transactions']=control.trace[trace_start:]
                (args.output/'results.json').write_text(json.dumps(document,indent=2)+'\n')
                print(row['status'],case.id,flush=True)
                if fatal or (args.fail_fast and row['status'] in {'FAIL','ERROR'}):break
    except (Exception,KeyboardInterrupt) as exc:
        document['run_error']=f'{type(exc).__name__}: {exc}';fatal=True
    finally:
        if owned:
            if 'ctx' in locals():control=ctx.c
            try:
                control.write(0x10014,0)
                control.write(0xa00,0)
            except Exception as exc:
                document['cleanup_error']=str(exc);fatal=True
        if control:control.close()
        (args.output/'results.json').write_text(json.dumps(document,indent=2)+'\n')
        render(document,args.output)
        write_manifest(args.output)
    counts=Counter(r['status'] for r in document['results'])
    print(json.dumps(dict(counts)))
    # A partial run is not full qualification.
    return 1 if fatal or counts['FAIL'] or counts['ERROR'] else (2 if counts['SKIP'] or (args.mode=='run' and counts['NOT_RUN']) else 0)


if __name__=='__main__':
    raise SystemExit(main())
