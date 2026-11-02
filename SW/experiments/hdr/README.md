# RAW exposure-bracket merge component

This portable C component is implemented and tested offline. It is **not linked
into the camera firmware**, and it does not establish that OV5640 exposure changes
have settled on any captured frame. No camera HDR mode is advertised.

`HdrBracket_Validate` requires immutable per-frame effective exposure metadata,
manual exposure, identical Bayer phase/geometry/configuration generations and line
period, increasing frame identifiers/timestamps and an explicit maximum pair age.
Exposure energy includes analog gain. Cached current sensor telemetry must never
be marked `effective_exposure_verified` without a proven acquisition contract.

`HdrBracket_Merge` consumes two caller-owned unpacked RAW10 buffers and produces
Q16 black-subtracted DN per exposure line at unity gain. It uses the longer
exposure below its declared saturation threshold and the shorter exposure above
that threshold. Both saturated and below-black results carry invalidity flags;
they are not invented recovered detail. Outputs are distinct from immutable input
buffers. Malformed metadata, size or samples are rejected before output writes.

This minimal merge has no motion compensation, noise weighting, sensor-response
calibration or transition blending. Exposure/gain error can produce a transition
discontinuity; moving subjects can ghost. Equal-exposure pairs are rejected. The
output is linear radiance-relative data, not a display image or tone-mapped RGB.
The actual black/saturation thresholds and exposure metadata remain sensor-mode
qualification inputs. Synthetic arithmetic tests are not measured dynamic range.

## Reproduce

```bash
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  experiments/hdr/bracket_merge.c experiments/hdr/test_merge.c \
  -o /tmp/maxcamera-hdr-merge
ASAN_OPTIONS=detect_leaks=0 /tmp/maxcamera-hdr-merge
```

ASan/UBSan pass. LeakSanitizer cannot operate in the sandbox's traced execution;
leak detection was disabled for the rerun. This component performs no allocation.

## Integration decision

Start with a single-shot, static-scene bracket, not a continuous HDR stream.
At 2592x1944, two uint16 RAW buffers require 20,155,392 bytes; uint32 radiance plus
uint8 validity require 25,194,240 additional bytes (45,349,632 total). These must
be dedicated leases, never reused while capture/GVSP owns them. Integer division
per output pixel has not been timed on R5; it must not enter the existing capture
poll loop. A worker core/accelerator is not authorized or configured by this work.

The immediate gate is a separately approved RAW bracket capture proving effective
exposure, settling, sample alignment and frame ownership. After that, measure a
bounded single-shot compute budget and choose CPU worker versus a RAW PL merge.
Current RGB8 tone mapping cannot substitute for this higher-precision path.

## Single-shot session and buffer contract

`bracket_session.c/.h` implements the nonblocking software boundary around two
reserved RAW10 buffers and the existing merge. It remains an **offline integration
component**, not an exposed GenICam feature or production acquisition hook.
Run `bash experiments/hdr/run_tests.sh` for sanitizers and C++/C linkage tests.

Begin validates two nonoverlapping, caller-owned buffers and manual short/long
requests atomically. The adapter claims each configuration command once, then
acknowledges completion before DMA can be armed. A generation/phase token rejects
late acknowledgements and stale frames. No implicit retries or sensor writes occur
inside this component. Configuration templates always carry verification=false;
a successful register write never manufactures effective-exposure evidence.

The adapter supplies completed frame metadata only after proving actual exposure,
DMA completion and cache visibility. Unverified/mismatched configuration, reversed
frame sequence/timestamp, excessive pair age or a late callback fails closed.
Every active event checks the monotonic deadline, including delayed callbacks
without an intervening poll. Unsigned elapsed-time subtraction handles clock wrap;
timeouts are bounded to INT64_MAX. Use a monotonic device time and extended capture
sequence, not a wrapping GVSP block ID or host receipt time. Timestamp reset/mode
changes require cancellation and a new generation.

Cancellation during configuration or DMA enters DRAINING. Buffers remain owned
until the adapter confirms both old I/O paths quiescent. Even after quiescence or
a failure, leases remain reserved/held until explicit Release. READY also holds
both immutable frames through merge/consumer completion. Cancel does not permit
reuse while a consumer may still read them. Init is once per context lifetime,
after all previous hardware/consumer work is quiescent; reinitializing an active
context would discard generation protection and is forbidden. Generation
exhaustion fails rather than wrapping. Calls are serialized, not IRQ-safe locks.

Each session operation performs constant work over two descriptors, allocates no
memory and does not scan image data. The synchronous merge still scans all pixels and performs integer division;
that entry point must not run inside the capture poll loop. The optional
HdrMergeJob worker now bounds each Step to at most256 samples, with a separate
complete prevalidation pass before output writes. Init once, Begin on an inactive
job, call Step with budget1..256, and publish only at DONE. Cancellation marks
partial private output unusable and never releases the caller's frame leases.
This bounds computational work; R5 wall-clock latency and an appropriate poll
budget remain unmeasured. No production call site has been added. R5 object compilation passes, but link/runtime latency, bus load and
buffer reservation are not qualified by an object-size result.

### Actual camera integration gaps

`Ov5640_SetExposure` delegates to GroupUpdate, which ends with register0x3212
launch. It reports SCCB transaction success; no source contract establishes the
first sensor frame using the new values. Do not guess a discard-frame count.
`GigeCamera_Poll` currently rotates six DMA buffers excluding only the current
completion and active GVSP sender; it does not know HDR leases. The DMA arm
function calls ForgetBuffer before reuse. Directly retaining those buffers in an
HDR consumer would therefore be unsafe until selection honors leases or uses
separate dedicated buffers. Current frame timestamps are captured at DMA arm,
not independently observed sensor SOF/exposure boundaries.

The linker maps application DDR from0x00100000 for0x3fefffff bytes. This is a
linker region, not measured free memory or a reservation for HDR. The current raw
array alone reserves six padded maximum RGB-sized buffers. A future adapter must
reserve and verify its own45,349,632-byte full-resolution working set, preserve
GVSP/resend lifetime, and perform existing cache maintenance at the correct DMA
boundary. No linker, camera rotation or sensor driver was changed here.

## Reproducible exposure-plateau screening

`analyze_plateaus.py` validates pinned RAW10LE16 files and reports per-CFA-site
stability, clipping and exposure response. It never sets effective-exposure
verification or HDR qualification true, even on an ideal synthetic ratio.
`python3 experiments/hdr/test_plateaus.py` covers integrity, sequence/generation,
manual-gain consistency, clipping, drift and immutable input manifests.
See [scene, capture procedure and manifest contract](../../../docs/HDR_EXPOSURE_EXPERIMENT.md).
No camera acquisition is performed by either Python tool.
