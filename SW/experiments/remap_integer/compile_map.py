#!/usr/bin/env python3
"""Compile existing float maps to the isolated integer remapper's bounded ABI.

This is not lens calibration or a camera upload tool. Source maps are immutable.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import zlib


def compile_words(width, height, x_bytes, y_bytes):
    if type(width) is not int or type(height) is not int or not (1 <= width <= 640 and 1 <= height <= 480):
        raise ValueError("Unsupported geometry: maximum 640x480")
    if len(x_bytes) != width*height*4 or len(y_bytes) != len(x_bytes):
        raise ValueError("Map plane size mismatch")
    result = bytearray()
    black = 0
    largest = 0
    for n, ((x,), (y,)) in enumerate(zip(struct.iter_unpack('<f', x_bytes), struct.iter_unpack('<f', y_bytes))):
        if not math.isfinite(x) or not math.isfinite(y):
            raise ValueError(f"Nonfinite coordinate at pixel {n}")
        if x < 0 or x > width-1 or y < 0 or y > height-1:
            word = 0
            black += 1
        else:
            sx, sy = math.floor(x+.5), math.floor(y+.5)
            displacement = abs(sy-n//width)
            if displacement > 7:
                raise ValueError(f"Unsupported vertical displacement at pixel {n}: {displacement} rows")
            largest = max(largest, displacement)
            word = 0x80000000 | sx | (sy << 10)
        result.extend(struct.pack('<I', word))
    return bytes(result), black, largest


def compile_directory(source, destination):
    source, destination = Path(source), Path(destination)
    report_raw = (source/'report.json').read_bytes()
    report = json.loads(report_raw)
    config = report['configuration']
    planes = [(source/name).read_bytes() for name in ('map_x.f32le', 'map_y.f32le')]
    for name, data in zip(('map_x.f32le', 'map_y.f32le'), planes):
        if hashlib.sha256(data).hexdigest() != report['sha256'][name]:
            raise ValueError(f"Source hash mismatch: {name}")
    data, black, displacement = compile_words(config['width'], config['height'], *planes)
    result = dict(schema_version=1, width=config['width'], height=config['height'],
                  source_report_sha256=hashlib.sha256(report_raw).hexdigest(),
                  source_map_sha256=report['sha256'],
                  calibration_id=config.get('calibration_id'), geometry_id=config.get('geometry_id'),
                  format='little-endian uint32: valid31, x[9:0], y[18:10], reserved[30:19]=0; zero=black',
                  interpolation='nearest; floor(coordinate+0.5) after float32 input bounds check',
                  map_bytes=len(data), constant_black_pixels=black,
                  maximum_vertical_displacement=displacement,
                  sha256=hashlib.sha256(data).hexdigest(),
                  crc32=zlib.crc32(data),
                  deployed=False, optical_validation=False,
                  limitations=['Same-canvas maximum 640x480 and +/-7 source rows',
                               'Map must remain immutable throughout acquisition',
                               'No camera upload, cache maintenance or map lease integration'])
    destination.mkdir(parents=True, exist_ok=False)
    try:
        (destination/'map.u32le').write_bytes(data)
        (destination/'report.json').write_text(json.dumps(result, indent=2)+'\n')
    except Exception:
        for name in ('map.u32le', 'report.json'):
            (destination/name).unlink(missing_ok=True)
        raise
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    print(json.dumps(compile_directory(args.source, args.destination), indent=2))
