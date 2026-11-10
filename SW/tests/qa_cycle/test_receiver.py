"""Exercise real UDP image/JPEG capture, frame limits and damaged-frame rejection."""
import csv
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
EXE=ROOT/'build/tests/qa_cycle_receiver-next'


class NativeReceiverTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run(['cc','-O2','-std=c11','-Wall','-Wextra','-Werror',
                        str(ROOT/'tools/qa_cycle_receiver.c'),'-o',str(EXE)],check=True)

    def capture(self, jpeg=False, fault=None):
        with tempfile.TemporaryDirectory() as temporary:
            path=Path(temporary)/'capture.bin';trace=path.with_suffix('.csv')
            env=os.environ.copy();env.update(MAXCAMERA_FRAME_LIMIT='3',MAXCAMERA_SAVE_SKIP='1',MAXCAMERA_JPEG=str(int(jpeg)))
            packet_trace=Path(temporary)/'failure-window.csv'
            env.update(MAXCAMERA_PACKET_TRACE=str(packet_trace),MAXCAMERA_TRACE_FAILURES='1')
            process=subprocess.Popen([str(EXE),'127.0.0.1','16','16',str(0x010a0052),'320','1',str(path),str(trace)],
                                     stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=env)
            ready=json.loads(process.stdout.readline());sender=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
            payloads={}
            try:
                def send(block, kind, number, body):
                    sender.sendto(struct.pack('!HHI',0,block,kind<<24|number)+body,('127.0.0.1',ready['port']))
                for index in range(1,6 if fault=='window_exhaustion' else 5 if fault not in (None,'wrap','late_previous_payload') else 4):
                    if fault=='whole_frame' and index==2:continue
                    block=({1:65535,2:1,3:2}[index] if fault=='wrap' else index)
                    payload=(b'JPEG'+bytes([block%256])*block) if jpeg else bytes([block%256])*320
                    payloads[block]=payload
                    body=(struct.pack('!HHQQQI',0,6,100*block,len(payload),1000000,0) if jpeg else
                          struct.pack('!HHQIIIIIHH',0,1,100*block,0x010a0052,16,16,0,0,0,0))
                    if fault=='metadata' and block==2:
                        body=struct.pack('!HHQIIIIIHH',0,1,100*block,0x010a0052,17,16,0,0,0,0)
                    if fault=='encoded_overflow' and block==2:
                        body=struct.pack('!HHQQQI',0,6,100*block,321,1000000,0)
                    send(block,1,0,body)
                    if fault=='late_previous_payload' and block==3:
                        send(2,3,1,payloads[2])
                    tail=struct.pack('!I',6) if jpeg else struct.pack('!II',1,16)
                    if fault=='duplicate' and block==2:
                        send(block,3,1,payload);send(block,3,1,payload);send(block,2,2,tail)
                        continue
                    # Deliberately deliver the trailer before payload data.
                    if not (fault=='missing_trailer' and block==2):send(block,2,2,tail)
                    if not ((fault in ['missing','late_previous_payload'] and block==2) or (fault=='window_exhaustion' and block==1)):send(block,3,1,payload)
                    if fault=='duplicate' and block==2:send(block,3,1,payload)
                stdout,stderr=process.communicate(timeout=3)
                record=json.loads(stdout)
                self.assertEqual(record['good'],3)
                self.assertEqual(process.returncode,0 if fault in (None,'wrap','late_previous_payload') else 1,(record,stderr))
                with trace.open() as stream:blocks=[int(row['block_id']) for row in csv.DictReader(stream)]
                self.assertEqual(blocks,[2,3,4] if fault=='window_exhaustion' else [65535,1,2] if fault=='wrap' else [1,2,3] if fault in (None,'late_previous_payload') else [1,3,4])
                self.assertFalse(Path(str(path)+'.frame000001.bin').exists())
                self.assertEqual(Path(str(path)+'.frame000002.bin').read_bytes(),payloads[blocks[1]])
                self.assertEqual(path.read_bytes(),payloads[blocks[-1]])
                with packet_trace.open() as stream:events=list(csv.DictReader(stream))
                self.assertEqual(bool(events),fault not in (None,'wrap','late_previous_payload'))
                self.assertEqual(record['block_gaps'],int(fault=='whole_frame'))
                self.assertEqual(record['sequence_errors'],0)
                self.assertEqual(record['socket_drops'],0)
                if fault=='late_previous_payload':
                    self.assertEqual(record['foreign_frame_packets'],0)
                    self.assertEqual(record['interleaved_packets'],1)
            finally:
                sender.close()
                if process.poll() is None:process.kill();process.wait()

    def test_window_exhaustion_keeps_missing_frame_failure(self):self.capture(fault='window_exhaustion')
    def test_missing_trailer_is_failed(self):self.capture(fault='missing_trailer')

    def test_entire_missing_frame_is_failed(self):self.capture(fault='whole_frame')
    def test_classic_block_id_wrap_is_contiguous(self):self.capture(fault='wrap')

    def test_image_count_and_early_trailer(self):self.capture()
    def test_jpeg_variable_lengths(self):self.capture(jpeg=True)
    def test_missing_image_payload(self):self.capture(fault='missing')
    def test_missing_jpeg_payload(self):self.capture(jpeg=True,fault='missing')
    def test_duplicate_payload(self):self.capture(fault='duplicate')
    def test_wrong_image_metadata(self):self.capture(fault='metadata')
    def test_jpeg_encoded_length_bound(self):self.capture(jpeg=True,fault='encoded_overflow')
    def test_previous_frame_payload_after_next_leader_is_reassembled(self):self.capture(fault='late_previous_payload')


if __name__=='__main__':unittest.main()
