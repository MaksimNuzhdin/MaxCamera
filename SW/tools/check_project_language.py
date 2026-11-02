#!/usr/bin/env python3
"""Enforce English-only project text, including source archives and PDF text.

Raw captures and executable/FPGA images are binary evidence, not language text.
Their bytes must never be edited to satisfy this check. JSON string values are
also inspected so escaped Unicode cannot bypass the policy.
"""
from __future__ import annotations
import argparse
import gzip
import io
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tarfile
import zipfile

FORBIDDEN = re.compile(r'[\u0400-\u052f]')
TEXT_SUFFIXES = {'.md','.txt','.rst','.log','.json','.csv','.xml','.xsd','.xci',
                 '.bd','.xpr','.c','.h','.cpp','.hpp','.cc','.inc','.py','.sh',
                 '.tcl','.v','.sv','.vhd','.xdc','.dts','.dtsi','.patch','.yaml',
                 '.yml','.toml','.ini','.cfg','.cmake','.bif','.bsp','.s','.html',
                 '.css','.js','.ts','.bat','.ps1','.gitignore'}
BINARY_SUFFIXES = {'.bin','.elf','.bit','.dcp','.png','.jpg','.jpeg','.gif','.webp',
                   '.pcap','.pcapng','.o','.a','.so','.ttf','.otf','.deb','.woff',
                   '.woff2','.pyc','.db','.sqlite','.sqlite3','.ico'}
SKIP_DIRS = {'.git','.codex','.agents','.aws','__pycache__'}

def json_strings(value):
    if isinstance(value,str):
        yield value
    elif isinstance(value,list):
        for item in value:
            yield from json_strings(item)
    elif isinstance(value,dict):
        for key,item in value.items():
            yield key
            yield from json_strings(item)

def text_payload(data: bytes, name: str):
    if b'\0' in data:
        return None
    try:
        text=data.decode('utf-8-sig')
    except UnicodeError:
        return None
    if Path(name).suffix.lower()=='.json':
        try:
            text+='\n'+'\n'.join(json_strings(json.loads(text)))
        except (ValueError,TypeError):
            pass
    return text

def scan(roots):
    failures=[];counts=dict(text_files=0,archive_members=0,pdfs=0)
    def inspect_text(data,name,archive=False):
        text=text_payload(data,name)
        if text is None:
            return
        counts['archive_members' if archive else 'text_files']+=1
        match=FORBIDDEN.search(text)
        if match:
            # Output locations only; do not reproduce forbidden text in logs.
            failures.append(dict(path=name,line=text.count('\n',0,match.start())+1,
                                 reason='Cyrillic text violates the English-only policy'))
    def inspect_archive(data,name,depth=0):
        if depth>3:
            raise ValueError('Nested archive depth exceeds the inspection limit: '+name)
        if name.endswith(('.tar.gz','.tgz','.tar')):
            with tarfile.open(fileobj=io.BytesIO(data),mode='r:*') as archive:
                for member in archive.getmembers():
                    if member.isfile() and Path(member.name).suffix.lower() not in BINARY_SUFFIXES:
                        inspect_text(archive.extractfile(member).read(),name+'::'+member.name,True)
        elif name.endswith(('.zip','.xsa','.hdf')):
            with zipfile.ZipFile(io.BytesIO(data)) as archive:
                for member in archive.infolist():
                    suffix=Path(member.filename).suffix.lower()
                    if member.is_dir() or suffix in BINARY_SUFFIXES:
                        continue
                    if suffix=='.zip':
                        inspect_archive(archive.read(member),name+'::'+member.filename,depth+1)
                    elif suffix in TEXT_SUFFIXES or not suffix:
                        inspect_text(archive.read(member),name+'::'+member.filename,True)
        else:
            inspect_text(gzip.decompress(data),name,True)
    for root in roots:
        for directory,dirs,files in os.walk(root):
            dirs[:]=[d for d in dirs if d not in SKIP_DIRS]
            for filename in files:
                path=Path(directory)/filename
                if path.is_symlink():
                    continue
                name=str(path)
                if FORBIDDEN.search(name):
                    failures.append(dict(path=name,reason='Non-English filename'))
                suffix=path.suffix.lower()
                if suffix in BINARY_SUFFIXES:
                    continue
                if suffix=='.pdf':
                    result=subprocess.run(['/usr/bin/pdftotext','-enc','UTF-8',str(path),'-'],
                                          capture_output=True,check=False)
                    if result.returncode:
                        failures.append(dict(path=name,reason='PDF text extraction failed'))
                    else:
                        counts['pdfs']+=1
                        text=result.stdout.decode('utf-8')
                        if FORBIDDEN.search(text):
                            failures.append(dict(path=name,reason='Cyrillic PDF text'))
                        meta=subprocess.run(['/usr/bin/pdfinfo',str(path)],capture_output=True,check=False)
                        if FORBIDDEN.search(meta.stdout.decode('utf-8',errors='replace')):
                            failures.append(dict(path=name,reason='Cyrillic PDF metadata'))
                elif suffix in {'.gz','.tgz','.tar','.zip','.xsa','.hdf'}:
                    inspect_archive(path.read_bytes(),name)
                elif suffix in TEXT_SUFFIXES or not suffix or filename=='CMakeLists.txt':
                    inspect_text(path.read_bytes(),name)
    return dict(passed=not failures,roots=[str(x) for x in roots],counts=counts,failures=failures)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('roots',nargs='*',type=Path)
    parser.add_argument('--json-output',type=Path)
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    roots=args.roots or [root,root.parents[1]/'MaxCamera']
    for path in roots:
        if not path.is_dir():
            parser.error('Missing scan root: '+str(path))
    result=scan(roots)
    if args.json_output:
        args.json_output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
    return 0 if result['passed'] else 1

if __name__=='__main__':
    sys.exit(main())
