# Live FPGA Black Level candidate

## Status and scope

Offline candidate `0.3.11-black-live-dev` adds live `FpgaBlackLevel` writes for
healthy, actively requested RAW10 acquisition with FPGA processing enabled on
recognized processing v9/v10 hardware. No FPGA source or bitstream changes are
required. The v10 bitstream and UserSet7 remain unchanged. Candidate firmware is now running
in RAM; boot firmware was not changed.
A bounded RAM-only scaled RGB acceptance passed after JTAG reconnection; see the
board acceptance section below. Other configurations remain unqualified.

The board rejects writes during pending recovery or UserSet activity. The camera
also requires healthy state, acquisition requested, streaming, sensor STREAMING,
actual sensor RAW10 format, processing enabled and the audited capability mask.
Stop/drain, fault, bypass and legacy hardware therefore reject live writes.
The transport-lock exception applies only to Black Level in this eligible state;
geometry, pixel format, routing, DMA, tables and all other scalar controls retain
their previous locks. `FpgaBlackLevelLiveActive` reports the eligibility state.
UserSet activity always locks the XML node, even if the active flag were stale.

A valid live write performs one 32-bit MMIO write to the shared RAW5 black register
at `0xA00E0028`, then updates the requested configuration readback. It does not
perform SCCB access, poll, restart acquisition, change routes or rewrite tables.
The allowed range is 0 through 1023 DN. Color processing shares this RAW correction;
writing the color core's old black offset would not implement the intended result.
Stopped writes retain their existing staging and AcquisitionStart semantics.
No persistent configuration layout or existing feature address changes.

## Frame semantics and offline evidence

The exported Vitis HLS 2024.2 RAW5 RTL latches black level once per HLS invocation.
A write during a frame therefore cannot change the correction within that frame.
It applies at a subsequent invocation. Pipeline prefetch/backpressure may retain
frames, so this does not promise the next host-delivered frame or a specific GVSP
frame ID. Readback is the requested setting, not frame-tagged applied telemetry.
DMA completion is not used as a frame-boundary surrogate.

`tests/fpga_isp/run_raw_live_black.sh` compiles the actual exported RAW5 Verilog
with Xilinx xvlog/xelab/XSim. It streams two 32x16 frames with input bubbles and
output backpressure and writes the AXI-Lite register halfway through first-frame
input acceptance. Every output pixel, SOF, EOL, KEEP, STRB and stalled output
stability is checked. Runs writing 16 (unchanged baseline), 64, 0 and 1023 all
pass in 2741 cycles; maximum offset correctly saturates pixels to zero.
This is bounded RTL cycle evidence, not a measured camera FPS result.

`tests/color_isp/run_live_black.sh` checks all 32 combinations of the five camera
state gates, one exact RAW-bank write, range endpoints/rejection, legacy and
bypass denial, stopped staging, transport exceptions, other busy locks and v9/v10
controls. Native C tests use UBSan. Generated XML/ROM/URL/header coherence and the
eBUS GenApi schema pass. Aravis evaluates 104 formulas and 39 boundary cases,
including the complete 16-case Black Level lock truth table. The existing GigE
native regression suite passes. Vitis R5 firmware compiles and links successfully.

Evidence: `diagnostics/live-black-offline-20261027/`. The initial v9 native-test
invocation omitted `GIGE_POLYPHASE_SUPPORT=1` and failed at ApplyColor; rerunning
with the required existing test configuration passes. XSim startup fails inside
the restricted sandbox; offline runs outside it pass. Neither failure required
firmware or FPGA logic changes. The R5 build retains existing generated BSP
macro-redefinition warnings.

## Pending hardware acceptance

Keep the current camera reserved for the user's manual UserSet7 check until
explicitly returned. After return, use a volatile RAM deployment with the existing
qualified v10 bitstream and this candidate ELF, keeping the sealed current pair
available for rollback. Do not write boot firmware, Flash or UserSets for this test.

Measure unchanged-baseline versus repeated live 0/64/1023/0 changes on natural
RAW and FPGA RGB preview, including full-input scaled RGB and maximum-payload
RAW. Record accepted/rejected GVCP writes, visible correction changes, complete
frames, payload integrity, FPS, CSI/drop counters and recovery state. Exercise
Stop/drain, restart, bypass, legacy denial where applicable and recovery locks;
restore runtime settings and release ownership. Verify compatibility of loading
existing UserSet7 without resaving it. Report the measured FPS delta and limits;
RTL cycle equality alone does not establish zero effect on camera throughput.

## Bounded board acceptance on 2026-10-27

Full-input scaled640x480 RGB8 passed at14.294076 FPS during live
Black Level updates versus14.294562 FPS unchanged. Each six-second
series recorded86 good frames without integrity, drop or CSI errors. Black1023
produced zero RGB and restored black0 recovered natural imagery after restart.
Transport-locked scalar writes worked; structural and range guards remained active.
Original runtime was restored exactly and ownership released. No Flash/boot/preset
write or FPGA programming occurred. Maximum RAW and recovery lifecycle remain
pending. Evidence: `diagnostics/live-black-board-resume-20261027/`.
