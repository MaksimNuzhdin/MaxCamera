# DinxTech MaxCamera

A source-prepared GenICam/GigE Vision camera portfolio project for the **ALINX
AXU2CGB** (XCZU2CG) and **ALINX AN5641** camera module (OV5640 sensor).
The R5 firmware owns camera control, acquisition and UDP image transport; the PL
implements the bounded ISP. This is an engineering portfolio, not formal protocol
certification or an end-to-end HDR product. AI analytics are experimental and
have the bounded test scope described below.

| Directory | Contents |
|---|---|
| [HW](HW/README.md) | Source-based Vivado v13 reconstruction, pin constraints, custom RTL, fan IP, seven HLS cores and tests |
| [SW](SW/README.md) | R5 application, local drivers, GenApi XML/catalog, host utilities and offline tests |
| [docs](docs/INDEX.md) | Architecture, ISP/control/driver guides, editable manuals and real image examples |
| [LICENSES](LICENSES) | Preserved third-party notices; see [rights review](RIGHTS.md) before publication |

The latest original-workspace qualification is **FW0.3.15-ppc4-dev + FPGA v13**.
A bounded RGB4x320x240→1280x960 test delivered57 frames at14.293804 FPS with20 us
packet spacing and no missing/incomplete frames or CSI/recovery errors; one saved
frame exactly matched the independent Q12 reference. The200 MHz routed design
used45832 LUT,126 BRAM36 and235 DSP. See [scope and provenance](docs/VERIFICATION.md).
Repackaging and compilation do not extend that board qualification to new binaries.

Supported ISP stages include defect-pixel correction, black level, gains/AWB,
CCM, tone/gamma, denoising, sharpening, lens-shading controls, CLAHE, bounded
scaling, adaptive RGB tone and a640x480 nearest-neighbor±7-row remapper.
Calibration-dependent quality is not implied. HDR remains an offline component
awaiting frame-effective exposure verification. The AI continuation below has
separate bounds and does not extend the original ISP qualification. Wider4x
geometries and endurance remain unqualified. Hardware trigger and training/data
collection are outside this release.

Start with [HW build](HW/README.md) and [SW build/tests](SW/README.md).
AMD2024.2 tools/IP licenses and externally obtained Vitis Vision headers are
required; vendor build outputs and SDKs are intentionally not included. The
export recreates the block design and builds software, but has not undergone a
fresh full HLS/synthesis/routing/board qualification. See the validation report.

The original development trees, captured originals and rollback images remain
outside this source export. Existing per-file licenses are preserved; no new
project-wide license has been selected.

## A53 Linux analytics continuation

On2026-11-06, firmware0.3.16-ai-linux-dev and Linux6.6.40 were loaded onto the
camera through JTAG. [AI implementation and controls](SW/ai/README.md) cover
YuNet faces, SFace gallery comparison,80-class YOLOX objects, plate candidates/
CRNN OCR, motion, pedestrians, QR and tracking. The R5 remains the sole GenICam
and GigE authority. Empty enrollment folders and a hash-verified model downloader
are included; model weights and captured/user images are outside this repository.

Bounded RGB640x480 runs with20 us packet spacing delivered1232 complete frames
across AI-off/face/object/all-feature cases, about14.293 FPS, without observed
missing/incomplete frames, drops or CSI errors. Inference on one A53 is slower:
about0.31 seconds for faces,8 seconds for objects and9.6 seconds for all features.
Static-pattern Linux DDR copy and GVSP bytes match by CRC32 after acquisition
stops. These checks do not establish optical accuracy, simultaneous frame-identity
proof, all-mode coherency or endurance. See [machine-readable scope](SW/tests/ai_runtime/evidence/board-summary.json).

MicroSD BOOT.BIN/FAT32 disk-image build scripts and a volatile JTAG loader are
provided. The local image passes FAT/readback and QEMU checks; physical MicroSD
boot remains untested. No Flash or saved UserSet changes were made by this work.
