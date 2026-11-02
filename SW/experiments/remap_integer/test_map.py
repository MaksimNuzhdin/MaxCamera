import hashlib
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zlib
from compile_map import compile_words, compile_directory


def floats(values):
    return b''.join(struct.pack('<f', v) for v in values)


class MapCompilerTests(unittest.TestCase):
    def test_identity_and_fractional_rounding(self):
        data, black, displacement = compile_words(3, 1, floats([0, .6, 2]), floats([0]*3))
        self.assertEqual(struct.unpack('<III', data), (0x80000000, 0x80000001, 0x80000002))
        self.assertEqual((black, displacement), (0, 0))

    def test_bounds_before_quantization(self):
        data, black, _ = compile_words(3, 1, floats([-.1, 1, 2.1]), floats([0]*3))
        self.assertEqual(struct.unpack('<III', data), (0, 0x80000001, 0))
        self.assertEqual(black, 2)

    def test_displacement_rejected(self):
        with self.assertRaisesRegex(ValueError, 'displacement'):
            compile_words(1, 9, floats([0]*9), floats([8]*9))

    def test_nonfinite_and_size_and_geometry(self):
        for value in (float('nan'), float('inf'), -float('inf')):
            with self.assertRaisesRegex(ValueError, 'Nonfinite'):
                compile_words(1, 1, floats([value]), floats([0]))
        for args in ((641, 1, b'', b''), (1, 1, b'', b''), (True, 1, b'', b'')):
            with self.assertRaises(ValueError):
                compile_words(*args)

    def test_hashes_no_overwrite_and_failed_input_leaves_no_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp)/'source'; source.mkdir()
            hashes = {}
            for name in ('map_x.f32le', 'map_y.f32le'):
                raw = floats([0]); (source/name).write_bytes(raw)
                hashes[name] = hashlib.sha256(raw).hexdigest()
            (source/'report.json').write_text(json.dumps(dict(configuration=dict(width=1,height=1),sha256=hashes)))
            destination = Path(tmp)/'compiled'
            report = compile_directory(source, destination)
            self.assertEqual(report['sha256'], hashlib.sha256((destination/'map.u32le').read_bytes()).hexdigest())
            self.assertEqual(report['crc32'], zlib.crc32((destination/'map.u32le').read_bytes()))
            self.assertFalse(report['optical_validation'])
            with self.assertRaises(FileExistsError):
                compile_directory(source, destination)
            (source/'map_x.f32le').write_bytes(floats([1]))
            with self.assertRaisesRegex(ValueError, 'hash'):
                compile_directory(source, Path(tmp)/'bad')
            self.assertFalse((Path(tmp)/'bad').exists())


if __name__ == '__main__':
    unittest.main()
