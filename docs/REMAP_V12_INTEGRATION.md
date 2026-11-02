# Remap v12 integration contract and open gates

The authorized correction profile is same-canvas RGB8 nearest-neighbor at most
640x480 with source rows within +/-7 and black border. Disabled correction must
preserve existing full-resolution/native/raw behavior; this profile is not a
restriction on the rest of the camera. No new candidate has been loaded.

Proposed graph: clone qualified v11, insert integer_remap after the VPSS route and
before CLAHE. All existing stages remain. Extend tone_control to three masters,
retaining CLAHE/adaptive addresses and assigning remap0xA0190000 on M02. Proposed
v12 signature is0x4953500C, added capability131072. Final exported AXI-Lite register
offsets must be pinned from the generated HLS header before firmware ABI closure.

Map DDR master: add S01 to the existing axi_interconnect_1, preserving writer S00
and the existing M00 connection to PS S_AXI_HP1_FPD. This avoids changing PS DDR
configuration or the qualified writer's interface/address. HLS map AXI data/address
width, interconnect arbitration cost and all new address mappings need actual
export/BD validation. Clock remains200MHz. Sharing this port still needs bounded
DDR arbitration/backpressure and FPS tests; average bandwidth is not that proof.

## Single-invocation lifecycle

The coordinator's per-frame, non-auto-restart proposal is structurally suitable
for the current ap_ctrl_hs core, subject to the following verification gates:

- Never set remap auto_restart. Program/configure while idle and lease/flush an
  immutable map bank before the first start. Arm the DMA destination before
  starting remap, once per corresponding color frame. Native/raw routes do not
  start the color remap path.
- On DMA completion, keep the completed buffer pinned and wait nonblocking for
  remap AP_IDLE. Preserve the original deadline; checking/remedying a stuck core
  must not skip the existing timeout. Read final status before another start can
  clear it, reject an erroneous frame before transport submission, and only then
  rearm. Handle status faults during a stop/drain too.
- Normal stop must drain the in-flight sensor/color/remap/writer frame before
  gating its source, resetting video, changing configuration or releasing the
  map lease. Starting a new acquisition must first prove old remap idle. A pending
  operation cannot be treated as idle merely because the GVCP owner disappeared.
- AP_IDLE must be verified to imply no outstanding map AXI reads for this exported
  implementation, including partial bursts, repeated starts and AXI stalls. The
  existing eight-call co-simulation is functional evidence, not that complete
  quiescence/reset assertion. Add explicit AR/R-last accounting or equivalent
  verification before accepting a master-only video reset.
- If AP_IDLE/quiescence is not attained, retain the map lease and reject video-only
  setup/soft recovery. A qualified system reset must reset the master and PS DDR
  transaction path together. Do not leave stale responses in the shared HP path.

After quiescence is established, matching the existing active-low stream reset
for remap is a candidate wiring choice, not permission to reset an active AXI
reader. The shared memory interconnect/PS reset must remain unchanged in normal
video reset. A separate PS/control reset domain alone would not fix an unfinished
stream invocation or define safe map lifetime.

Manual per-frame control adds scheduling latency. Preserve the sensor cadence and
all supported bypass geometries, measure the arm-to-start gap and complete frame
FPS, and verify that CSI storage absorbs this gap. No no-impact FPS claim is made
in advance. No true HDR/optical calibration claim follows from this integration.

## Current gate order

1. Revalidate frozen enabled-profile/native-bypass Csim and ten short RTL calls.
2. Export exact ABI and run actual isolated Vivado synthesis at200MHz.
3. Evaluate core plus control/DDR arbitration/glue against v11 free resources.
4. Only if supported, build the v11-derived complete v12 candidate and check all
   setup/hold/pulse, routing/DRC, resource, address/reset and preserved-path gates.
5. Hardware qualification must use matching firmware and the agreed lifecycle,
   with exact pixels, frame rejection, map swap/drain, reset and FPS coverage.

Firmware/control/map lifecycle and their GenICam integration remain owned by the
coordinator. This document proposes FPGA wiring/verification and does not mutate
those source files or certify the unbuilt combined design.

## Exported ABI from the frozen native-bypass core

Generated header: build/remap-v12-ip/project/solution/impl/ip/drivers/
integer_remap_v1_0/src/xinteger_remap_hw.h. Offsets from proposed0xA0190000:

| Register | Offset | Semantics |
| --- | --- | --- |
| AP control |0x00|start bit0, done bit1 clear-on-read, idle bit2, ready bit3 clear-on-read; auto_restart bit7 must remain0 |
| Map pointer low/high |0x10/0x14|64-bit pointer |
| Rows |0x1C|32-bit |
| Columns |0x24|32-bit |
| Enabled |0x2C|bit0 |
| Final status |0x34|32-bit read-only; current invocation writes0 on entry |
| Status valid |0x38|bit0 clear-on-read |

GIE/IER/ISR are0x04/0x08/0x0C. Do not treat status-valid as a persistent error
latch. Read status only after the matching non-auto-restart invocation is idle,
then classify/drop before starting the next frame. Enabled geometry remains
640x480, disabled geometry allows2592x1944. Exported map AXI is32-bit data with
64-bit address; restrict map leases to the actually mapped PS DDR range and
validate address conversion rather than silently truncating pointers.

## First board candidate and VPSS qualifier correction (2026-11-02)

