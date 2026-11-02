# Offline lens-shading fit and evaluation

This workflow consumes existing owner-supplied uniform-field captures. It does
not acquire images, write camera settings, save a UserSet or qualify optics.
Training datasets remain deferred. See [ISP capabilities](ISP_CAPABILITIES.md).

Inputs must contain exactly width * height * 2 bytes: little-endian uint16 BGGR
samples in 0..1023, without headers, padding or PFNC10p packing. Dimensions must
be even and at least 32. Matching optics, geometry, Bayer phase and black level,
uniform illumination, fixed exposure/gain/WB and disabled sensor/FPGA LSC are
assumptions the tool cannot verify. Supply known capture conditions as an optional
JSON object; do not invent missing values.

From the project root, with captures that already exist:

```sh
python3 tools/calibrate_fpga_lsc.py \
  --input /path/to/fit.raw --width 640 --height 480 --black 31 \
  --validation-input /path/to/heldout.raw \
  --conditions /path/to/conditions.json --output /path/to/grid.bin
```

Repeat either input option for distinct files. Omit validation inputs when none
exist. Identical content is rejected across and within fit/validation groups,
including renamed copies. Different hashes do not prove independent capture
conditions. Validation frames are never used to fit the table.

The existing 504-byte four-phase 7x9 Q12 table format is preserved. Its adjacent
JSON report records absolute input paths, byte counts and SHA256 hashes, table
and tool hashes, black level, supplied conditions and explicit limitations.
Output destinations must differ and cannot alias source evidence, including
existing hardlinks. Preserve the table, report and captures together.

Optional held-out evaluation predicts BLC followed by the implemented Q16
bilinear interpolation, Q12 rounding and RAW10 saturation. Before/after metrics
include per-phase mean, coefficient of variation, percentile spread and saturated
fraction. BPC and the remaining ISP are not modeled. Check saturation as well as
spread: clipping can misleadingly make a field look uniform.

`held_out_evaluation_performed` means offline metrics were computed.
`independent_validation_performed` remains false: no optical threshold, lighting
verification, hardware application or independently qualified capture is implied.
`camera_programmed` remains false. Fit-only measurements are not held-out results.

Verification:

```sh
python3 tests/camera_isp/test_lsc_calibration.py
python3 tests/calibration/test_lsc_reporting.py
```

Tests cover integer interpolation across phases/edges, saturation, provenance,
content leakage, malformed trailing bytes and input preservation. Temporary
synthetic fixtures are not camera calibration data.

Next gate: suitable existing fit and independently acquired validation fields,
known capture conditions and agreed optical criteria. This change does not assert
that such a real pair exists. Camera table application and persistence acceptance
remain separate; v9 hardware and its save/load limitations are unchanged.
