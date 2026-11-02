#!/usr/bin/env python3
"""Offline synthetic regression; no capture, camera writes or optical qualification."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from calibrate_fpga_lsc import corrected_frame

# Independently evaluate scalar integer interpolation at every pixel, including
# four phases, right/bottom endpoints, nonzero black and saturation.
rng = np.random.default_rng(8)
raw = rng.integers(0, 1024, (32, 40), dtype=np.uint16)
grid = rng.integers(0, 16385, (4, 7, 9), dtype=np.uint16)
actual = corrected_frame(raw, grid, 31)
for y in range(32):
    for x in range(40):
        cy, cx = y*6*65536//31, x*8*65536//39
        iy, ix = min(cy//65536, 5), min(cx//65536, 7)
        fy, fx = cy-iy*65536, cx-ix*65536
        phase = 2*(y % 2)+x % 2
        mix = sum(int(grid[phase, iy+dy, ix+dx])*wy*wx
                  for dy, wy in [(0,65536-fy),(1,fy)]
                  for dx, wx in [(0,65536-fx),(1,fx)])
        gain = (mix+2**31)//2**32
        expected = min((max(int(raw[y,x])-31,0)*gain+2048)//4096,1023)
        assert actual[y,x] == expected

with tempfile.TemporaryDirectory(prefix='lsc-report-test-') as directory:
    folder = Path(directory)
    source, holdout, output = [folder/n for n in ['fit.raw','heldout.raw','grid.bin']]
    np.full((32,40),512,dtype='<u2').tofile(source)
    np.full((32,40),513,dtype='<u2').tofile(holdout)
    conditions = folder/'conditions.json'
    conditions.write_text('{"synthetic":true,"purpose":"unit test only"}')
    base = [sys.executable,str(ROOT/'tools/calibrate_fpga_lsc.py'),
            '--input',str(source),'--width','40','--height','32']
    def run(extra, success):
        result = subprocess.run(base+extra,capture_output=True,text=True)
        assert (result.returncode == 0) == success, result.stderr
        return result
    run(['--output',str(output),'--validation-input',str(holdout),
         '--conditions',str(conditions)],True)
    report = json.loads(output.with_suffix('.json').read_text())
    assert report['fit_inputs'][0]['sha256'] == hashlib.sha256(source.read_bytes()).hexdigest()
    assert report['table_sha256'] == hashlib.sha256(output.read_bytes()).hexdigest()
    assert report['held_out_evaluation_performed']
    assert not report['independent_validation_performed'] and not report['camera_programmed']
    assert not report['capture_conditions_verified']
    assert report['validation'][0]['after'][0]['mean_dn'] == 513
    assert report['validation'][0]['after'][0]['coefficient_of_variation'] == 0
    original = source.read_bytes()
    run(['--output',str(source)],False)
    assert source.read_bytes() == original
    alias = folder/'hardlink.bin';os.link(source,alias)
    run(['--output',str(alias)],False)
    assert source.read_bytes() == original
    run(['--output',str(folder/'collision.json')],False)
    duplicate = folder/'renamed.raw';duplicate.write_bytes(original)
    run(['--output',str(output),'--validation-input',str(duplicate)],False)
    run(['--output',str(output),'--input',str(duplicate)],False)
    source.write_bytes(original+b'\x00')
    # The previous np.fromfile loader accepted this malformed byte count.
    assert np.fromfile(source,dtype='<u2').size == 32*40
    run(['--output',str(output)],False)
    source.write_bytes(original)
    run(['--output',str(output)],True)
    report = json.loads(output.with_suffix('.json').read_text())
    assert report['validation'] == [] and not report['held_out_evaluation_performed']
print('PASS exact integer LSC model, provenance, held-out reporting, duplicate rejection, byte-count and input preservation')
