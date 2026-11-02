# Bounded FPGA upscale

## Implementation and status

Firmware candidate `0.3.12-upscale-dev` enables enlargement in the existing
Xilinx Video Processing Subsystem (VPSS) path. The matching processing v8/v9/v10
images already contain six-tap, 64-phase horizontal and vertical scalers. No
production RTL, HLS processing graph, IP configuration or bitstream changes are
needed for this milestone. The previous live Black Level and independent LED
paths remain intact. This is an offline milestone: no camera access, JTAG,
deployment, reset, Flash/boot write or UserSet operation was performed.

`GigeFpgaPolyphase_Plan` now permits at most 4x enlargement independently on each
ROI axis. This is a conservative firmware bound. Both input and final output
remain within2592x1944, with existing RAW-input and PixelFormat alignment rules.
Area interpolation remains downscale-only. Polyphase also supports mixed axes,
for example reducing height while increasing width. Equal geometry bypasses
VPSS, and an unchanged single axis uses explicit identity coefficients.

The color HLS stage never receives an enlargement request. For Polyphase it emits
the ROI unchanged on enlarged axes and applies its existing area prefilter only
where reduction exceeds4:1. VPSS consumes that intermediate geometry and emits
the requested final geometry. The official Xilinx layer-two scaler drivers
program both axes. The programmer rejects invalid dimensions, more than4x
upscale and unprefiltered reduction beyond4:1 before any MMIO write.

Firmware geometry validation and acquisition use the same planner. Unsupported
legacy hardware and Area mode reject enlargement. All structural controls remain
stopped-only. Persistent configuration layout, existing feature addresses and
interpolation enum values are unchanged, so existing profiles retain their
meaning. Two read-only append-only GenICam nodes report the bounded software path:

| Node | Meaning |
|---|---|
| `FpgaUpscaleAvailable` | Installed image has VPSS and this firmware supports bounded enlargement. |
| `FpgaUpscaleMaximum` | Maximum enlargement per ROI axis:4 with VPSS, otherwise1. |

## Operator recipe for a future authorized board test

Stop acquisition and wait until SensorBusy is false. Select a supported RGB/BGR
or FPGA YUV output, enable FPGA processing and scaler, select Polyphase, then set
input/ROI geometry and output Width/Height. For example320x240 input to640x480
RGB is2x in each axis. Set output dimensions before shrinking input dimensions
if an intermediate geometry would otherwise be invalid. Start only after the
complete configuration is valid. All settings apply at AcquisitionStart.

The ROI must fit the selected sensor RAW window. A smaller RAW window is a sensor
crop, so changing input geometry can change field of view. Enlargement interpolates
available pixels; it does not recover lost detail or provide super-resolution.
The2592x1944 final-output bound remains in force, and individual PixelFormats may
have stricter bounds or alignment rules. No new sensor high-rate or binning mode
is introduced by this firmware change.

## Verification and performance limits

`tests/polyphase/run_native.sh` covers production planning/programming, v8/v9/v10
MMIO routing, 4x boundary/range/ROI/mixed axes, read-only capabilities, old hardware
and busy locks. It preserves the official-driver trace comparison and downscale
regression. `test_driver.c` enumerates reduction geometry and sampled enlargement
bounds, including overflow-like arguments and null output pointers. The controls
use AddressSanitizer and UndefinedBehaviorSanitizer.

`tests/polyphase/run_upscale.sh` uses the actual Xilinx VPSS behavioral simulation
and production route RTL. Production register traces must exactly equal traces
from a separately configured official Xilinx driver harness. For two consecutive
frames, an independent NumPy implementation of separable six-tap Q12 convolution
checks every output byte, Q16 coordinate steps, phase selection, edge replication,
rounding and saturation. The bench adds deterministic input bubbles and output
backpressure and checks SOF, EOL, KEEP, STRB and stalled-output stability.
Cases include2x,4x,fractional, single-axis, mixed-axis, maximum-width/height,
downscale and equal-size bypass.

