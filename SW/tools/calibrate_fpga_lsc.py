#!/usr/bin/env python3
"""Fit the FPGA 4x7x9 Q12 LSC grid from user-supplied uniform-field RAW10 frames.

Input files contain little-endian 16-bit BGGR samples. Capture with FPGA/sensor
lens correction disabled, fixed exposure/gain/WB, and uniform illumination.
This command does not capture frames, write camera settings or claim calibration
quality without an independent validation field.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np


def fit(frames, black=0):
    data = np.asarray(frames, dtype=np.float64)
    if data.ndim != 3 or min(data.shape[1:]) < 32 or any(n % 2 for n in data.shape[1:]):
        raise ValueError('Expected at least 32x32 even-sized BGGR frames')
    if not np.isfinite(data).all() or data.min() < 0 or data.max() > 1023 or not 0 <= black < 1023:
        raise ValueError('RAW10 samples or black level are outside bounds')
    if np.mean(data >= 1000) > .01:
        raise ValueError('Uniform field has more than 1% clipped samples')
    image = np.median(data, axis=0) - black
    table = np.empty((4, 7, 9), dtype=np.uint16)
    measurements = []
    for phase, (dy, dx) in enumerate([(0, 0), (0, 1), (1, 0), (1, 1)]):
        plane = image[dy::2, dx::2]
        reference = float(np.percentile(plane, 95))
        samples = np.empty((7, 9))
        radius_y, radius_x = max(1, plane.shape[0] // 28), max(1, plane.shape[1] // 36)
        for gy in range(7):
            y = round(gy * (plane.shape[0] - 1) / 6)
            for gx in range(9):
                x = round(gx * (plane.shape[1] - 1) / 8)
                samples[gy, gx] = np.median(plane[max(0, y-radius_y):y+radius_y+1,
                                                       max(0, x-radius_x):x+radius_x+1])
        if samples.min() < 32 or reference < 32:
            raise ValueError('Uniform field is too dark for reliable LSC fitting')
        ratios = np.maximum(1, reference / samples)
        if ratios.max() > 4:
            raise ValueError('Required lens-shading gain exceeds FPGA 4x bound')
        table[phase] = np.rint(ratios * 4096).astype(np.uint16)
        measurements.append({'phase': phase, 'reference_dn': reference,
                             'minimum_patch_dn': float(samples.min()),
                             'maximum_gain': float(ratios.max())})
    return table, measurements


def read_frame(path, width, height):
    """Hash the exact bytes used, rejecting partial samples and trailing bytes."""
    payload = path.read_bytes()
    if len(payload) != width * height * 2:
        raise ValueError(f'{path}: expected {width*height*2} bytes, got {len(payload)}')
    raw = np.frombuffer(payload, dtype='<u2').reshape(height, width)
    if raw.max() > 1023:
        raise ValueError(f'{path}: samples exceed RAW10')
    return raw, {'path': str(path.resolve()), 'bytes': len(payload),
                 'sha256': hashlib.sha256(payload).hexdigest()}


def corrected_frame(raw, table, black):
    """Model RAW BLC then the Q16 interpolation/Q12 rounding in raw_lsc.cpp.

    BPC is not modeled. This estimates table behavior, not calibrated optics.
    """
    rows, cols = raw.shape
    result = np.empty_like(raw)
    cx = np.arange(cols, dtype=np.int64) * 8 * 65536 // (cols-1)
    ix = np.minimum(cx >> 16, 7)
    fx = cx - ix * 65536
    for y in range(rows):
        cy = y * 6 * 65536 // (rows-1)
        iy = min(cy >> 16, 5)
        fy = cy - iy * 65536
        phase = (y & 1)*2 + (np.arange(cols) & 1)
        gains = table.astype(np.int64, copy=False)
        top = gains[phase, iy, ix]*(65536-fx) + gains[phase, iy, ix+1]*fx
        bottom = gains[phase, iy+1, ix]*(65536-fx) + gains[phase, iy+1, ix+1]*fx
        gain = (top*(65536-fy) + bottom*fy + (1 << 31)) >> 32
        value = np.maximum(raw[y].astype(np.int64)-black, 0)
        result[y] = np.minimum((value*gain+2048) >> 12, 1023)
    return result


def field_metrics(raw):
    """Descriptive per-phase spread, with no invented optical pass threshold."""
    metrics = []
    for phase, (dy, dx) in enumerate([(0, 0), (0, 1), (1, 0), (1, 1)]):
        plane = raw[dy::2, dx::2]
        mean = float(plane.mean())
        metrics.append({'phase': phase, 'mean_dn': mean,
                        'coefficient_of_variation': float(plane.std()/mean) if mean else None,
                        'p95_minus_p5_dn': float(np.percentile(plane, 95)-np.percentile(plane, 5)),
                        'saturated_fraction': float(np.mean(plane >= 1023))})
    return metrics


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--input', type=Path, action='append', required=True)
    p.add_argument('--width', type=int, required=True)
    p.add_argument('--height', type=int, required=True)
    p.add_argument('--black', type=int, default=0)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--validation-input', type=Path, action='append', default=[],
                   help='Optional held-out uniform-field RAW10; never used to fit the grid')
    p.add_argument('--conditions', type=Path,
                   help='Optional JSON object of owner-supplied capture conditions; not independently verified')
    a = p.parse_args()
    if min(a.width, a.height) < 32 or a.width % 2 or a.height % 2 or not 0 <= a.black < 1023:
        p.error('Expected even dimensions of at least 32 and black level in [0,1022]')
    report_path = a.output.with_suffix('.json')
    sources = a.input + a.validation_input + ([a.conditions] if a.conditions else [])
    outputs = [a.output.resolve(), report_path.resolve()]
    def aliases(left, right):
        return left.resolve() == right.resolve() or (
            left.exists() and right.exists() and left.samefile(right))
    if aliases(a.output, report_path) or any(aliases(x, y) for x in sources for y in outputs):
        p.error('Table/report destinations must differ and must not overwrite input evidence')
    try:
        conditions = json.loads(a.conditions.read_text()) if a.conditions else None
        if conditions is not None and not isinstance(conditions, dict):
            raise ValueError('Capture conditions must be a JSON object')
        frames, provenance = [], []
        for path in a.input:
            raw, identity = read_frame(path, a.width, a.height)
            frames.append(raw); provenance.append(identity)
        fit_hashes = {v['sha256'] for v in provenance}
        if len(fit_hashes) != len(provenance):
            raise ValueError('Duplicate fit content would bias the frame median')
        table, measurements = fit(frames, a.black)
        validation = []
        seen = set(fit_hashes)
        for path in a.validation_input:
            raw, identity = read_frame(path, a.width, a.height)
            if identity['sha256'] in seen:
                raise ValueError('Validation content duplicates fit or another validation input')
            seen.add(identity['sha256'])
            validation.append({'input': identity,
                               'before': field_metrics(np.maximum(raw.astype(np.int32)-a.black, 0)),
                               'after': field_metrics(corrected_frame(raw, table, a.black))})
    except (ValueError, OSError) as error:
        p.error(str(error))
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_bytes(table.astype('<u2').tobytes())
    report = {'format': 'BGGR-four-phase-7x9-Q12', 'bytes': 504,
              'schema_version': 2, 'fit_inputs': provenance,
              'inputs': [str(v) for v in a.input], 'width': a.width, 'height': a.height,
              'black_dn': a.black, 'measurements': measurements,
              'table_sha256': hashlib.sha256(table.astype('<u2').tobytes()).hexdigest(),
              'tool_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'capture_conditions': conditions, 'capture_conditions_verified': False,
              'assumptions': ['Little-endian uint16 BGGR RAW10 with no stride or header',
                              'Uniform field, fixed exposure/gain/WB, sensor and FPGA LSC disabled',
                              'Matching optics, geometry, Bayer phase and black level for fit and validation'],
              'validation': validation, 'held_out_evaluation_performed': bool(validation),
              'independent_validation_performed': False, 'camera_programmed': False,
              'limitations': ['Content-distinct files do not establish independent capture conditions',
                              'Metrics are offline BLC/LSC predictions; BPC and other ISP stages are not modeled',
                              'No optical acceptance threshold, calibrated profile or hardware validation is asserted']}
    report_path.write_text(json.dumps(report, indent=2, allow_nan=False) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
