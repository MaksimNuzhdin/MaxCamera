"""Replay interleaved frames with deterministic pixels through the native receiver."""
import csv
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import time
import unittest

ROOT=Path(__file__).resolve().parents[2]


class CrossFrameReplayTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe=ROOT/'build/tests/qa_cycle_receiver-replay'
        subprocess.run(['cc','-O2','-std=c11','-Wall','-Wextra','-Werror',str(ROOT/'tools/qa_cycle_receiver.c'),'-o',str(cls.exe)],check=True)

    def test_late_previous_payloads_and_trailer_preserve_both_images(self):
        # Reproduce the observed ordering: an early trailer and the next
        # leader precede the remaining first-frame payload packets.
        size=921600
        fixture=json.loads((Path(__file__).with_name('fixtures')/'interleaved-rgb640.json').read_text())
        schedule=fixture['packets']
        with tempfile.TemporaryDirectory() as tmp:
            output=Path(tmp)/'frame.bin';trace=Path(tmp)/'frames.csv'
            env={**os.environ,'MAXCAMERA_FRAME_LIMIT':'2'}
            process=subprocess.Popen([str(self.exe),'127.0.0.1','640','480',str(0x02180014),str(size),'3',str(output),str(trace)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=env)
            sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
            try:
                ready=json.loads(process.stdout.readline())
                for block,kind,pid in schedule:
                    if kind==1:body=struct.pack('!HHQIIIIIHH',0,1,block*70000,0x02180014,640,480,0,0,0,0)
                    elif kind==2:body=struct.pack('!II',1,480)
                    else:body=bytes([block])*min(1464,size-(pid-1)*1464)
                    sock.sendto(struct.pack('!HHI',0,block,kind<<24|pid)+body,('127.0.0.1',ready['port']))
                    time.sleep(.0001)
                stdout,stderr=process.communicate(timeout=5);result=json.loads(stdout)
                self.assertEqual(process.returncode,0,(result,stderr))
                self.assertEqual(result['good'],2);self.assertEqual(result['missing_packets'],0)
                self.assertGreater(result['interleaved_packets'],0)
                self.assertEqual(Path(str(output)+'.first.bin').read_bytes(),bytes([1])*size)
                self.assertEqual(output.read_bytes(),bytes([2])*size)
                with trace.open() as stream:rows=list(csv.DictReader(stream))
                self.assertEqual([int(row['block_id']) for row in rows],[1,2])
                self.assertEqual([int(row['timestamp_ticks']) for row in rows],[70000,140000])
                for block,row in enumerate(rows,1):
                    expected=14695981039346656037
                    for _ in range(size):expected=((expected^block)*1099511628211)&0xffffffffffffffff
                    self.assertEqual(row['fnv64'],f'{expected:016x}')
                    self.assertEqual(float(row['byte_mean']),float(block))
            finally:
                sock.close()
                if process.poll() is None:process.kill();process.communicate()


if __name__=='__main__':unittest.main()
