#!/usr/bin/env python3
"""Offline RAW10LE plateau screening; never certifies effective exposure or HDR."""
import argparse
import hashlib
import json
from pathlib import Path
import statistics
import struct


def analyze(manifest, root):
    """Check pinned captures and report per-CFA-site response without association."""
    root = Path(root).resolve()
    if manifest['schema'] != 1 or manifest['format'] != 'RAW10LE16':
        raise ValueError('Requires schema1 unpacked little-endian RAW10')
    w, h = manifest['width'], manifest['height']
    if type(w) is not int or type(h) is not int or not 2 <= w <= 2592 or not 2 <= h <= 1944:
        raise ValueError('Invalid geometry')
    black, white = manifest['black'], manifest['white']
    if not 0 <= black < white <= 1023:
        raise ValueError('Invalid screening thresholds')
    if manifest['manual_exposure'] is not True or manifest['manual_gain'] is not True:
        raise ValueError('Automatic exposure/gain invalidates plateau comparison')
    for key in ('firmware_sha256', 'bitstream_sha256'):
        value = manifest[key]
        if len(value) != 64 or any(c not in '0123456789abcdef' for c in value):
            raise ValueError('Missing pinned image identity')
    x, y, pw, ph = manifest['patch']
    if any(type(v) is not int for v in (x,y,pw,ph)) or min(x,y)<0 or min(pw,ph)<2 or x+pw>w or y+ph>h:
        raise ValueError('Invalid patch')
    tolerance = manifest['maximum_relative_drift']
    if not 0 < tolerance < 1:
        raise ValueError('Explicit drift screening threshold required')
    last_sequence = last_time = None
    seen = set()
    plateaus = []
    baseline = None
    for plateau in manifest['plateaus']:
        lines, gain = plateau['commanded_exposure_lines'], plateau['gain16']
        if type(lines) is not int or not 1 <= lines <= 65535 or type(gain) is not int or not 16 <= gain <= 1023:
            raise ValueError('Invalid exposure request')
        if baseline is not None and gain != baseline['gain16']:
            raise ValueError('Keep analog gain fixed for this experiment')
        if len(plateau['frames']) < 3:
            raise ValueError('At least three recorded frames per plateau required')
        sites = [[] for _ in range(4)]
        clipped = 0
        for frame in plateau['frames']:
            sequence, timestamp = frame['capture_sequence'], frame['host_receive_monotonic_ns']
            if type(sequence) is not int or type(timestamp) is not int or min(sequence,timestamp)<0:
                raise ValueError('Invalid sequence/time')
            if last_sequence is not None and (sequence != last_sequence+1 or timestamp <= last_time):
                raise ValueError('Missing/reversed frame or nonmonotonic receipt time')
            if frame['configuration_generation'] != manifest['configuration_generation']:
                raise ValueError('Configuration changed')
            path = (root/frame['file']).resolve()
            if not path.is_relative_to(root) or path in seen:
                raise ValueError('Capture path escapes root or is reused')
            seen.add(path)
            data = path.read_bytes()
            if len(data) != w*h*2 or hashlib.sha256(data).hexdigest() != frame['sha256']:
                raise ValueError('Capture length/hash mismatch')
            samples = [v[0] for v in struct.iter_unpack('<H', data)]
            if any(v > 1023 for v in samples):
                raise ValueError('Out-of-range RAW10 sample')
            for site in range(4):
                patch = [samples[row*w+col] for row in range(y,y+ph) for col in range(x,x+pw)
                         if ((row & 1)*2+(col & 1)) == site]
                clipped += sum(v <= black or v >= white for v in patch)
                sites[site].append(statistics.median(patch)-black)
            last_sequence, last_time = sequence, timestamp
        means = [statistics.median(values) for values in sites]
        drifts = [(max(values)-min(values))/mean if mean>0 else None for values,mean in zip(sites,means)]
        result = dict(name=plateau['name'], commanded_exposure_lines=lines, gain16=gain,
                      site_medians_above_black=means, site_relative_drift=drifts,
                      clipped_patch_samples=clipped,
                      screening_passed=clipped==0 and all(v is not None and v<=tolerance for v in drifts))
        if baseline is not None:
            result['commanded_energy_ratio_to_first'] = lines/baseline['commanded_exposure_lines']
            result['observed_site_ratio_to_first'] = [a/b if b>0 else None for a,b in zip(means,baseline['site_medians_above_black'])]
        else:
            baseline = result
        plateaus.append(result)
    if not plateaus:
        raise ValueError('At least one plateau required')
    return dict(schema=1, captures_valid=True, plateaus=plateaus,
                screening_passed=all(p['screening_passed'] for p in plateaus),
                effective_exposure_verified=False, hdr_qualified=False,
                timestamp_scope='Host monotonic receipt only; not sensor SOF or effective exposure',
                limitation='Drift and response screening only; thresholds and patch are declared assumptions. No automatic settling count or metadata association.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    args = parser.parse_args()
    print(json.dumps(analyze(json.loads(args.manifest.read_text()), args.manifest.parent), indent=2))


if __name__ == '__main__':
    main()
