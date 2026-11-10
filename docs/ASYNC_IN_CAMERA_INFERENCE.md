# Asynchronous in-camera inference: architecture investigation

Current-status note, November10,2026: the dated plans below retain their original
scope. The later A53 analytics and same-IP Ethernet web implementation is now
bounded-camera tested. Publication of the current update is authorized. See
[Project status](PROJECT_STATUS.md) and [MicroSD boot](MICROSD_BOOT.md) for the
current implemented/open items; do not interpret an older research-only or
publishing-pending statement as the current feature state.


Date: 2026-10-27. Research only: no model download/execution, enrollment, dataset,
AI deployment, boot change or hardware access by this reviewer.

## Observed ownership and memory

The approved LED load log in diagnostics/stream-led-board-20261027/board-load.log
shows R5 #0 running, R5 #1 reset, A53 #0 stopped at the FSBL exit breakpoint in
EL3, and A53 #1 in power-on reset. tools/run_board.tcl enables RPU split mode,
runs FSBL on A53 #0 and starts the application on R5 #0. These are observed states
for this session, not proof that another core can safely start arbitrary code.
The maintained platform exports standalone_psu_cortexr5_0; the build selects its
Cortex-R5 toolchain. No Linux application/runtime or OpenAMP/RPMsg handoff was
found in the inspected application paths gige/, src/, main.cpp. PMU firmware and
FSBL responsibilities must remain intact. R5 #0 owns camera lifecycle, SCCB,
frame completion, GVCP/GVSP and GEM; do not move or duplicate these owners.

src/lscript.ld permits application DDR from 0x00100000 through the configured
low DDR region below 0x40000000. The built BD declares DDR high 0x3fffffff.
The LED ELF size report has 623396 text, 3100 data and 152344880 BSS bytes.
These are static allocations, not measured free RAM. There is no reserved AI
carveout yet; verify ELF section addresses, FSBL/PMU reservations, stack/heap,
DMA buffers and any future OS map before selecting physical addresses.
GigeCamera.cpp allocates six maximum raw buffers and four converted-image
buffers (about 151 MB combined); their apparent spare capacity is not a worker
allocator. The current application heap is only 0x2000 by linker default.

## Actual frame lifetime

Sensor -> CSI receiver -> selected RAW/color ISP and scaler/output stages ->
AXI framebuffer writer at 0xa0080000 -> DDR -> R5 GVSP packet sender -> GEM3.
GigeCamera.cpp arm() starts a single-shot write, records its timestamp and
forgets old resend history for the reused raw buffer. On completion it selects
another buffer, excluding the completed one and g->frame, then rearms capture.
GigeVision_SubmitFrameStrided pins the active sender buffer through its trailer;
resend history does not confer permanent ownership. A worker retaining the
pointer after completion without a new ownership rule can read a mixed frame.
Converted images rotate through four slots and are equally unsafe to retain.

Current DMA cache handling is local to R5: clean/invalidate at initialization
and completion; GEM descriptors use a shareable noncacheable MPU region.
This is not an inter-core coherency protocol. Before enabling another CPU,
verify AXI port/coherency attributes and both cores' mappings. Prefer explicit
cache maintenance and barriers with a small aligned noncacheable control page;
do not assume volatile or a shared physical address makes pixels coherent.

## Minimal bounded handoff proposal

Keep acquisition and Ethernet entirely on R5 #0. Use one A53 worker initially;
R5 #1 is a possible small bare-metal helper, not an existing OpenCV runtime.
A53 is the more natural candidate for a Linux/OpenCV experiment, but neither
its boot/runtime integration nor CPU/memory budget is validated here.

Offer at most one immutable completed frame to the worker. Add an explicit
analytics pin to capture-buffer selection, separate from the GVSP pin. The
worker copies/resizes into its own reserved snapshot and acknowledges release
before doing inference. No full-frame copy or inference inside the R5 polling
loop. While busy, skip analytics offers; never wait for inference or queue an
unbounded frame history. The next offer is the latest eligible frame. A worker
crash disables further analytics; never revoke a pin and overwrite while an
unconfirmed worker might still read it. Release only after acknowledgement or
confirmed worker quiescence/reset. Test whether retaining one of six slots
preserves the existing throughput; do not assume this from buffer count alone.

