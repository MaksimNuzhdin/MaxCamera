#!/usr/bin/env python3
"""Prepare calibrated Brown-Conrady inverse maps; never access the camera.

Maps describe an output pixel's source coordinate on an equal-size RGB canvas.
This is a calibration/build input tool, not a deployed remapping engine.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct


def validate(config):
    for key, limit in (("width", 2592), ("height", 1944)):
        value = config[key]
        if type(value) is not int or not 2 <= value <= limit:
            raise ValueError(f"Invalid {key}")
    for key in ("calibration_id", "geometry_id"):
        if not isinstance(config[key], str) or not config[key].strip():
            raise ValueError(f"Missing {key}")
    for key in ("input_intrinsics", "output_intrinsics"):
        values = config[key]
        if len(values) != 4 or not all(type(x) in (int, float) and math.isfinite(x) for x in values):
            raise ValueError(f"{key} must contain finite fx, fy, cx, cy")
        if values[0] <= 0 or values[1] <= 0:
            raise ValueError("Focal lengths must be positive")
    if len(config["distortion"]) != 5 or not all(
            type(x) in (int, float) and math.isfinite(x) for x in config["distortion"]):
        raise ValueError("Expected finite k1, k2, p1, p2, k3")


def coordinate(config, u, v):
    fx, fy, cx, cy = config["input_intrinsics"]
    ox, oy, ocx, ocy = config["output_intrinsics"]
    k1, k2, p1, p2, k3 = config["distortion"]
    x, y = (u-ocx)/ox, (v-ocy)/oy
    r2 = x*x+y*y
    radial = 1+r2*(k1+r2*(k2+r2*k3))
    sx = fx*(x*radial+2*p1*x*y+p2*(r2+2*x*x))+cx
    sy = fy*(y*radial+p1*(r2+2*y*y)+2*p2*x*y)+cy
    # The downstream AMD map interface consumes float32, not Python doubles.
    sx, sy = struct.unpack("<ff", struct.pack("<ff", sx, sy))
    if not math.isfinite(sx) or not math.isfinite(sy):
        raise ValueError("Nonfinite map coordinate")
    if not (-32768 <= sx <= 32767 and -32768 <= sy <= 32767):
        raise ValueError("Map exceeds conservative signed coordinate range")
    return sx, sy


def generate(config, output):
    validate(config)
    output = Path(output)
    output.mkdir(parents=True, exist_ok=False)
    width, height = config["width"], config["height"]
    interior = 0
    ahead = behind = 0
    hashes = [hashlib.sha256(), hashlib.sha256()]
    paths = [output/"map_x.f32le", output/"map_y.f32le"]
    try:
        with paths[0].open("wb") as xf, paths[1].open("wb") as yf:
            for v in range(height):
                for u in range(width):
                    sx, sy = coordinate(config, u, v)
                    # Explicit four-tap interior policy; boundary pixels require
                    # an engine-defined border rule and are not silently qualified.
                    if 0 <= sx < width-1 and 0 <= sy < height-1:
                        interior += 1
                        low = math.floor(sy)
                        ahead = max(ahead, low+1-v)
                        behind = max(behind, v-low)
                    for value, stream, digest in zip((sx, sy), (xf, yf), hashes):
                        raw = struct.pack("<f", value)
                        stream.write(raw)
                        digest.update(raw)
        # A symmetric buffer must avoid reading the current partially written
        # row. This is a conservative planning bound, not an AMD RTL proof.
        window = 2*(max(ahead, behind)+2)
        result = dict(schema_version=1, configuration=config, pixels=width*height,
                      format="separate raster little-endian IEEE754 float32 maps",
                      map_bytes=width*height*8, rgb_frame_bytes=width*height*3,
                      bilinear_interior_pixels=interior,
                      boundary_or_outside_pixels=width*height-interior,
                      lookahead_rows=ahead, lookbehind_rows=behind,
                      conservative_window_rows=window,
                      logical_rgb_window_bytes=(window+2)*width*3,
                      sha256={p.name: h.hexdigest() for p, h in zip(paths, hashes)},
                      deployed=False, optical_validation=False,
                      limitations=["No map DMA or camera remap engine installed",
                                   "Window bound is not synthesized BRAM usage or a throughput claim",
                                   "AMD remap edge-row behavior requires explicit RTL qualification"])
        (output/"report.json").write_text(json.dumps(result, indent=2)+"\n")
        return result
    except Exception:
        # Never leave a successful-looking partial map set.
        for p in paths:
            p.unlink(missing_ok=True)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("calibration", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    raw = args.calibration.read_bytes()
    config = json.loads(raw)
    config["source_sha256"] = hashlib.sha256(raw).hexdigest()
    report = generate(config, args.output)
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
