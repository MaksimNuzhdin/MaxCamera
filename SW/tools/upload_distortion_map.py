#!/usr/bin/env python3
"""Validate a compiled map offline; explicitly upload only to an idle supported camera.

No acquisition, reset, persistent settings or geometry changes are performed.
An existing volatile map is never overwritten. Upload errors are not retried as
new transactions; the shared GVCP client only retries the same request ID.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import time
import zlib
from gige_feature_catalog import FEATURES

REG = {f['key']: f['address'] for f in FEATURES}


def read_map(directory):
    directory = Path(directory)
    report = json.loads((directory/'report.json').read_text())
    data = (directory/'map.u32le').read_bytes()
    w, h = report['width'], report['height']
    if type(w) is not int or type(h) is not int or not 1 <= w <= 640 or not 1 <= h <= 480:
        raise ValueError('Unsupported map dimensions')
    if len(data) != w*h*4 or hashlib.sha256(data).hexdigest() != report['sha256']:
        raise ValueError('Map length or SHA256 mismatch')
    crc = zlib.crc32(data)
    if crc != report['crc32']:
        raise ValueError('Map CRC32 mismatch')
    for index, (word,) in enumerate(struct.iter_unpack('<I', data)):
        x, y = word & 1023, (word >> 10) & 511
        if word & 0x7ff80000 or (word and not word & 0x80000000):
            raise ValueError(f'Invalid encoding at word {index}')
        if word & 0x80000000 and (x >= w or y >= h or abs(y-index//w) > 7):
            raise ValueError(f'Invalid coordinate at word {index}')
    return report, data


def upload(control, report, data, enable=False, clock=time.monotonic, wait=time.sleep):
    """Caller holds exclusive control; geometry and runtime remain unchanged."""
    read = lambda key: control.read(REG[key])
    write = lambda key, value: control.write(REG[key], value)
    expected = {12: 262143, 13: 524287}.get(read('FPGA_VERSION'))
    if expected is None or read('FPGA_CAPABILITIES') != expected or read('REMAP_AVAILABLE') != 1:
        raise RuntimeError('Requires matching validated v12/v13 remap firmware and FPGA')
    if control.read(0x10018) or control.read(0x1001c) or read('SENSOR_BUSY'):
        raise RuntimeError('Camera must be stopped, drained and unlocked')
    if not read('SENSOR_READY') or not read('FPGA_ENABLE'):
        raise RuntimeError('Initialized FPGA color route is required')
    if (control.read(0x10000), control.read(0x10004)) != (report['width'], report['height']):
        raise RuntimeError('Map dimensions must match the current output')
    if read('REMAP_STATE') != 0 or read('REMAP_ENABLE'):
        raise RuntimeError('Existing volatile map preserved; abort it explicitly before replacing')
    write('REMAP_WIDTH', report['width']);write('REMAP_HEIGHT', report['height'])
    write('REMAP_EXPECTED_CRC', report['crc32']);write('REMAP_BEGIN', 1)
    # Explicit indices make repeated packets idempotent. Each packet is512 bytes.
    words = [word for (word,) in struct.iter_unpack('<I', data)]
    for start in range(0, len(words), 32):
        pairs = []
        for index in range(start, min(start+32, len(words))):
            pairs.extend((REG['REMAP_INDEX'], index, REG['REMAP_DATA'], words[index]))
        reply, _ = control.command(0x82, struct.pack('!'+len(pairs)*'I', *pairs))
        if reply != struct.pack('!HH', 0, len(pairs)//2):
            raise RuntimeError('Incomplete register-write acknowledgement')
    write('REMAP_COMMIT', 1)
    end = clock()+30
    while True:
        state = read('REMAP_STATE')
        if state == 3:
            break
        if state != 2 or clock() >= end:
            raise RuntimeError(f'Map validation did not finish: state={state}, error={read("REMAP_ERROR")}')
        wait(.01)
    if read('REMAP_UPLOADED') != len(words) or read('REMAP_CHECKED') != len(words):
        raise RuntimeError('Map validation count mismatch')
    if enable:
        write('REMAP_ENABLE', 1)
        if read('REMAP_ENABLE') != 1:
            raise RuntimeError('Enable readback mismatch')
    return dict(sha256=report['sha256'], crc32=report['crc32'], words=len(words),
                enabled=enable, optical_validation=False, persistence=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--upload', action='store_true')
    parser.add_argument('--ip')
    parser.add_argument('--bind', default='0.0.0.0')
    parser.add_argument('--enable', action='store_true')
    args = parser.parse_args()
    report, data = read_map(args.directory)
    if not args.upload:
        print(json.dumps(dict(validated=True, uploaded=False, sha256=report['sha256'])))
        return
    if not args.ip:
        parser.error('--upload requires an explicit --ip')
    from gige_probe import Control
    control = Control(args.ip, args.bind)
    acquired = False
    try:
        if control.read(0xa00) != 0:
            raise RuntimeError('Another controller owns the camera')
        control.write(0xa00, 2);acquired = True
        print(json.dumps(upload(control, report, data, args.enable), indent=2))
    finally:
        try:
            if acquired:
                control.write(0xa00, 0)
        finally:
            control.sock.close()


if __name__ == '__main__':
    main()
