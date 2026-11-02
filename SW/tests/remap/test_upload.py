"""Offline client tests. Fake control does not constitute board acceptance."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib
sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'tools'))
from upload_distortion_map import REG, read_map, upload


class Camera:
    def __init__(self):
        self.reg = {REG[k]: v for k, v in dict(FPGA_VERSION=12, FPGA_CAPABILITIES=262143, REMAP_AVAILABLE=1,
            SENSOR_BUSY=0, SENSOR_READY=1, FPGA_ENABLE=1, REMAP_STATE=0,
            REMAP_ENABLE=0, REMAP_UPLOADED=0, REMAP_CHECKED=0, REMAP_ERROR=0).items()}
        self.reg.update({0x10018: 0, 0x1001c: 0, 0x10000: 40, 0x10004: 1})
        self.words = [];self.writes = [];self.bad_ack = False;self.validation_error = False

    def read(self, address):
        return self.reg[address]

    def write(self, address, value):
        self.writes.append((address, value));self.reg[address] = value
        if address == REG['REMAP_BEGIN']:
            self.reg[REG['REMAP_STATE']] = 1
        if address == REG['REMAP_COMMIT']:
            raw = struct.pack('<'+'I'*len(self.words), *self.words)
            assert zlib.crc32(raw) == self.reg[REG['REMAP_EXPECTED_CRC']]
            self.reg[REG['REMAP_UPLOADED']] = self.reg[REG['REMAP_CHECKED']] = len(self.words)
            self.reg[REG['REMAP_STATE']] = 4 if self.validation_error else 3

    def command(self, opcode, body):
        assert opcode == 0x82 and len(body) <= 512 and len(body) % 16 == 0
        for index_addr, index, data_addr, value in struct.iter_unpack('!IIII', body):
            assert (index_addr, data_addr) == (REG['REMAP_INDEX'], REG['REMAP_DATA'])
            assert index == len(self.words)
            self.words.append(value)
        return struct.pack('!HH', 0, len(body)//8-int(self.bad_ack)), None


class UploadTests(unittest.TestCase):
    def setUp(self):
        self.data = struct.pack('<40I', *(0x80000000+x for x in range(40)))
        self.report = dict(width=40, height=1, sha256=hashlib.sha256(self.data).hexdigest(), crc32=zlib.crc32(self.data))

    def test_batch_and_enable_after_validation(self):
        c = Camera();result = upload(c, self.report, self.data, True)
        self.assertEqual(result['words'], 40)
        self.assertEqual(c.writes[-1], (REG['REMAP_ENABLE'], 1))
        self.assertFalse(result['persistence']);self.assertFalse(result['optical_validation'])

    def test_supported_pairs_and_unknown_pair_rejection(self):
        for version, capability in [(12, 262143), (13, 524287)]:
            c = Camera()
            c.reg[REG['FPGA_VERSION']] = version
            c.reg[REG['FPGA_CAPABILITIES']] = capability
            self.assertEqual(upload(c, self.report, self.data)['words'], 40)
        for version, capability in [(11, 262143), (14, 524287),
                                     (13, 262143), (12, 524287)]:
            c = Camera()
            c.reg[REG['FPGA_VERSION']] = version
            c.reg[REG['FPGA_CAPABILITIES']] = capability
            with self.assertRaisesRegex(RuntimeError, 'matching validated'):
                upload(c, self.report, self.data)
            self.assertEqual(c.writes, [])
            self.assertEqual(c.words, [])

    def test_preflight_preserves_existing_map_and_stream(self):
        for address, value in [(REG['REMAP_STATE'], 3), (0x10018, 1),
                (0x1001c, 1), (REG['FPGA_VERSION'], 11), (0x10000, 80)]:
            c = Camera();c.reg[address] = value
            with self.assertRaises(RuntimeError):upload(c, self.report, self.data)
            self.assertEqual(c.writes, [])

    def test_partial_ack_never_commits(self):
        c = Camera();c.bad_ack = True
        with self.assertRaises(RuntimeError):upload(c, self.report, self.data, True)
        self.assertNotIn((REG['REMAP_COMMIT'], 1), c.writes)
        self.assertNotIn((REG['REMAP_ENABLE'], 1), c.writes)

    def test_validation_error_never_enables(self):
        c = Camera();c.validation_error = True
        with self.assertRaises(RuntimeError):upload(c, self.report, self.data, True)
        self.assertNotIn((REG['REMAP_ENABLE'], 1), c.writes)

    def test_file_integrity_before_network(self):
        with tempfile.TemporaryDirectory() as name:
            p = Path(name);(p/'report.json').write_text(json.dumps(self.report));(p/'map.u32le').write_bytes(self.data)
            self.assertEqual(read_map(p)[1], self.data)
            (p/'map.u32le').write_bytes(self.data[:-1])
            with self.assertRaises(ValueError):read_map(p)

    def test_hashed_but_unsupported_coordinate(self):
        bad = struct.pack('<I', 0x80000000 | (8 << 10)) + self.data[4:]
        report = dict(self.report, sha256=hashlib.sha256(bad).hexdigest(), crc32=zlib.crc32(bad))
        with tempfile.TemporaryDirectory() as name:
            p = Path(name);(p/'report.json').write_text(json.dumps(report));(p/'map.u32le').write_bytes(bad)
            with self.assertRaises(ValueError):read_map(p)


if __name__ == '__main__':
    unittest.main()
