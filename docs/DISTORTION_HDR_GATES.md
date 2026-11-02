# Distortion and HDR: concrete remaining implementation gates

This is a source-grounded integration assessment for the November 2026 portfolio
scope. Neither feature is currently exposed as implemented camera processing.
Synthetic geometry or arithmetic cannot establish optical calibration or recovered
scene dynamic range. No new camera acquisition, calibration capture or deployment
was performed by this assessment.

## Lens distortion correction

The existing LSC grid in `raw_lsc.cpp` changes Bayer sample amplitude; it does not
move pixel coordinates. The scaler interpolates an affine axis-aligned grid;
it cannot implement radial/tangential lens correction by changing scale ratios.

A usable correction requires intrinsics in pixel units, distortion model and
coefficients, source size/ROI/orientation, focus state, and the desired output
intrinsics/crop. A Brown-Conrady implementation would inverse-map each output pixel
through output intrinsics and apply radial/tangential distortion into source
coordinates. These parameters must refer to this lens and geometry. No qualified
set is present in the current evidence. An identity or invented coefficient set
would demonstrate arithmetic only, not correct this camera's distortion.

The installed pinned AMD implementation is
`build/deps/Vitis_Libraries-2024.2_rel/vision/L1/include/imgproc/xf_remap.hpp`.
Its bilinear path accepts separate X/Y map streams and a bounded line window;
its storage and accessible vertical displacement depend on WIN_ROW and COLS.
The camera currently supplies no X/Y map DMA streams. Arbitrary full-frame maps
cannot be substituted into the VPSS control registers. A 2592x1944 pair of float32
maps alone occupies 40,310,784 bytes; an RGB8 frame occupies 15,116,544 bytes.
DDR capacity alone does not establish a map/source/output bandwidth or buffer
ownership contract. The current continuous capture/GVSP buffers must not be
reused underneath an asynchronous remapper.

Minimal next implementation scope: fix a calibrated resolution and measured
vertical-displacement bound; choose a line-window remap only if the entire map
fits that bound, otherwise define separate DDR source/output leases and map DMA.
Then validate identity, borders/out-of-range fill, interpolation and metadata,
combined resources/timing and optical straight-line/reprojection behavior.
The new adaptive block's full-design fit must be known before allocating the
remaining resources. No general remap capability should be advertised meanwhile.

## True multi-exposure HDR

`Ov5640_SetExposure` in `Ov5640.c` validates line counts and gain and issues a
six-register grouped update. It does not tag a captured frame with the effective
exposure/gain or prove which frame after a change has fully settled. Current
streaming telemetry is a cached measurement, not an immutable per-frame exposure
record. The existing application streams a single image path; there is no verified
short/long exposure pairing state machine or aligned dual-stream merge input.

The pinned AMD `xf_hdrmerge.hpp` consumes two same-position input streams and
weight tables. Invoking that function does not establish correct brackets,
exposure normalization, black subtraction, motion correspondence or valid weights.
The current public color output is RGB8; merging already clipped display pixels
cannot recover the lost RAW radiance. A higher-precision RAW merge and subsequent
demosaic/tone path needs an explicit precision/metadata design, not simply a
new gamma preset or the adaptive RGB8 block.

Minimal next hardware experiment, requiring separate authorization: retain the
current image/presets, stop the stream, select a bounded RAW mode, disable automatic
exposure/gain, capture two known manual exposures with conservative settling,
record exact settings/timestamps and restore the original runtime. The experiment
must identify actual usable brackets before implementing a production acquisition
state machine. A static scene is sufficient for initial arithmetic validation;
motion/ghosting and scene dynamic-range claims require additional evidence.
No Flash/boot changes or training dataset are necessary for that experiment.

HDR output cadence necessarily depends on bracket acquisition, buffering and
merge cost. Any lower optional-mode rate must be explicit and measured; the
existing non-HDR modes must keep their documented behavior. Until these gates
are satisfied, HDR remains open rather than being replaced by tone mapping.

