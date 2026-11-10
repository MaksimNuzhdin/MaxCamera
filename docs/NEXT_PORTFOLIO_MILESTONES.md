# Remaining portfolio milestones after bounded v11 acceptance

Current-status note, November10,2026: the dated plans below retain their original
scope. The later A53 analytics and same-IP Ethernet web implementation is now
bounded-camera tested. Publication of the current update is authorized. See
[Project status](PROJECT_STATUS.md) and [MicroSD boot](MICROSD_BOOT.md) for the
current implemented/open items; do not interpret an older research-only or
publishing-pending statement as the current feature state.


The v11 board bundle is sealed and independently verified (112 files). Eight
cases delivered 500 complete frames at approximately 14.293 FPS with 20 us packet
spacing. This does not close 4x enlargement, distortion correction or true HDR.
No additional hardware session or candidate deployment is implied by this plan.

## 1. Reproduce the 4x failure in the complete finite-buffer path

Next executable offline milestone: extend the existing scaler simulation around
the actual v10 crop/scaler/packing/downsize/writer-facing graph, driving the saved
2592x1944 sensor geometry, centered 320x240 ROI, 1280x960 RGB output and 35,550 ns
row cadence. Model a physical source that cannot obey downstream TREADY forever;
use the actual CSI buffering contract and generated HLS/IP FIFO depths rather
than an invented queue capacity. Preserve the successful 640x480 ROI 2x control.

Acceptance: identify the first overflow, invalid metadata or deadlock condition,
with cycle/row position and bounded occupancy traces. A fix must eliminate that
condition and preserve exact pixels/metadata in both geometries under the same
input cadence. The former isolated VPSS source stalls on TREADY, so its passing
oracle does not satisfy this milestone. Do not increase timeout or lower nominal
FPS to call the existing mode fixed.

The downstream two-byte/200 MHz service limit gives a plausible row-pressure
hypothesis (38.4 us demand versus 35.55 us source period). It is not yet a causal
proof. Candidate remedies to evaluate only after reproduction are bounded added
elastic storage before the expansion bottleneck, or wider writer service. Both
need full-design timing/resource checks. A DDR staging path is a larger fallback,
not a small register unlock. No firmware-only correction is proven at present.
No user input is needed for this offline milestone. A resulting new FPGA candidate
needs a separate bounded RAM-load test authorization; the v11 acceptance is done.

The first boundary probe has now completed both geometries with exact independent
Q12 pixels and identical packed writer bytes. Peak unserved demand was 315 RGB
pixels at 2x and 5,561 at 4x; only 4x missed a following-row service deadline.
This counter has no capacity limit. Physical CSI timing, full upstream scheduling
and DDR stalls remain absent, so these results neither reproduce the physical
overflow nor establish a required FIFO size. Independent review and source hashes
are in `diagnostics/offline-followup-20261101/`.

## 2. Bound distortion correction before integrating it

The generic Brown-Conrady inverse-map generator is tested; the camera has no
map DMA/remapper. Next offline deliverable is a small fixed-mode remap prototype
with explicit coordinate/border rules, independent pixel oracle, AXIS stall tests
and actual synthesis cost. Use synthetic maps only to validate arithmetic and
architecture, never as lens calibration evidence. Prove that its displacement
window fits the remaining memory and DSP budget before changing the production
processing graph.

Current v11 leaves 5 DSP and 42.5 BRAM36 tiles. Even all remaining raw BRAM stores
only about 25 full-width RGB rows before banking/control overhead. Full-resolution
float X/Y maps need 40,310,784 bytes and cannot be hidden in that BRAM budget.
Thus arbitrary full-frame calibrated remapping is not a safe drop-in stage.
Map DMA, row scheduling and border validity must be implemented and budgeted;
removing working ISP stages or moving processing to the receiver is not assumed.