The bench reports total cycles and first/last output cycles, including register
programming and the stated synthetic stream pacing. These are reproducible RTL
cycle measurements, not camera FPS. Upscale emits more pixels: output bandwidth,
sensor timing and Ethernet payload can limit frame rate. Existing firmware paces
FPGA output against its100MB/s transport budget. No claim of universally unchanged
FPS is made. Camera integrity/FPS, optical quality and a loaded-board upscale
acceptance remain separate, unrun gates. Long-duration endurance is postponed.

An unchanged bitstream adds zero physical DSP/BRAM/LUT resources for this software
milestone; it does not establish resource or timing closure for later added IP.
The last routed v10 image used233 of240 DSP at200MHz. This milestone does not
consume that remaining seven-DSP margin. The new firmware ELF is an undeployed
candidate; keep the sealed accepted runtime/rollback pair when planning any future
hardware test.

The evidence bundle records precise results, source and artifact hashes, tool
versions and known harness failures: `diagnostics/upscale-offline-20261101/`.

## Subsequent natural-scene evidence on 2026-11-01

The user-authorized photo collection on0.3.12/FPGA10 completed a centered
640x480 ROI to1280x960 RGB2x case (two Aravis SUCCESS frames), alongside18 total
completed feature cases. This is source-image evidence, not a strict FPS or
packet-integrity qualification. The centered320x240 ROI to1280x9604x case timed
out before its first accepted frame and automatic recovery reached3. Its secondary
Stop/restore access-denied followed the frame timeout; a fresh normal lease after
owner0/ready1/idle0 restored the exact original runtime and released the camera.

The4x mathematical/isolated-RTL bound therefore does not establish real-time
camera acquisition for that geometry. Do not present4x as board-qualified. The
source stream in isolated VPSS simulation honors TREADY; real sensor/CSI buffering
is finite. Burst pacing, buffering, geometry programming and error/recovery must
be investigated from preserved evidence before any fix or new hardware test.
Simply increasing a timeout is not a validated remedy. No root cause is yet proven.
The offline v11 candidate inherits this unresolved acquisition qualification gate.
Originals/settings/tracebacks: `diagnostics/scene-originals-20261101/`.

## Two-stage controlled board follow-up

A strict v10 2x control completed57 receiver frames at14.295498FPS with no
camera CSI/drop/recovery increment. Nominal centered4x repeated its upstream
fault: no GVSP packet or new framebuffer completion,12110 additional CSI
error-bearing polls and one automatic recovery. A separate requested1FPS4x
trial retained the35550ns line period, also produced no packet/frame, and raised
CSI poll count3533 plus one recovery. This rules out a simple average-FPS/VTS
remedy in that configuration; it does not uniquely establish a FIFO root cause.
Neither failing case is accepted, and no timeout threshold was raised.

On v11, two unpaced1280x960 captures lost host UDP packets while camera error
counters stayed zero;1278 missing packets matched1278 host RcvbufErrors in the
instrumented attempt. These are a separate host reception issue. A controlled
20us interpacket-delay profile completed57 2x frames at14.290956FPS without
packet/socket/camera loss and matched v10 bypass pixels exactly. Do not call this
unpaced transport qualification or a4x fix. The v11 tone/history/LED/live-black
suite passed only within its stated paced profile. Original runtime and packet
delay were restored; no persistent UserSet/Flash/boot write occurred.
Evidence and exact scopes: `diagnostics/v11-board-20261101/`.

## November 2 format discriminator

With the same2592x1944 RAW10 sensor timing, centered320x240 ROI and1280x960
output, RGB4x again produced no packets; the retained first CSI error was stream-
line-buffer-full0x00040000. YUYV4x completed57 frames at14.293775 FPS with no new
CSI/drop/recovery/socket errors. Initial RGB2x completed57 frames at14.293395 FPS;
restored-baseline RGB2x matched its pixels. All captures used20 us packet spacing.
See `diagnostics/4x-firstfault-board-20261102/` for exact identity and logs.

