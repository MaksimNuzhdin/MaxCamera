# FPGA ISP control adapter

Sources: `gige/GigeFpgaIsp.*`, `GigeFpgaPolyphase.*`, `GigeFpgaOutput.*`,
`GigeFpgaContrast.h` and generated/frozen register headers. The adapter separates
GenApi virtual addresses from physical AXI-Lite addresses and validates image
signature/capabilities before accessing optional banks.

`GigeFpgaIsp_Init` uses supplied read/write callbacks; `Read`/`Write` expose
validated controls. `Geometry` plans input/ROI/output dimensions; `ApplyOutput`
programs a stopped pipeline; `Disable` selects bypass and clears software stats.
Import checks capabilities before accepting a preset. Do not call raw MMIO with
a GenICam register address.

## Lifecycle and compatibility

v10 signature0x4953500A includes independent LED/reset. v11 signature0x4953500B
adds adaptive tone at0xA0180000; firmware must not touch that bank on v10.
Generated HLS ABI constants are checked against the exported IP. Structural
geometry/format/LUT controls stay stopped-only. Live Black Level is a separate
health/route-gated operation; see its guide for conditions.

Polyphase supports bounded per-axis upscale up to4x in the new firmware; Area
remains reduction-only. The planner prefilters large reductions before VPSS.
Adaptive-tone enable/target are stopped-only; history epochs reset on new setup.
Status mean/gain are unlatched invocation telemetry, not an atomic frame-tagged
measurement. Old presets default the new feature off.

Reproduce native checks with `bash tests/polyphase/run_native.sh` and
`bash tests/adaptive_integration/run_native.sh` in the configured workspace.
These use mock MMIO. Full routing and camera acceptance are separate gates;
consult [feature guide](../../ISP_FEATURES.md) and sealed evidence.
