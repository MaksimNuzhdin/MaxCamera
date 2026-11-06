# A53 analytics implementation

Experimental implementation, authorized on November 5, 2026. Earlier research
notes describe historical approval boundaries; they do not describe this new
implementation. The R5 camera remains the sole GenICam/GVCP/GVSP authority.
A53 Linux runs CPU inference with no added FPGA/DPU resource requirement.
On November6 the final0.3.16 firmware and Linux6.6.40 were loaded through JTAG
on the camera. Bounded RGB640x480 streaming/AI and static-pattern copy checks pass.
This does not establish optical accuracy, all-mode cache/clock qualification,
physical MicroSD boot or sustained/endurance performance.

## Features

| Enable bit | Function | Backend and limitations |
| --- | --- | --- |
| 1 | Face detection | OpenCV YuNet, face boxes; no identity requirement |
| 2 | Face recognition | SFace cosine comparison with explicitly supplied gallery; empty gallery returns unknown |
| 4 | Object detection | YOLOX, 80 COCO classes, per-class NMS, top-left letterbox coordinate inversion |
| 8 | Plate candidates and OCR | OpenCV Russian plate cascade plus English CRNN; country-dependent candidate recall, no jurisdiction-specific validation |
| 16 | Motion regions | Consecutive sampled-frame difference; illumination/camera movement can trigger |
| 32 | Pedestrian detection | OpenCV HOG/SVM, separate from COCO person detection |
| 64 | QR decoding | OpenCV QRCodeDetector; text limited to31 UTF-8 bytes in GenICam |
| 128 | Tracking | Nearest-center association by detector/class, expires after three sampled frames; requires another detector bit |

Inputs are bounded to640 pixels on the longest side. Boxes are mapped back into
the captured output image, not full-sensor coordinates before ROI/scaling.
Results contain at most32 objects. Face recognition implies running the face
detector. Unknown identities use class0 and text `unknown`; their score is the
face detection score. Matched identities use class1 and cosine similarity.
An empty successful result is valid and distinct from an unavailable worker.
Scores for plate/motion/QR are operational markers, not calibrated confidence.
The synthetic OCR smoke test read `AB123CD` as `AB123ED`; no OCR accuracy target
has been established. No custom training or automatic enrollment is performed.

## Presets

The empty folders `presets/faces`, `presets/plates` and `presets/objects` exist.
On MicroSD they are located under the FAT32 root `PRESETS`. For identities create
`PRESETS/FACES/<identity>/` and add JPEG/PNG reference images, each with exactly
one face. Identity directory names become result labels. Reboot to reload the
gallery. Only `faces` currently consumes presets; `plates` and `objects` are
reserved empty directories and do not imply custom model loading.

The FAT filesystem is case-insensitive; Linux paths use lowercase names.
The card mounts read-only; inference does not modify enrollment data. The
in-memory boot copy of ONNX weights is deleted after import to reduce RAM use;
this never removes SD/user model files. Restart requires reboot. Gallery data
and identity strings are not logged by default. Accuracy, similarity threshold
and reference-image quality require validation with the intended use case.

## GenICam

The generated `MaxAI` category is part of `gige/MaxCamera.xml`, served by R5.
`MaxAIAvailable` requires a recent worker heartbeat, and
`MaxAISupportedFeatures` reports the loaded feature mask. The safe default is
`MaxAIEnabledFeatures=0`. Start with a small subset such as1 or17, then measure
load before enabling255. `MaxAISamplePeriodUs` is an admission interval, not a
throughput guarantee. Sampling skips while the one worker slot is occupied.
Only the direct FPGA RGB8/BGR8 output path currently offers frames; unsupported
formats continue normal streaming without AI offers. Select FPGA processing and
RGB8/BGR8 using the existing stopped-acquisition configuration sequence.
The decoder also supports Mono8/YUYV, but those formats are not yet connected
to the production offer point. No host-side video processing is required.

Set `MaxAIMinimumScore` and `MaxAIFaceCosineThreshold` in millionths; default
cosine0.45 is an uncalibrated starting point. Configuration changes invalidate
results. Read `MaxAIWorkerState`, `MaxAIErrorCode` and
`MaxAIInferenceTimeUs` for diagnostics. `MaxAIResultLatch=1` copies the latest
complete result into an immutable register snapshot. Check `MaxAIResultValid`,
read `MaxAIObjectCount`, select each object with `MaxAIObjectSelector`, then read
feature/class/score/box/track fields. Feature IDs are bit positions0 through7.
`MaxAIObjectTextWord0..7` contain four bytes per word, least significant byte
first, forming a NUL-terminated32-byte UTF-8 label. A numeric GenApi client can
read them even without a custom StringReg adapter. Release the snapshot with
`MaxAIResultRelease=1`. A new offer/result cannot mutate a latched snapshot.

