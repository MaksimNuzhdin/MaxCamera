"""Exercise the real Python worker against a sparse physical-memory surrogate."""
import mmap
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]
BASE = 0x1f000000

class MailboxWorkerTest(unittest.TestCase):
    def test_worker_copy_ack_and_committed_result(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ('worker.py','runtime.py'):
                shutil.copy2(ROOT/'ai'/name,root/name)
            (root/'models').symlink_to(ROOT/'ai/models')
            (root/'presets/faces').mkdir(parents=True)
            subprocess.run(['cc','-shared','-fPIC',str(ROOT/'ai/barrier.c'),'-o',str(root/'libai_barrier.so')],check=True)
            physical = root/'memory'
            with physical.open('w+b') as file:
                file.truncate(BASE+8192)
                memory = mmap.mmap(file.fileno(),8192,offset=BASE)
                def write(word, *values):struct.pack_into('<'+'I'*len(values),memory,word*4,*values)
                def read(word):return struct.unpack_from('<I',memory,word*4)[0]
                write(0,0x3149414d,1)
                write(8,2,16,500000,500000)
                write(13,450000)
                # A completed padded RGB image at an allowed R5 physical address.
                address,w,h,stride=0x200000,32,24,128
                file.seek(address);file.write(bytes(h*stride));file.flush()
                process = subprocess.Popen([sys.executable,str(root/'worker.py'),'--device',str(physical)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
                try:
                    deadline=time.monotonic()+30
                    while not read(6) and process.poll() is None and time.monotonic()<deadline:time.sleep(.02)
                    self.assertEqual(read(7),255)
                    write(32,1,2,address,w,h,stride,0x02180014,123,0,45)
                    write(2,1)
                    while read(4)!=1 and process.poll() is None and time.monotonic()<deadline:time.sleep(.02)
                    self.assertEqual(read(3),1,'Exact capture-copy acknowledgement')
                    self.assertEqual(read(4),1,'Committed result sequence')
                    self.assertEqual(read(64),1)
                    self.assertEqual(read(65),2)
                    self.assertEqual(read(66),0)
                    self.assertEqual(read(68),0)
                    # A second request cannot run until R5 acknowledges the first result.
                    write(32,2,2,address,w,h,stride,0x02180014,124,0,46);write(2,2)
                    time.sleep(.1);self.assertEqual(read(3),1)
                    write(5,1)
                    while read(4)!=2 and time.monotonic()<deadline:time.sleep(.02)
                    self.assertEqual(read(3),2)
                    self.assertEqual(read(4),2)
                finally:
                    process.terminate()
                    try:process.communicate(timeout=5)
                    except subprocess.TimeoutExpired:process.kill();process.communicate()
                    memory.close()

if __name__ == '__main__':unittest.main()