This proves a useful format distinction and preserves a bounded4x YUYV case,
not general4x acceptance or an RGB repair. The same output is3 bytes/pixel in RGB
and2 in YUYV. The writer-facing2-byte/clock path and current packer both impose
service ceilings; widening only the writer cannot remove both. A candidate
throughput remedy must jointly verify simultaneous packer dequeue/enqueue,
writer/interface width, partial-row padding, RAW/native bypass and resource/timing
fit. No such hardware change is included in v12 remap integration. Do not hide
this limitation by increasing timeouts, lowering advertised FPS or silently
changing sensor geometry. The earlier finite downstream model still omits the
physical CSI/upstream scheduling and does not localize the first blocked stage.

## Paired PPC4 writer and bounded v13 board result

The isolated continuous-source test reproduces a second limit in the current
RGB packer: ignoring simultaneous dequeue capacity costs15,367 input cycles for
10,240 pixels. A one-line readiness change accepts the same pixels in10,247
cycles with exact bytes and SOF/EOL, including backpressure checks. Fourteen
throughput cases and the existing RGB/BGR regression pass. A2-byte/clock sink
remains the bottleneck, so packer-only replacement is not proposed.

At the recorded35.55 us sensor line interval and200 MHz fabric clock, a1280-wide
RGB4x output requires at least38.4 us of service per source ROI line on the old
2-byte path. RGB2x requires19.2 us and YUYV4x25.6 us. A paired3-byte/clock path
would require25.6 us. This ideal service bound is consistent with board failures
and successes; it does not simulate encrypted upstream scheduling or real DDR.

The isolated PPC4 writer adds1020 LUT/2 BRAM and no DSP versus PPC2. Its256-bit
memory interface through the actual128-bit PS port adds1623 interconnect LUT.
The preliminary combined delta2643 LUT leaves748 before small adapters and
removal of the old downsizer; additive isolated counts do not establish routed fit.

Firmware0.3.15-ppc4-dev is built for signature0x4953500D, capability bit18.
It selectsPPC4, rounds writer row width to4 bytes and stride to32 bytes only for
that signature. True GVSP row length and older-image behavior remain unchanged.
The actual vendor writer ignores TKEEP for row length and writes32-byte memory
words; an isolated66-byte row therefore needs stride96. That is a unit-test case, not
a supported camera size: the camera minimum is160x120, and board testing uses
RGB162x120 (486 payload bytes,488 writer bytes,stride512). Zero-padding and ten vendor-memory
cases pass. Root layout sanitizer tests cover partial/native/RAW rows and buffer
bounds. No optical detail is created by enlargement.

The paired v13 design routes at200 MHz with45832/47232 LUT,126/150 BRAM36
and235/240 DSP. Setup/hold/pulse slack is+0.429/+0.011/+0.145 ns, with zero
failing endpoints or routing errors. The corrected0.3.14/v12 pair remains rollback.

RAM-loaded0.3.15/v13 completed57 RGB4x frames at14.293804 FPS for320x240 input
ROI to1280x960 output, with20 us packet spacing and35.55 us sensor line interval.
No incomplete frames, missing packets, socket drops, CSI errors or recovery were
reported. One saved frame matches the independent six-tap Q12 reference exactly.
Seven saved format/regression frames match their v12 baselines byte-for-byte;
mode-specific FPS remains within the existing0.5% comparison tolerance.

This repairs the reproduced profile, not every4x output geometry. Wider outputs
can still exceed the source-line service budget even with spare average frame
bandwidth. No advertised FPS or timeout threshold changed. Synthetic pixels do
not prove optical improvement or endurance. Final runtime restoration and seal
are tracked in `../diagnostics/ppc4-v13-board-20261102/`.
Evidence: `../diagnostics/rgb-writer-feasibility-20261102/`.