`MaxAIResultSequence` is a worker request sequence, `MaxAICaptureSequence` is the
R5 capture counter. Neither is a GVSP BlockID. Timestamp high/low words use the
existing device timestamp clock/epoch, not wall time. Snapshot validity records
successful publication and worker liveness; it is not a freshness guarantee.
Read `MaxAIResultWidth/Height/PixelFormat/ConfigGeneration` for the latched
source. `MaxAIResultAgeUs` reports elapsed time since the capture was offered,
including inference time, and saturates at32-bit maximum. Compare age and capture
sequence/timestamp with your acquisition before acting on a result. Latency is a live diagnostic and does not belong to the latched object
snapshot. AI settings are volatile, excluded from UserSet persistence, and
cleared on control-session loss. Client ownership remains enforced by GVCP.

## Memory and ownership

The AI build uses `src/lscript_ai.ld`: R5 allocations must end below0x10000000.
Linux sees0x10000000..0x3fffffff, with0x1f000000..0x1fffffff reserved/no-map.
The first8192 bytes of that reservation form the version1 little-endian mailbox.
R5 maps it shared/noncacheable; Linux maps it through `/dev/mem` with O_SYNC.
Explicit DMB barriers bracket publication/consumption. The version is an
experimental project ABI, not an OpenAMP or GenICam standard.

The DMA-completed, cache-synchronized capture buffer is leased before the
request sequence is published. All buffer selection, including restart/recovery,
excludes that lease. A53 validates the R5 address bounds, copies to private RAM,
unmaps the capture view and acknowledges that exact sequence before inference.
A timeout or disable never revokes a reader's lease. R5 accepts only matching
request/configuration, bounded scores/boxes and32 objects. Result publication is
single-slot with R5 acknowledgement. Configuration uses odd/even generations.
Sequence/generation exhaustion fails closed; reboot establishes a new session.
R5 reinitialization while an old Linux worker survives is unsupported: reboot
both processors together. Linux remoteproc, GEM, CSI, PL peripherals, SCCB,
GPIO and FPGA reconfiguration are disabled. Linux owns only its console, SD and
core runtime services. Clock/power unused-device shutdown is suppressed.
The actual Device mapping rejected Python bulk stores with SIGBUS. The native
helper now performs aligned volatile32-bit control accesses and payload reads;
Python never bulk-copies into the Device-mapped control page. DMB barriers still
provide publication ordering. A static-pattern check after acquisition stops
matches921600 bytes read by Linux with GVSP by CRC32. This is a pixel visibility
check, not a proof of simultaneous frame identity or all-mode coherency.
Broader reset/crash/clock/GIC and full-resolution stress still require qualification.

## Build and validation

```
MAX_CAMERA_AI_IPC=ON MAX_CAMERA_GIGE_FPGA_ISP=ON \
  MAX_CAMERA_APP_BUILD_DIR=build/ai-r5 bash tools/build_gige_raw10_hw_app.sh
python3 tools/fetch_ai_models.py --fixtures
python3 tools/build_ai_rootfs.py
bash tools/build_ai_tfa.sh
python3 tools/build_ai_sd_boot.py
python3 tools/package_ai_microsd.py
bash tools/test_ai_linux_qemu.sh
python3 -m unittest discover -s tests/ai_runtime -v
cc -std=c11 -Wall -Wextra -Werror -I gige \
  tests/ai_runtime/test_control.c gige/GigeAI.c -o /tmp/maxcamera-ai-control
/tmp/maxcamera-ai-control
```

Prerequisites are the existing Vitis2024.2 installation, PetaLinux SDK/vendor
Linux6.6.40 artifacts, the `mmap` module from the matching installed PetaLinux
sysroot component, `dtc` tools, `mkfs.vfat`, `fsck.fat`, and the pinned AArch64
wheels in `build/ai-downloads`. Download wheels with:

```
python3 -m pip download --dest build/ai-downloads --only-binary=:all: \
  --platform manylinux2014_aarch64 --python-version 312 --implementation cp \
  --abi cp312 opencv-python-headless==4.12.0.88 numpy==2.2.6
```

