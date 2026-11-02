"""Synthetic map arithmetic and planning tests, not lens calibration."""
import copy
import hashlib
import importlib.util
import math
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("maps", Path(__file__).resolve().parents[1]/"tools/distortion_maps.py")
maps = importlib.util.module_from_spec(spec)
spec.loader.exec_module(maps)


class DistortionMaps(unittest.TestCase):
    def setUp(self):
        self.config = dict(width=8, height=6, calibration_id="synthetic-test-only",
                           geometry_id="synthetic-full-canvas",
                           input_intrinsics=[2, 2, 3, 2], output_intrinsics=[2, 2, 3, 2],
                           distortion=[0, 0, 0, 0, 0])

    def test_identity_serialization_and_border_scope(self):
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp)/"maps"
            result = maps.generate(self.config, out)
            xs = struct.unpack("<48f", (out/"map_x.f32le").read_bytes())
            ys = struct.unpack("<48f", (out/"map_y.f32le").read_bytes())
            self.assertEqual(xs, tuple(float(x) for y in range(6) for x in range(8)))
            self.assertEqual(ys, tuple(float(y) for y in range(6) for x in range(8)))
            self.assertEqual(result["bilinear_interior_pixels"], 35)
            self.assertEqual(result["boundary_or_outside_pixels"], 13)
            self.assertEqual(result["map_bytes"], 384)
            self.assertFalse(result["deployed"])
            self.assertEqual(result["sha256"]["map_x.f32le"], hashlib.sha256((out/"map_x.f32le").read_bytes()).hexdigest())
            with self.assertRaises(FileExistsError):
                maps.generate(self.config, out)

    def test_radial_and_tangential_known_coordinates(self):
        # At normalized (1, 0): radial=1.25; tangential=(3*p2,p1).
        self.config["distortion"] = [.25, 0, .125, .25, 0]
        self.assertEqual(maps.coordinate(self.config, 5, 2), (7.0, 2.25))
        # At normalized (0, 1): tangential=(p2,3*p1).
        self.assertEqual(maps.coordinate(self.config, 3, 4), (3.5, 5.25))

    def test_different_output_intrinsics(self):
        self.config["output_intrinsics"] = [4, 4, 3, 2]
        self.assertEqual(maps.coordinate(self.config, 5, 4), (4.0, 3.0))

    def test_displacement_budget_and_outside(self):
        self.config["input_intrinsics"][3] = 4
        with tempfile.TemporaryDirectory() as temp:
            result = maps.generate(self.config, Path(temp)/"maps")
        self.assertEqual(result["lookahead_rows"], 3)
        self.assertEqual(result["conservative_window_rows"], 10)
        self.assertEqual(result["bilinear_interior_pixels"], 21)

    def test_invalid_input_and_nonfinite_map(self):
        for key, value in [("width", True), ("height", 1945), ("calibration_id", ""),
                           ("distortion", [math.nan]*5), ("input_intrinsics", [0, 2, 3, 2])]:
            c = copy.deepcopy(self.config);c[key] = value
            with self.assertRaises(ValueError):
                maps.validate(c)
        self.config["distortion"] = [1e308]*5
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp)/"maps"
            with self.assertRaises((ValueError, OverflowError)):
                maps.generate(self.config, out)
            self.assertFalse((out/"report.json").exists())
            self.assertFalse((out/"map_x.f32le").exists())


if __name__ == "__main__":
    unittest.main()
