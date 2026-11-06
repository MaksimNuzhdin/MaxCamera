# Camera software

This directory is the R5 bare-metal application plus local drivers. `gige/` owns
GVCP/GVSP, GenApi metadata, capture and ISP controls; `drivers/` contains LED,
timer, UART, temperature and fan support. The application entry point, OV5640
and SCCB transport sources are in `src/`, alongside the firmware CMake
configuration. [Driver guides](../docs/drivers) document APIs.

## Platform and firmware

Use Vitis2024.2 to create an embedded platform from the matching HW-generated
XSA, with standalone domain `standalone_psu_cortexr5_0` on `psu_cortexr5_0`.
Generate/export its BSP, linker/toolchain metadata and platform boot components
using the vendor tools. Do not import the old workstation platform or copy its
absolute paths. The application linker configuration reserves DDR from0x00100000;
review it against the exported hardware. Existing FSBL/PMUFW startup and processor
ownership must be preserved when loading; this package does not modify boot media.

```bash
export VITIS_ROOT=/path/to/Vitis/2024.2
export MAXCAMERA_PLATFORM=/path/to/platform/export/platform/sw/standalone_psu_cortexr5_0
bash SW/build.sh
```

`build.sh` uses the Vitis-bundled CMake/Python environment and Ninja, links the
vendor BSP plus its lwIP2.2.0 sources, and enables the RAW10/FPGA ISP configuration.
BSP/SDK paths are explicit environment inputs, not committed source. The export
was compiled/linked against the existing qualified platform; creation of a new
BSP is a separate unrun reproducibility gate. The result is not board-qualified
merely because it links. No loader runs automatically.

## Offline tests and host tools

From `SW/`:

```bash
bash tests/gige/run.sh
bash tests/remap/run.sh
bash tests/sccb_async/run.sh
bash experiments/hdr/run_tests.sh
python3 tests/test_distortion_maps.py
```

C/C++ compilers, Python3 and sanitizer runtimes are required. NumPy is required
for calibration/map utilities. Optional GenApi schema checking uses `GENAPI_SCHEMA`;
optional Aravis formula execution is skipped unless its runtime is installed.
No offline test command above opens the camera. Hardware-specific test fixtures
and older workstation automation are intentionally not release entry points.

`tools/generate_gige_xml.py` regenerates the committed XML/ROM/register outputs
from the catalog. Those generated text assets are intentional source exceptions;
coherence tests must pass before committing changes. `tools/gige_probe.py`, the
UDP receiver and map uploader are explicit host diagnostics: running acquisition
or upload is a separate action requiring exclusive camera ownership. Calibration
maps are not supplied as if they were measured for the user's lens.

## Experimental A53 Linux/AI

See [analytics controls, presets, build and measured limits](ai/README.md).
Enable firmware integration with `MAX_CAMERA_AI_IPC=ON` when running `build.sh`.
The default remains disabled. Exact models are fetched with
`python3 tools/fetch_ai_models.py --fixtures`; target wheels and the external
AMD/PetaLinux artifacts are documented separately. The tested JTAG deployment
uses0.3.16-ai-linux-dev with the original v13 PL image, not a regenerated FPGA.