Descriptor: protocol version, generation/epoch, ownership state, capture
sequence, transmitted GVSP BlockID if submitted (explicit invalid otherwise),
arm timestamp and units, format, width/height, stride, buffer length, physical
offset within an approved carveout, ROI/scale transform. Validate all bounds.
Use release/acquire barriers and architecture-correct cache operations; publish
metadata only after DMA completion. Never publish partially updated 64-bit
fields across a 32-bit R5 reader/writer: use generation/seqlock or a locked
snapshot protocol. Reset/geometry changes invalidate old generations/results.

Start with RGB/YUV output already available in DDR. The existing scaler changes
the primary output; it does not provide a simultaneous independent thumbnail.
Do not change host stream dimensions to feed AI. Worker-side resize/letterbox
into, for example, 320x240 RGB uses 230400 bytes per slot; two slots use 460800
bytes, excluding scratch/model/runtime. RAW Bayer needs a separate specified
conversion; defer that input profile initially. Even low-rate full-frame reads
consume DDR bandwidth: measure while streaming, especially maximum LE16.

Return bounded results to R5 through the control page: source sequence/epoch,
BlockID/timestamp, completion time/age, status, count, boxes/landmarks and scores,
plus resize/ROI coordinate transform. R5 exposes a latched vendor GenICam result
snapshot and selector-indexed bounded array, analogous to current latched scene
statistics. This is proposed work, not an existing AI API. Keep delayed results
separate from the current image. Do not append late data to an already-sent GVSP
frame or claim chunk/event/multipart support without implementing/testing it.

## Runtime and model implications

A second bare-metal image can validate the mailbox/copy/checksum path without
an OS, but standard OpenCV DNN integration is not present in this BSP. Linux on
A53 needs a distinct kernel/rootfs/device tree, reserved memory and peripheral
ownership; it must not reset the running R5/GEM/camera. AMD OpenAMP documents
shared memory and IPI configuration as explicit integration work. It is an
option, not a component already configured here. Linux must not claim R5-owned GEM, CSI, SCCB, DMA or GPIO devices. Do not
attach remoteproc to the already-running R5 without a compatible resource table,
device tree, driver and PMU firmware lifecycle contract. Use IPI/mailbox messages
for descriptors or doorbells, not frame payloads. A volatile proof-of-concept
still needs separate approval for starting cores/loading a new runtime; LED
approval does not cover this. Persistent boot/Flash changes need separate scope.

OpenCV documents FaceDetectorYN and FaceRecognizerSF as separate pretrained
ONNX workflows. Detection returns face locations/landmarks; recognition compares
identity embeddings and requires an enrollment/reference policy, consent,
retention/access rules and measured false-match behavior. No enrollment is
needed to prototype detection. No model license, target latency, runtime memory
or accuracy on this camera has been qualified. Model file size is not runtime
memory. Start with detection only if that experiment is later approved.

Official references inspected (no model files downloaded):
- https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/1837006921/OpenAMP%20Base%20Hardware%20Configurations
- https://docs.opencv.org/4.13.0/d0/dd4/tutorial_dnn_face.html

## Finite stages and acceptance gates

1. Offline ownership/mailbox tests: wrap, stale generations, malformed bounds,
   delayed/crashed worker, reset/cancellation, no overwrite of leased buffers.
   Finalize non-overlapping DDR map and inspect RPU/APU/AXI configuration.
2. Separately approved RAM-only worker checksum/copy experiment, no AI model.
   Measure snapshot latency, R5 polling/control latency, FPS, frame integrity,
   packet/CSI errors, recovery and restoration against identical baseline.
   Start low-rate; worker overload must skip analytics, never stall capture.
3. If stage 2 passes, evaluate exact official pretrained detector license,
   operators, runtime allocation and latency before any model download/run.
   Independently approve/runtime-qualify Linux or a supported bare-metal runtime.