The isolated 640x480 nearest-neighbor, 16-row remapper now passes C simulation
and HLS synthesis. Sequential raster indexing reduced its estimate from 14 to
4 DSP and from 5,767 to 5,472 LUT, with 32 BRAM18 blocks unchanged. Only 54
nominal LUTs remain against the v11 budget before map DMA, bypass and integration.
The 3.737 ns HLS estimate also exceeds the 3.650 ns effective scheduling budget
(5 ns clock minus 1.35 ns uncertainty). Neither combined fit nor frame throughput
is proven. Original and optimized evidence are separately preserved in
`diagnostics/remap-synthesis-20261101/` and `diagnostics/remap-wrapper-20261101/`.
Do not add this float-map prototype to production based on C simulation alone.
Further work needs a lower-cost coordinate/reader architecture or measured
whole-design margin; lens calibration alone would not resolve this resource gate.

One lower-cost architecture is now implemented in `experiments/remap_integer/`:
precomputed integer nearest coordinates, a16-row ring with explicit +/-7-row
support, exact bypass and a DDR map reader. Full Csim and eight short RTL/AXI
co-simulation transactions pass. HLS estimates0 DSP,4796 LUT and34 BRAM18, leaving
730 nominal LUT before graph integration. This remains a640x480 isolated profile,
not a silently reduced production feature. Next gates are actual Vivado cost,
maximum-geometry/stall/reset RTL tests, combined routing, immutable map/control
integration and explicit adoption of the bounded profile before camera testing.

Lens-specific completion needs calibrated intrinsics/distortion for the actual
AN5641 lens, focus and sensor mode, or a later authorized calibration capture.
The existing PCB scene cannot provide those values. No dataset/training task is
reintroduced. A smaller supported mode can be proposed only with explicit limits,
not silently advertised as full-resolution correction.

## 3. Integrate true HDR only with effective exposure and frame leases

The offline RAW10 merge validator is complete; it is not a camera HDR feature.
Next software boundary is a stopped/SingleShot bracket state machine that reserves
immutable frame leases, associates verified effective exposure and gain with each
frame, rejects unsettled/mismatched pairs, and cancels without blocking GVCP.
First inspect the current grouped sensor-write/settling contract and DDR map;
do not mark cached exposure readback as per-frame truth. No extra core is assumed
available or booted by this change.

The isolated session and bounded merge worker are implemented and tested in
`experiments/hdr/`, with sealed evidence in `diagnostics/hdr-session-20261101/`.
They preserve DMA/consumer leases during cancellation, reject stale generations
and unverified exposure metadata, and divide validation/merge work into bounded
pixel batches. Sanitizer, C/C++ linkage and R5 object-compilation checks pass.
These modules are not wired into production acquisition: the current sensor
group-write acknowledgement does not establish the effective exposure of a
particular frame, and the six-buffer rotation does not implement these leases.
No safe fixed discard count or R5 execution-time bound has been established.
`docs/OFFLINE_CAPTURE_GATES.md` records the actual stopped-write versus streaming
group-launch behavior and the smallest exposure-settling experiment. The same
document specifies the new4x diagnostic session and its built, undeployed
first-fault recorder. Neither experiment is implicitly authorized by this plan.

At 2592x1944, the current pair plus radiance/validity representation requires
45,349,632 dedicated bytes. Those buffers must not overlap capture/GVSP ownership.
R5 merge latency and memory traffic remain unmeasured; per-pixel division cannot
be inserted into the real-time poll loop. A static-scene, single-shot pipeline is
the bounded target, not an unqualified continuous HDR stream.

Hardware completion needs a separate exposure-bracket test window with a useful
static scene, demonstrated settling/frame association, then bounded processing
latency and recovered-range evidence. No training data is required. Tone mapping
of one RGB8 frame does not replace exposure-bracket acquisition or this gate.

## Release closure

Keep these three gates explicit in README/TODO. Preserve current v11 and v10
rollback, failed capture evidence and natural-scene originals. Optical benefit,
physical LED observation, broad formats, endurance, clean-checkout reproduction
and redistribution/license review remain separate from the bounded acceptance.
No public publication, Flash/boot changes or persistent UserSet overwrite is part
of the next offline work.
