#!/usr/bin/env python3
"""Fetch the exact declared model bytes; never silently accept a changed upstream file."""
import argparse
import hashlib
import json
from pathlib import Path
import urllib.request

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--fixtures',action='store_true')
args=parser.parse_args()
for model in json.loads((ROOT/'ai/models/MANIFEST.json').read_text())['models']:
    destination=ROOT/'ai/models'/model['file']
    if destination.is_file() and hashlib.sha256(destination.read_bytes()).hexdigest()==model['sha256']:
        print('Verified '+model['file']);continue
    url=model['source'].replace('https://github.com/opencv/opencv/blob/','https://raw.githubusercontent.com/opencv/opencv/')
    with urllib.request.urlopen(url,timeout=120) as response:data=response.read()
    if len(data)!=model['bytes'] or hashlib.sha256(data).hexdigest()!=model['sha256']:
        raise RuntimeError('Upstream model bytes changed: '+model['file'])
    temporary=destination.with_suffix(destination.suffix+'.partial')
    temporary.write_bytes(data);temporary.replace(destination)
    print('Downloaded and verified '+model['file'])
if args.fixtures:
    directory=ROOT/'build/ai-downloads';directory.mkdir(parents=True,exist_ok=True)
    for name in ('lena.jpg','basketball1.png'):
        with urllib.request.urlopen('https://raw.githubusercontent.com/opencv/opencv/4.12.0/samples/data/'+name,timeout=30) as response:
            (directory/name).write_bytes(response.read())
