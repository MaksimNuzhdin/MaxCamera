#!/usr/bin/env python3
"""Prove the independent receiver rejects missing, duplicate and bad metadata."""
import csv,json,socket,struct,subprocess,tempfile,time
from pathlib import Path
root=Path(__file__).resolve().parents[2]
exe=root/'build/tests/gige_udp_receiver'
with tempfile.TemporaryDirectory() as out:
 for fault in ['none','missing','duplicate','metadata']:
  dest=Path(out)/fault
  trace=Path(out)/(fault+'.csv')
  proc=subprocess.Popen([str(exe),'127.0.0.1','16','16',str(0x010a0052),'320','.15',str(dest),str(trace)],stdout=subprocess.PIPE,text=True)
  port=json.loads(proc.stdout.readline())['port'];s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
  def packet(block,kind,id,body):s.sendto(struct.pack('!HHI',0,block,(kind<<24)|id)+body,('127.0.0.1',port))
  payload=bytes((i*73+9)&255 for i in range(320))
  for block in range(1,4):
   leader=struct.pack('!HHQIIIIHH',0,1,0xfffffffe+block,0x010a0052,17 if fault=='metadata' and block==2 else 16,16,0,0,0)+bytes(4)
   assert len(leader)==36
   packet(block,1,0,leader)
   if not(fault=='missing' and block==2):packet(block,3,1,payload)
   if fault=='duplicate' and block==2:packet(block,3,1,payload)
   packet(block,2,2,struct.pack('!II',1,16));time.sleep(.005)
  s.close();text=proc.communicate(timeout=2)[0];record=json.loads(text)
  assert record['good']==(3 if fault=='none' else 2),record
  assert record['bad']==(0 if fault=='none' else 1),record
  assert proc.returncode==(0 if fault=='none' else 1),record
  assert dest.read_bytes()==payload
  frames=list(csv.DictReader(trace.open()))
  assert len(frames)==record['good']
  assert [int(f['timestamp_ticks']) for f in frames]==[0xfffffffe+b for b in range(1,4) if fault=='none' or b!=2]
 print('PASS: independent GVSP receiver detects loss, duplicates, invalid metadata; exact complete payload and 64-bit timestamp trace')
