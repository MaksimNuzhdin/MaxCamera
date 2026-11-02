# Bounded paired remap acceptance plan

The general remaining RAM/FPGA test authorization applies. The first v12 image
passed implementation gates but failed enabled identity acquisition on hardware.
It has been rolled back to qualified v11. The qualifier-corrected FPGA must pass
its own routed gates before a new paired load. Use the sealed first-error logging
ELF `firmware-0.3.14-remap-error-log.elf`, SHA256
`ca156be0832d5c8917fccbed36e85236537202bbd1253524240ca2185419e0c9`, under
`diagnostics/remap-v12-contract-review-20261102`. The superseded ff9085 firmware
was never loaded; the failed first v12 run used the AP_DONE latch fix bb67bbdd.
No Flash/boot/UserSet save or publication is involved.

## Baseline and paired images

Refresh exclusive owner/ready/idle state and full volatile runtime/table/identity
snapshot on restored v11. Preserve qualified v11 rollback. Capture deterministic
Square test pattern with rolling/transparent/monochrome off, manual exposure100
and gain16; disable CCM/gamma/AWB/median/bilateral/sharpen/CLAHE/adaptive tone for an
independent output-pixel oracle. Confirm stable horizontal and vertical variation.
If the pattern does not provide that evidence, do not claim a vertical-map oracle.

Use40 us packet spacing for small/full profiles and20 us for the2x ROI profile.
The initial20 us small-frame baseline dropped host socket packets and remains
preserved as failed evidence. The40 us baseline passed at unchanged14.293FPS.
Save v11 references for full-field640x480 RGB, centered
640x480 ROI ->1280x960 RGB, and full2592x1944 color-route RGB. The last profile uses
explicit requested1 FPS with actual rate/VTS/row-period readbacks, since a15 MB
frame at40 us spacing cannot sustain the small-frame cadence. This diagnostic
profile does not lower an advertised limit or qualify native maximum FPS.

Load only the routed v12 plus the corrected sealed0.3.14 ELF after lifecycle
review. Receive-only115200 UART must capture known startup. Check v12 signature,
capability262143, exported control ABI and GenApi node availability. Six PS init
files match v11 exactly. A failed load/identity/readiness must not trigger an
uncertain duplicate; retain primary evidence and use the qualified rollback.

## Functional sequence

- Repeat the three disabled-remap references. Require exact final-frame bytes,
  metadata, zero new CSI/drop/recovery and stated FPS comparison at identical
  requested rates. The native RGB test must actually use the FPGA color route,
  not native YUV/raw routing which bypasses the new core.
- On stopped640x480, upload one valid map payload with intentionally wrong expected
  CRC; require checksum rejection and enable refusal without starting capture.
  Abort only the test-owned staging state, never another user's map.
- Upload validated synthetic identity, enable and capture. Compare every RGB byte
  against the disabled baseline; retain map SHA256/CRC, state/progress/status and
  source geometry. This is transport/remapping evidence, not lens calibration.
- Stop/drain and explicitly replace the test-owned map with known +5/+7 inverse
  translation, then -5/-7 if clean. Independent NumPy reference shifts the captured
  input and fills out-of-image borders with black. Compare every pixel, including
  edge rows/columns. All maps stay within the approved profile.
- Exercise restart with the same ready map, stopped disable/bypass, active-write
  rejection, read-only status rejection and structural geometry invalidation.
  No source changes, timeout relaxation or retries until green on hardware failure.

For each acquisition retain exact state/counters, strict receiver CSV/stderr,
UART boundaries and data. Preserve frame rejection/stop failures separately from
primary receive/core errors. Any actual fault ends feature expansion; drain or
conditionally use a qualified system reset/rollback if reader quiescence cannot
be established. Do not reset only an active AXI master.

## Completion

Restore all original runtime fields/tables and persistent selection/default/mask,
stop and release the controller, close the owned serial/hardware-server sessions,
and record exact running bit/ELF identities. Selection of a qualified new RAM pair
must be explicitly reported; otherwise restore the sealed v11 pair. A passing
bounded run does not close real lens calibration, endurance, HDR exposure
association or the separate RGB4x downstream-pressure remedy.