## Implemented independent components (November 1)

`tools/distortion_maps.py` generates equal-canvas Brown-Conrady inverse maps from
explicit input/output intrinsics and five distortion coefficients. It records
calibration/geometry identity, source-file SHA-256, separate float32 map hashes,
interior/border coverage and a conservative symmetric line-window budget. It
refuses to overwrite existing artifacts and rejects invalid/nonfinite coordinates.
Five synthetic tests pass in `tests/test_distortion_maps.py`. No calibrated map
for this camera was invented or applied.

Input JSON requires `width`, `height`, `calibration_id`, `geometry_id`,
`input_intrinsics` and `output_intrinsics` as `[fx, fy, cx, cy]`, and `distortion`
as `[k1, k2, p1, p2, k3]`. The geometry identifier must name the actual lens/focus,
sensor ROI/orientation and pixel-coordinate convention used during calibration.
Run `python3 tools/distortion_maps.py calibration.json output-directory`.
These are build/calibration inputs for in-camera processing, not host-side image
correction. No image acquisition occurs.

The current feasibility decision is conditional line-window remapping, not
unrestricted DDR warping: obtain the actual displacement bound, compare its
synthesized memory plus map-source cost with the completed v11 budget, and only
then select a bounded mode. The tool's byte estimate is not mapped BRAM usage.
AMD bilinear remap checks both neighboring row tags even at an integer last-row
coordinate; identity-edge behavior needs explicit RTL testing and a documented
border policy. A generic map alone does not make this block integration-ready.

`experiments/hdr/` implements portable RAW10 bracket validation and fixed-point
radiance merging with saturation/black validity flags. ASan/UBSan synthetic tests
pass, including rejection of unverified exposure metadata, mismatched generations,
stale/reordered frames, malformed samples and partial output updates. The merge
is intentionally not linked into firmware; its capture contract and CPU timing
are unproven. See its README for the single-shot architecture and memory budget.
This is a real two-exposure arithmetic component, not another display tone curve,
but it does not close camera HDR acceptance.

Evidence: `diagnostics/distortion-hdr-components-20261101/`. Neither component
accesses hardware or changes presets. Remaining smallest inputs are calibrated
lens/geometry parameters for distortion, and authorization for an exposure-settling
RAW bracket experiment for HDR. New pipeline deployment remains separately gated.


## Post-route budget decision

The full v11 candidate leaves 5526 LUT, 42.5 BRAM36 and five DSP. The BRAM bit
capacity corresponds to 195840 bytes before banking/port/packing overhead. At
2592-pixel RGB width, one logical row is 7776 bytes: even an ideal buffer holds
only about25 rows if every remaining BRAM is allocated to it. Map DMA, FIFOs,
control and implementation margin also need resources. Therefore unrestricted
full-frame distortion correction is **not** approved as a feasible next drop-in
stage. A bounded smaller-resolution/vertical-displacement remapper still needs
standalone resource proof plus a map transport and integration budget; map
arithmetic alone cannot justify advertising that mode.

HDR is also not a drop-in RGB postprocessing block: it requires exposure-tagged
RAW buffers and processing before precision is lost. The implemented merge is a
candidate single-shot component; it cannot run inside the R5 capture poll loop
without measured scheduling/coherency and dedicated buffer ownership. Neither
remaining feature is marked complete because the independent tests passed.

## HDR session contract increment

The offline `experiments/hdr/bracket_session.c/.h` now implements generation-tagged
configuration, DMA and consumer ownership with explicit cancel/drain, deadlines
and verified exposure/frame metadata. It allocates no memory and performs constant
work over two leases. Native sanitizers/C++ linkage and R5 object compilation pass.
It does not establish sensor settling or reserve actual DDR. Current production
buffer rotation still excludes only the completed/current GVSP buffer and must
be extended or bypassed with dedicated buffers before this can safely integrate.
See `experiments/hdr/README.md` and `diagnostics/hdr-session-20261101/`.
