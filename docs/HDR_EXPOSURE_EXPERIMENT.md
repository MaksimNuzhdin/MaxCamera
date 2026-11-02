# HDR frame-effective-exposure experiment

Status: prepared offline, not executed. Current released pair is FW0.3.15/FPGA v13.
No HDR mode is linked or advertised. This experiment screens acquisition behavior;
it is not a dynamic-range, optical calibration, motion or endurance qualification.
Existing authorization covers bounded RAM/transient tests, but suitable scene
readiness is still pending. No new core, Flash, boot or UserSet write is required.

## Scene and duration

Reserve approximately10–15 minutes of exclusive camera access after the user
confirms readiness. Keep the camera and scene stationary. Provide steady daylight
or a known non-flickering continuous lamp that will remain on throughout the test.
Avoid screens, pulsed lights and moving objects. Include a matte neutral midtone
patch, a darker textured region and a bright textured region; keep at least one
patch above black and below saturation at both exposures. This is not a request
for a training dataset or a calibrated target. If light/scene stability cannot be
established, retain the evidence and stop without claiming association.

## Bounded acquisition procedure

1. Confirm exclusive control, exact running ELF/bit hashes, firmware/FPGA identity,
   available disk space and original runtime snapshot. Preserve v12 rollback and
   all UserSets. Record sensor mode, dimensions, Bayer phase, row period, manual
   gain, exposure lines, packet interval and black/saturation screening assumptions.
2. Use a fixed supported640x480 unpacked RAW10LE16 profile for the first screen.
   Verify actual readbacks and payload size614400 bytes. Keep geometry, row timing,
   gain and ISP path fixed. Disable automatic exposure/gain and test patterns.
   This screen applies only to this exact mode; it does not qualify full resolution.
3. Choose two legal manual exposures with a nominal4:1 line ratio using a short
   preview, while keeping the selected patch above black and below saturation.
   Record the chosen values; do not hardcode unobserved suitable exposures. A
   clipped or noisy patch requires changing the planned exposure/patch before the
   recorded trial, not changing acceptance thresholds after observing failure.
4. First run separate stopped/configure/start acquisitions at short and long
   exposure. Retain at least12 consecutive frames per acquisition, including first
   frames. Record start/stop, command issue/ack and host receipt monotonic times.
   Restart creates a new capture sequence epoch and a separate manifest. Never
   splice restart gaps into a supposedly continuous stream.
5. Then record one continuous short→long→short sequence, at least12 frames per
   commanded plateau, repeated once if the first trial is clean. Keep every frame
   across each transition, including transitional frames. Log the precise SCCB
   command issue/ack events and captured frame sequence; command labels describe
   requests, not effective exposure. Do not discard an assumed number of frames.
   If the current control path refuses a live exposure write, preserve that result;
   do not remove locks to force this experiment. Stopped-start evidence remains
   separate from live group-update evidence.
6. Approximately96 RAW frames occupy59 MB at640x480; cap the trial at120 frames
   (74 MB payload) plus logs. No open-ended acquisition or full raw packet capture.
   Reject gaps, incomplete frames, changed timing/configuration, CSI errors or
   recovery as an invalid association trial. Preserve original failure evidence.
7. Restore the snapshot, verify ready/stopped/unowned state and original settings,
   close only our connections, retain hashes and log the final state. No persistent
   save or startup-selection change.

## Offline screening and manifest

Run `python3 experiments/hdr/analyze_plateaus.py CAPTURE_DIR/manifest.json`.
The tool only reads local files. It verifies image identity fields, each capture's
SHA256/size/RAW10 range, continuous capture sequence, monotonic host receipt time,
fixed configuration generation and fixed manual gain. It computes independent
medians for the four CFA parity sites, temporal relative drift, clipped sample
counts and observed response ratios versus commanded exposure ratios. It does
not demosaic or silently select a favorable subset.

Manifest schema1 has these required fields:

- `format`: `RAW10LE16`; integer `width`, `height`; `manual_exposure` and
  `manual_gain`: true; pinned `firmware_sha256`, `bitstream_sha256`.
- `black`, `white`: declared screening thresholds within0..1023, not a claim of
  measured sensor calibration. `patch`: integer `[x,y,width,height]`, at least2x2.
- `maximum_relative_drift`: declared fraction between0 and1 (for example0.05 is
  an experiment screening choice, not an industry standard or measured bound).
- `configuration_generation`: fixed trial epoch for geometry/timing/route; exposure
  requests are separately logged. Any other configuration change ends this trial.
- `plateaus`: ordered entries with `name`, `commanded_exposure_lines`, `gain16`,
  and `frames`. Each frame has a unique relative `file`, `sha256`,
  `capture_sequence`, `host_receive_monotonic_ns`, `configuration_generation`.
  At least three frames per plateau are required by the checker; the acquisition
  plan uses12 to retain transition detail. A single-plateau stopped-start trial
  reports stability only. Response comparison requires distinct requested
  exposures in a continuous trial; never join restart epochs to manufacture it.

Keep a separate event log with the command issue/ack times, requested values,
readbacks, acquisition boundaries and frame errors. Receipt timestamps are not DMA
completion or sensor SOF timestamps. A plateau containing a transition may rightly
fail drift screening; preserve that complete result. Any follow-up stable-tail
analysis must name the exact excluded frame IDs and the evidence justifying them,
retain the original result and never relabel excluded frames as verified.

## Association and integration gates

A stable brightness ratio is supporting evidence, not proof that an SCCB ack or
cached exposure value belongs to a particular frame. Do not translate this tool's
screening result into `HdrFrame.effective_exposure_verified`. The output always
sets that flag and `hdr_qualified` false. Establish a repeatable association rule
for each direction, restart history and exact mode using retained transition
frames and command events; if that cannot be bounded, a sensor-event/metadata
instrumentation change is required before production HDR. No guessed settle delay.

Only after this gate: reserve dedicated buffers (full-resolution pair plus
radiance/flags45,349,632 bytes), integrate cache visibility at DMA completion,
exclude leased memory from DMA/resend reuse, and prove stop/error/reset drains
before reuse. The existing `HdrSession` contract retains leases during configuration,
DMA, READY consumption and cancellation drain. The bounded merge worker processes
at most256 samples per call, but its R5 time and bus interference are unmeasured.
Do not run an unmeasured merge in the capture/GVCP loop. Control availability must
stay absent/false until those gates and end-to-end acceptance pass. The single-shot
candidate has no motion compensation or calibrated response; RGB tone mapping is
not a substitute for true multi-exposure HDR.
