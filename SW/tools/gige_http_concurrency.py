#!/usr/bin/env python3
"""Read-only concurrent HTTP status load; retain every error and latency."""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import threading
import time
from urllib.error import HTTPError
from urllib.request import build_opener,ProxyHandler


def load(ip,seconds,clients,stop=None):
    start=threading.Barrier(clients);records=[]
    def reader(index):
        http=build_opener(ProxyHandler({}));start.wait();deadline=time.monotonic()+seconds
        while time.monotonic()<deadline and not (stop and stop.is_set()):
            t=time.monotonic();size=0;error=None
            try:
                with http.open('http://'+ip+'/api/status',timeout=5) as response:
                    data=response.read();size=len(data);code=response.status
                    status=json.loads(data)
                    assert status['transport']=='ethernet'
            except HTTPError as e:code=e.code;size=len(e.read())
            except (OSError,ValueError,TimeoutError,AssertionError) as e:code=type(e).__name__;error=str(e)
            records.append(dict(client=index,status=code,bytes=size,milliseconds=1000*(time.monotonic()-t),error=error))
    with ThreadPoolExecutor(max_workers=clients) as pool:list(pool.map(reader,range(clients)))
    codes=dict(Counter(str(r['status']) for r in records));latencies=sorted(r['milliseconds'] for r in records)
    summary=dict(clients=clients,seconds=seconds,requests=len(records),statuses=codes,
                 latency_ms=dict(p50=latencies[len(latencies)//2] if latencies else None,p99=latencies[min(len(latencies)-1,int(len(latencies)*.99))] if latencies else None,maximum=max(latencies) if latencies else None),
                 passed=len(records)>=clients and codes=={'200':len(records)})
    return summary,records


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ip',default='10.0.0.10')
    p.add_argument('--seconds',type=int,default=30);p.add_argument('--clients',type=int,choices=(1,2,4),default=2)
    p.add_argument('--output',type=Path,required=True);a=p.parse_args();assert 5<=a.seconds<=600
    a.output.mkdir(parents=True,exist_ok=False);summary,records=load(a.ip,a.seconds,a.clients)
    (a.output/'requests.json').write_text(json.dumps(records,indent=2)+'\n')
    (a.output/'result.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary),flush=True)
    return 0 if summary['passed'] else 1


if __name__=='__main__':raise SystemExit(main())