4. Only then model experiment and result metadata acceptance; no identity
   recognition/enrollment or dataset work unless separately selected.

Unknown gates: exact A53 clocks/runtime map, spare DDR under runtime load,
interconnect coherency/QoS, safe worker core lifecycle, cache maintenance cost,
model operator support and latency, minimum detectable face size and acceptable
result age. No new FPGA DSP stage is proposed (current design uses 233/240 DSP).

## Required GenICam authority and proposed vendor nodes

User requirement recorded 2026-10-27: the primary R5 camera controller remains
sole GenICam/GVCP authority for model selection, enable/disable, parameters,
status and result reads. The A53 worker has no independent camera-control
endpoint. R5 validates/queues bounded IPC requests and exposes their asynchronous
status; model loading or inference never blocks capture or GVCP processing.

The following names are proposals in a vendor-specific AI category, not standard
SFNC nodes, existing XML controls, or an implemented feature:

| Proposed node family | Contract |
| --- | --- |
| MaxAIAvailable, MaxAIWorkerState, MaxAIProtocolVersion | Capability/runtime readiness; absent worker means unavailable, never success. |
| MaxAIModelSelector, MaxAIModelSupported, MaxAIModelEnabled | Enumerated installed/approved model IDs; selector chooses per-model configuration. Enable is a requested state, separate from running state. |
| MaxAIModelState, MaxAIErrorCode, MaxAIErrorSequence | Disabled, Starting, Ready, Busy, Stopping, Fault; bounded numeric errors and sequence. |
| MaxAISamplePeriodUs, MaxAIMaxResultAgeUs | Requested sampling cadence and staleness limit; actual cadence and skipped samples exposed separately. No throughput promise. |
| MaxAIParameterSelector, MaxAIParameterValue, MaxAIApplyConfiguration | Model-declared typed/ranged parameters, staged then atomically committed; unsupported parameters rejected. |
| MaxAIConfigGeneration, MaxAISessionGeneration | Accepted configuration and worker/session identity; returned on all acknowledgements/results. |
| MaxAIResultLatch, MaxAIResultRelease, MaxAIResultVersion | R5 copies one bounded committed result into an immutable read snapshot. Reads stay consistent until release/re-latch; access-session ownership and timeout bound retention. |
| MaxAIResultValid, MaxAIResultModel, MaxAIResultConfigGeneration | Validity and exact model/config identity for the latched snapshot. |
| MaxAIResultFrameID, MaxAIResultFrameIDValid, MaxAIResultCaptureSequence | Distinguishes a transmitted GVSP BlockID from a capture never submitted for transmission. |
| MaxAIResultTimestamp, MaxAIResultCompletionTimestamp, MaxAIResultAgeUs | Source and completion times with explicit units/epoch; live age derived from latched source timestamp. |
| MaxAIObjectCount, MaxAIObjectSelector | Bounded indexed objects in the same latched snapshot. |
| MaxAIObjectClassID, MaxAIObjectScore, MaxAIObjectX/Y/Width/Height | Typed result fields and documented image coordinate space; optional landmarks only when model supports them. |
| MaxAIActualPeriodUs, MaxAISkippedSamples, MaxAIInferenceTimeUs | Measured scheduling/latency diagnostics, not nominal advertised performance. |

A result commit uses an IPC sequence/checksum/bounds contract; R5 checks session,
config generation and model ID before publishing. The GenICam latch operates on
R5-owned memory, never live worker memory. Multiword fields and indexed results
cannot mix generations. A bounded snapshot survives a worker update; disable or
reset marks it invalid explicitly rather than silently presenting old results as
current. Clients re-latch to see new data. Optional events/chunks are deferred;
direct GenICam retrieval is mandatory and sufficient for the initial design.