The first v12 image (`229d735c1bccbbe965953c7a2bdc6b690d690304567744e1a05327d4951b161d`)
passed disabled-remap byte comparison against v11:57 frames at14.293541FPS,
with no missing packets, CSI errors, dropped frames or recovery. The first enabled
identity acquisition failed: one completed frame was rejected, no GVSP frame was
published, and automatic recovery cleared the hardware status before readback.
That candidate is not qualified. No translated-map acquisitions followed it.

A read-only DDR dump preserved the rejected921600-byte RGB frame. It matches the
v11 test-pattern baseline exactly (SHA256
`01270211bf55f8ebc5d6182f32bafe3695940e6fee3638b4f673af24ba889464`). The first256
bytes of active map bank0 also match the identity upload. Bank1 was inactive;
its mismatch is not evidence of DDR corruption. Correct pixel data does not
turn the failed acquisition into a pass or recover the lost first-error status.

Generated VPSS RTL drives video TSTRB zero, while its HLS conversion leaves it
unspecified. Remap deliberately validates full pixel qualifiers. The route now
sets scaled-path TSTRB from TKEEP, because every kept RGB byte is data. It
preserves TKEEP, SOF, EOL and all bypass qualifiers. The remap framing check is
unchanged. A dedicated Xilinx RTL test covers zero and unknown vendor TSTRB,
malformed KEEP, framing preservation, bypass and reset. A native remap test
confirms malformed KEEP/SOF/EOL still return REMAP_FRAMING with exact pixels.
These tests do not establish hardware FPS for the corrected image.

The failed candidate was rolled back in RAM to qualified v11. A57-frame capture
then passed at14.2932FPS with exact baseline bytes and zero packet loss, CSI
errors, dropped frames and recovery. Original runtime and selectors were
restored. No Flash, boot image or UserSet save was performed. The corrected
candidate is built separately in `build/fpga-color-v12-qualifier`; first-candidate
artifacts and failures remain preserved.

## Corrected v12 bounded board acceptance (2026-11-02)

The VPSS qualifier-corrected image passes full implementation gates:
43841LUT,47211FF,124BRAM36,235DSP; WNS+0.477ns, WHS+0.010ns,
WPWS+0.145ns; zero failing endpoints;88667/88667 routed nets; no routing
errors or DRC errors/critical warnings. Six PS initialization files remain
byte-identical to qualified v11. All11 VPSS scaling RTL cases pass independent
pixel comparisons with strict, unknown-sensitive qualifier assertions.

| Board case | Good frames | Delivered FPS | Pixel oracle |
| --- | ---: | ---: | --- |
| Disabled,640x480 |57|14.293313|Exact v11 RGB bytes |
| Disabled,2x to1280x960 |57|14.293009|Exact v11 RGB bytes |
| Disabled,2592x1944, requested1FPS |6|0.999946|Exact v11 RGB bytes |
| Enabled identity |57|14.293566|Exact baseline RGB bytes |
| Identity stop/restart, same map |57|14.293297|Exact baseline RGB bytes |
| Inverse translation+5/+7 |57|14.294451|Independent NumPy shift/black borders |
| Inverse translation-5/-7 |57|14.293981|Independent NumPy shift/black borders |
| Stopped disable/bypass |57|14.295083|Exact baseline RGB bytes |

All listed captures have zero bad/missing/duplicate/malformed packets, socket
receive drops, new camera drops, CSI errors, recovery and reader status. Some
captures contain reordered packets that the strict receiver reassembles without
loss; individual counts are retained in `remap-summary.json`. Small/full profiles
use40us packet spacing,2x uses20us. These short captures show no observed FPS
reduction; they do not qualify endurance or maximum native-resolution FPS.

Checksum mismatch is rejected before acquisition and cannot be enabled. Active
map mutation and read-only status writes are rejected. A stopped geometry change
invalidates the ready map and disables correction. All15 named GenICam nodes are
present on the camera; Boolean/Integer/Enumeration reads, Abort command, stopped
disable and empty-map enable rejection pass through named GenICam access.
Bulk map transfer and capture control are also exercised through GVCP registers.

Existing-feature regressions pass on the corrected pair: adaptive tone high/low
and repeated invocations match the integer oracle and first-frame bypass exactly;
live black level changes pixels and retains protected-control rejection;
logical stream LED blinks during acquisition and stops afterward. Captures range
from14.293030 to14.294184FPS, with no new camera errors/drops/recovery. LED scope
is logical state only, not an emitted-light or physical GPIO-register observation.

Final comparison verifies every original runtime field/table/selector, persistent
selection/default/mask and all camera identity bytes except the explicitly
expected firmware-version string. The first overly broad final snapshot check
included intentional firmware/capability changes and is retained as checker
failure evidence; it was not an acquisition failure. Camera is Ready, idle,
controller-free, remap empty/disabled, reader status0 and error/recovery counts0.
UART termios is restored and owned UART/JTAG-server sessions are closed.

The bounded-qualified corrected pair remains running in RAM:
bitstream SHA256 `720ed91c2c6a5fe2c180aa60f1b02e32a6095dad33e532aebddeb8a2bdd82d3f`,
XSA `cf0fd2d4f9025695b0bd694880a447e5eae7559b0b4384579076ed066b1ab393`,
firmware `ca156be0832d5c8917fccbed36e85236537202bbd1253524240ca2185419e0c9`
(version0.3.14-remap-dev). No Flash, boot image or UserSet save was performed.
Evidence is in `diagnostics/remap-v12-qualifier-offline-20261102` and
`diagnostics/remap-v12-qualifier-board-20261102`. Real optical calibration,
long-duration qualification, HDR exposure association, trained classification
accuracy and the separate RGB4x downstream-pressure remedy remain outside this
bounded remap acceptance.