Official model filenames and hashes are recorded in `models/MANIFEST.json`;
upstream license texts are in `licenses`. The loader fails unavailable when a
required model cannot be loaded. Restore exact approved files before rebuilding.
For other installations, set `MAX_CAMERA_LINUX_SYSROOT`,
`MAX_CAMERA_LINUX_ARTIFACTS`, `MAX_CAMERA_MMAP_MODULE`, `MAX_CAMERA_LINUX_CC` and
`MAX_CAMERA_BOOT_COMPONENTS`. The JTAG loader accepts `MAX_CAMERA_AI_BIT`,
`MAX_CAMERA_AI_XSA`, `MAX_CAMERA_AI_R5`, `MAX_CAMERA_AI_FSBL`,
`MAX_CAMERA_AI_PMUFW`, `MAX_CAMERA_AI_BL31` and `MAX_CAMERA_LINUX_IMAGE`.
The relocated TF-A build is required for JTAG boot-mode0 because the vendor
binary defaults to a low-DDR BL33 address. Its installed source mirror revision
is pinned by `build_ai_tfa.sh`; the source is extracted outside the project.
Local SDK/vendor binary provenance is recorded in the build manifests. Their
licenses/source remain in the existing SDK/PetaLinux installations; this is a
local engineering build, not a complete third-party redistribution audit.

`build/ai-linux/BOOT.BIN` contains the platform FSBL/PMU, matching v13 FPGA,
AI-enabled R5, relocated UART1 TF-A, Linux Image, patched DTB, initramfs and a small EL2
entry stub. It deliberately bypasses U-Boot to avoid its original low-DDR load
address overlapping R5. Raw data partitions have no execution entry. The entry
stub has a single load segment at0x20000000. Bootgen headers and hashes are saved.
The kernel loads at0x20200000, DTB at0x22000000 and initramfs at0x26000000.
The build checks R5 bounds and initramfs load capacity.

`build/ai-linux/maxcamera-ai-microsd.img` is a partitioned FAT32 disk image,
with `BOOT.BIN` and empty preset directories. Its FAT filesystem passes
`fsck.fat -n`, and embedded BOOT.BIN is read back and hash-compared. Write this
image to the intended MicroSD using your normal imaging tool, select the board's
SD boot mode according to its manual, and inspect the serial console on first
boot. No physical SD/Flash or boot switches were changed by these build scripts.
The image is an unqualified hardware candidate; preserve a known-good card.

## Bounded camera results

On the final0.3.16/v13 camera, RGB640x480 with20 us packet spacing delivered:

| Enabled mask | Observation | Complete frames | Approximate delivered FPS | Last inference latency |
| --- | --- | --- | --- | --- |
| 0 | AI off,8 seconds |115|14.293190|not applicable|
| 1 | Face detection,8 seconds |115|14.293489|0.309 seconds|
| 4 | Objects,35 seconds |501|14.293462|7.982 seconds|
| 255 | All features,35 seconds |501|14.293386|9.587 seconds|

These accepted series had no observed incomplete frames, missing packets,
dropped frames or CSI errors. The object/all-feature rate is about0.1 AI results
per second on this single A53. Video FPS is independent of inference FPS.
An earlier8-second object window was too short; its failure is retained.
The model returned object/unknown-face boxes and track IDs; no annotated truth
or optical accuracy verdict is inferred. The empty recognition gallery yields
unknown. OCR/recognition neural forward checks and QR/self-match fixtures are
also covered offline/QEMU; no real plate/identity accuracy campaign was run.
`MaxAIErrorCode`:0 success,1 worker exception,2 rejected result,3 a cancelled or
stale configuration. Code3 after a configuration change is not a CSI failure.

The final device is idle/unowned at DHCP10.0.0.8 with a volatile RGB640 preview,
all capabilities available and AI enable mask0. Acquire control and set the
wanted mask, then start acquisition. UserSetDefault and saved slots are unchanged.
No physical SD/Flash was written. Preset/image files are not uploaded to GitHub.

The Linux self-test runs on QEMU `virt`, one Cortex-A53 and752MiB RAM, with:
`console=ttyAMA0 rdinit=/init maxcamera.selftest=1 cma=0 swiotlb=128 rootfstype=ramfs`.
This validates CPU instruction compatibility, kernel/initramfs boot, dependency
closure and actual inference. It does not emulate the camera PL, FSBL/TF-A chain,
physical IPC visibility, sensor, SD controller or simultaneous GVSP traffic.
See `diagnostics/ai-linux-20261105/README.md` for results and remaining gates.

Sources: [OpenCV face workflow](https://docs.opencv.org/4.12.0/d0/dd4/tutorial_dnn_face.html),
[official OpenCV model zoo](https://github.com/opencv/opencv_zoo),
[AMD OpenAMP memory guidance](https://docs.amd.com/r/2022.2-English/ug1186-zynq-openamp-gsg/Settings-for-the-Device-Tree-Binary-Source).
The implementation uses a private mailbox, not the OpenAMP runtime.