Disable immediately stops new offers, advances configuration generation and
invalidates current results; worker cancellation/drain is asynchronous. Do not
release a buffer still being read. Timeout enters Fault, rejects stale replies,
and disables further offers without resetting the camera. Worker restart requires
confirmed quiescence, new session generation and fresh handshake; late replies
from a prior session never publish. Camera reset/format/ROI change similarly
invalidates coordinate transforms and old results. Ownership loss follows camera
control policy; it must not allow a new client to inherit another client's latch.

Initial scheduler permits one active model and one in-flight snapshot. Requesting
an incompatible second model returns a resource/conflict status; do not silently
run several models or replace one. Later scheduling needs measured memory/CPU
budgets and bounded per-model cadence. Named identity recognition remains a
separate enrollment/retention decision, not implied by these generic controls.

## Smallest proposed new-core experiment approval scope

Not executed or currently authorized: build and RAM-load one A53 bare-metal
checksum/copy worker plus matching R5 mailbox firmware, with a reviewed disjoint
DDR carveout. No Linux, model, dataset, Flash, persistent boot or UserSet changes.
Use the existing accepted FPGA image; no new processing stage. Startup may require
an intentional camera stop and volatile PS/core reinitialization, so treat it as
a temporary stream interruption, not a hot-add operation. Duration is not yet
measured. Do not start until memory/clock/reset ownership review is complete.

Bounded test: initially one small RGB snapshot per second, limited run, matched
baseline/candidate FPS and packet/CSI/control-latency measurements, checksum and
canary checks, delayed-worker/timeout simulation, no unbounded queue, then stop
the worker and restore saved camera settings. Failure stops analytics first.
Rollback is the sealed paired LED FPGA/R5 images reloaded into volatile memory
with the same established startup procedure; preserve v9 as secondary rollback.
This also interrupts streaming and should be included in the same explicit
experiment approval. Linux/OpenCV introduction is a later, separately scoped
runtime experiment, not part of this minimal transport test.

The offline proposal oracle and cache-publication checklist are in
[tests/ai_ipc_contract](../SW/tests/ai_ipc_contract/README.md). Eight tests pass as of
2026-10-27. They do not establish real inter-core ordering, memory availability,
AI execution, or a production GenICam interface.

## Candidate roadmap supplied by parent research

Planning estimates only, not independently measured on this camera. Official
catalog: https://huggingface.co/opencv/opencv_zoo/tree/main/models . No weights
were downloaded. On adoption pin exact revision, artifact/hash, runtime version
and reverify each model's license; directory-level reports are not a completed
license audit. Reported licenses: MIT for YuNet, Apache-2.0 for the other listed
model directories.

| Priority/function | Candidate weights | Approximate file size | Gate |
| --- | --- | ---: | --- |
| First: face detection | YuNet 2023mar ONNX, OpenCV4 | 0.233 MB | SingleShot latency/memory/stream interference |
| Next: object detection | Zoo NanoDet 416 | 3.8 MB | Supported operators and detection scope |
| Optional ROI classification | MobileNetV2 | 14 MB | Label set and ROI contract |
| Later identity comparison | SFace | 38.7 MB | Enrolled gallery, unknown threshold and retention policy |
| Later text detection/recognition | PP-OCRv3 + CRNN_EN | 36.2 MB | Digits/lowercase Latin; not Russian or Hebrew support |
| Later person/pose | MediaPipe person + pose | 17.56 MB | Two-network pipeline and result mapping |

About eight weight files/six functions/110.5 MB of weights is a catalog estimate,
not measured RAM consumption, capacity or a promise to load all models. Begin in
SingleShot mode. Measure peak RSS (or allocator high-water mark for bare metal),
latency and GVSP interference before enabling periodic requests. No FPS claim.

Initial proof remains one model/pipeline. A later multi-enabled scheduler runs
one inference job at a time, batch one; a selected pipeline may contain two
networks sequentially. Use bounded fair selection and per-model periods with
one latest pending snapshot, not a queue per model that grows under overload.
Record skipped opportunities and actual per-model cadence. Enabling a model is
not a promise of simultaneous execution; reject admission if its runtime memory
budget is unavailable. Resource accounting must include all resident weights,
activations, framework allocations and snapshot storage, not just file sizes.
