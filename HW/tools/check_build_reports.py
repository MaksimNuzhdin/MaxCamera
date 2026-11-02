#!/usr/bin/env python3
"""Fail closed on routed timing, resource, DRC, routing and bus-skew reports."""
import argparse,json,re
from pathlib import Path

def check(directory):
    timing=(directory/'timing.rpt').read_text()
    tail=timing.split('Design Timing Summary',1)[1]
    values=None
    for line in tail.splitlines():
        items=line.split()
        if len(items)==12 and re.fullmatch(r'-?\d+\.\d+',items[0]):
            values=[float(v) for v in items];break
    assert values is not None,'No complete design timing row'
    for i in (0,4,8):assert values[i]>=0,('Negative setup/hold/pulse slack',values)
    for i in (1,2,5,6,9,10):assert values[i]==0,('Failing timing endpoints',values)
    for name in ('no_clock','unconstrained_internal_endpoints'):
        counts=re.findall(r'checking '+name+r' \((\d+)\)',timing)
        assert counts and all(int(v)==0 for v in counts),name
    utilization=(directory/'utilization.rpt').read_text();resources={}
    for label,key in [('CLB LUTs','lut'),('CLB Registers','ff'),('Block RAM Tile','bram36'),('DSPs','dsp')]:
        line=next(v for v in utilization.splitlines() if re.match(r'\|\s*'+re.escape(label)+r'\*?\s*\|',v))
        fields=[x.strip() for x in line.strip().strip('|').split('|')]
        used,available=float(fields[1]),float(fields[4]);assert 0<=used<=available
        resources[key]=dict(used=used,available=available,remaining=available-used)
    route=(directory/'route-status.rpt').read_text()
    def count(name):
        m=re.search(re.escape(name)+r'\.+\s*:\s*(\d+)',route);assert m,name;return int(m[1])
    total,done,errors=count('# of routable nets'),count('# of fully routed nets'),count('# of nets with routing errors')
    assert total==done and total>0 and errors==0
    drc=(directory/'drc.rpt').read_text()
    assert not re.search(r'\|\s*(?:Error|Critical Warning)\s*\|',drc),'DRC error or critical warning'
    skew=(directory/'bus-skew.rpt').read_text();assert 'VIOLATED' not in skew
    slacks=[float(v) for v in re.findall(r'Slack \(MET\)\s*:\s*([-\d.]+)ns',skew)]
    assert slacks and all(v>=0 for v in slacks)
    return dict(passed=True,wns_ns=values[0],whs_ns=values[4],wpws_ns=values[8],failing_timing_endpoints=0,
                resources=resources,routable_nets=total,fully_routed_nets=done,routing_errors=errors,
                drc_errors_or_critical_warnings=0,bus_skew_slacks_ns=slacks)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('directory',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    result=check(a.directory);a.output.write_text(json.dumps(result,indent=2)+'\n');print('PASS full routed setup/hold/pulse, resource, routing, DRC and bus-skew gates')
