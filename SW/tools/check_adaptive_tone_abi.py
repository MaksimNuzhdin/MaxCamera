#!/usr/bin/env python3
"""Check frozen firmware register constants against exported Xilinx HLS ABI."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
source=root.parent/'HW/build/hls/camera_adaptive_tone/solution/impl/ip/drivers/camera_adaptive_tone_v1_0/src/xcamera_adaptive_tone_hw.h'
frozen=root/'gige/GigeFpgaAdaptiveRegisters.h'
pattern=r'^#define\s+(XCAMERA_ADAPTIVE_TONE_\w+)\s+(\w+)\s*$'
a=dict(re.findall(pattern,source.read_text(),re.M))
b=dict(re.findall(pattern,frozen.read_text(),re.M))
assert a and a==b,'Adaptive firmware ABI differs from exported HLS'
print('PASS frozen adaptive ABI matches exported HLS:',len(a),'constants')
