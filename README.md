# DinxTech MaxCamera

A source-prepared GenICam/GigE Vision camera portfolio project for the **ALINX
AXU2CGB** (XCZU2CG) and **ALINX AN5641** camera module (OV5640 sensor).
The R5 firmware owns camera control, acquisition and UDP image transport; the PL
implements the bounded ISP. This is an engineering portfolio, not formal protocol
certification or an end-to-end AI/HDR product.

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
awaiting frame-effective exposure verification; no AI model is deployed. Wider4x
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

## Browser camera console

The optional [MaxCamera Vision Console](webapp/README.md) provides authenticated browser preview and named runtime controls through an isolated Linux gateway. See the [complete user guide](webapp/USER_GUIDE.md), [security model](webapp/SECURITY.md) and [bounded verification](webapp/VERIFICATION.md). It does not replace camera firmware or expose GigE ports to the Internet.
