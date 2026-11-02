#!/usr/bin/env python3
"""Read-only source manifest/privacy/path/link screening; no Git or network writes."""
from pathlib import Path
import hashlib
import json
import re
import sys
root=Path(__file__).resolve().parents[1]
skip={'.git','build','__pycache__','.Xil'}
forbidden={'.elf','.bit','.bin','.dcp','.xsa','.pdi','.o','.obj','.pcap','.pcapng','.pyc','.log','.jou'}
files=[];errors=[]
secret_patterns=[r'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----',r'AKIA[0-9A-Z]{16}',r'gh[pousr]_[A-Za-z0-9]{30,}']
for p in sorted(root.rglob('*')):
 if any(x in skip for x in p.relative_to(root).parts):continue
 if p.is_symlink():errors.append([str(p.relative_to(root)),'symlink requires explicit review']);continue
 if not p.is_file():continue
 name=str(p.relative_to(root));data=p.read_bytes()
 if p.suffix in forbidden:errors.append([name,'generated/raw artifact'])
 if len(data)>10*1024*1024:errors.append([name,'over10MiB source budget'])
 files.append(dict(path=name,bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
 try:text=data.decode('utf-8')
 except UnicodeDecodeError:continue
 if ('/'+'home'+'/') in text or re.search(r'[A-Z]:\\Users\\',text):errors.append([name,'workstation path'])
 for pattern in secret_patterns:
  if re.search(pattern,text):errors.append([name,'credential-shaped content; inspect locally'])
 if p.suffix=='.md':
  for target in re.findall(r'\]\(([^)]+)\)',text):
   if target.startswith(('http:','https:','mailto:','#')):continue
   base=target.strip('<>').split('#')[0]
   if not (p.parent/base).exists():errors.append([name,'missing Markdown target: '+base])
print(json.dumps(dict(passed=not errors,files=len(files),bytes=sum(x['bytes'] for x in files),errors=errors,manifest=files),indent=2))
sys.exit(bool(errors))
