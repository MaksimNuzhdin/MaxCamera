import unittest
from dataclasses import replace
from contract import Descriptor, Controller, encode_result, decode_result, WIRE

class ContractTests(unittest.TestCase):
    def setUp(self):
        self.d = Descriptor(1,2,3,400,320,240,960,64,230400)
        self.c = Controller(); self.c.configure(True)
    def result(self, seq=1):
        return encode_result(self.c.session,self.c.config,seq,100,[(1,32768,0,0,20,30)])
    def test_wire_layout_and_roundtrip(self):
        data=self.d.encode()
        self.assertEqual(len(data),WIRE.size)
        self.assertEqual(data[:16],b'MAI1\x01\x008\x00\x01\x00\x00\x00\x02\x00\x00\x00')
        self.assertEqual(Descriptor.decode(data,230464),self.d)
    def test_malformed_descriptor_and_reserved_bounds(self):
        for d in [replace(self.d,offset=0xffffffc0),replace(self.d,length=1),
                  replace(self.d,stride=1),replace(self.d,offset=1),replace(self.d,format=99)]:
            with self.assertRaises(ValueError):Descriptor.decode(d.encode(),230464)
        for wire in [self.d.encode()[:-1],self.d.encode()+b'\0',b'BAD!'+self.d.encode()[4:]]:
            with self.assertRaises(ValueError):Descriptor.decode(wire,230464)
    def test_result_payload_validation(self):
        wire=self.result();self.assertEqual(decode_result(wire)[2],1)
        for bad in [wire[:-1],wire+b'\0',b'BAD!'+wire[4:]]:
            with self.assertRaises(ValueError):decode_result(bad)
        with self.assertRaises(ValueError):encode_result(1,1,1,1,[(1,1,0,0,1,1)]*17)
    def test_snapshot_is_consistent_across_publication(self):
        self.c.publish(self.result());old=self.c.latch()
        self.c.publish(self.result(2));self.assertEqual(self.c.snapshot,old)
        self.assertEqual(self.c.latch()[2],2)
    def test_disable_invalidates_results_but_keeps_read_lease(self):
        token=self.c.offer(1);wire=self.result();self.c.publish(wire);self.c.latch()
        self.c.configure(False)
        self.assertIsNone(self.c.snapshot);self.assertFalse(self.c.publish(wire))
        self.assertEqual(self.c.lease,token);self.assertIsNone(self.c.offer(2))
        self.c.release(token)
    def test_busy_skips_and_timeout_does_not_revoke(self):
        token=self.c.offer(1);self.assertIsNone(self.c.offer(2))
        self.c.timeout();self.assertEqual(self.c.lease,token)
        with self.assertRaises(ValueError):self.c.restart(False)
        self.c.restart(True);self.assertIsNone(self.c.lease)
    def test_old_session_config_and_out_of_order_results_rejected(self):
        old=self.result();self.c.configure(True);self.assertFalse(self.c.publish(old))
        old=self.result();self.c.restart(True);self.c.configure(True)
        self.assertFalse(self.c.publish(old));self.assertTrue(self.c.publish(self.result(3)))
        self.assertFalse(self.c.publish(self.result(2)))
    def test_generation_wrap_fails_closed(self):
        self.c.config=0xffffffff
        with self.assertRaises(ValueError):self.c.configure(True)

if __name__=='__main__':unittest.main()
