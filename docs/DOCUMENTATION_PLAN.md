# Portfolio documentation inventory

The documentation phase now includes 12 [per-driver READMEs](drivers/README.md),
[architecture](ARCHITECTURE.md), [build/use](BUILD_AND_USE.md), and
[ISP explanations with real camera examples](ISP_FEATURES.md). This inventory
retains remaining publication gates; it does not imply clean-checkout reproduction
or complete optical coverage. Feature status follows verified evidence.

## Hardware identity evidence

The owner confirmed **ALINX AXU2CGB** and **ALINX AN5641** on November 1, 2026,
with the official [board page](https://www.en.alinx.com/Product/SoC-Development-Boards/Zynq-UltraScale-plus-MPSoC/AXU2CGB.html) and
[module page](https://www.en.alinx.com/Product/Add-on-Modules/AN5641.html). The web reader opened both URLs but returned no
extractable specification text; no new specifications were copied from those
pages. Product identity comes from the owner, not an inferred directory name.

The companion `AXU2CGAB_User_Manual.pdf` covers the A/B board family.
`AN5641_User_Manual.pdf` identifies an OV5640 sensor and a 15-pin, two-lane MIPI
interface. The actual FPGA project targets `xczu2cg-sfvc784-1-e`. These documented
interfaces do not imply that every board peripheral or vendor demo is used by
MaxCamera. OV5640 is the sensor; AN5641 is the module. Vendor PDF redistribution
rights remain separate from factual identification and linking to product pages.

## Delivered driver guide coverage

| Source | Earlier evidence | Details covered by the new guide |
| --- | --- | --- |
| `Ov5640.c/.h`, `Ov5640Registers.h`, `Ov5640Tuning.inc` | `OV5640_README.md` | Transport contract, mode/PLL/window APIs, grouped exposure, stream lifecycle, supported formats and source licensing |
| `MaxIicDriver.c/.h`, `Ov5640MaxIic.c/.h` | `I2C_README.md` | PS/AXI bus selection, clock inputs, errors, sensor adapter and serialization |
| `AxiIicPolled.c/.h` | Transport experiments and source | Register sequencing, timeout and recovery, actual active/fallback status |
| `AxiGpioSccb.c/.h` | SCCB evidence and source | Open-drain GPIO wiring, synchronous bus ownership, timings and error return |
| `AxiGpioSccbAsync.c/.h` | `tests/sccb_async/README.md` | Nonblocking state machine, cancellation drain, synchronous exclusion and telemetry scheduling limits |
| `../drivers/LedsController/` | LED tests/evidence | Active-low masks, status mode, stream cadence and v10/v11 versus legacy reset alias |
| `../drivers/MaxFanController/` | Source only | Threshold/floor/duty registers, temperature encoding; self-test overwrites configuration and does not restore it |
| `../drivers/TempController/` | Source only | GPIO raw conversion, dependencies; current initializer logs failure but still returns OK and needs documented review |
| `../drivers/MaxTimer/` | Source only | Timer index/frequency, wrap, shared global instance, start/reset side effects |
| `../drivers/UART/` | Source only | BSP lookup, caller-owned XUartPs, baud setup; current wrapper ignores SetBaudRate status |
| `gige/GigeFpgaIsp.*`, polyphase/output/contrast register adapters | Feature docs and native tests | Signature/capability ABI, MMIO map, stop-only versus live controls, history reset and fallback |
| `gige/GigeVisionBoard.*` and camera integration | GigE docs and evidence | R5 ownership, DDR/cache/DMA lifetime, acquisition/recovery, timing and resource dependencies |

Do not silently rewrite drivers while documenting them. Each README
must distinguish its public contract from known source limitations and link an
actual build/test procedure. External sibling driver directories must be packaged
or documented as explicit dependencies for a reproducible public checkout.

## ISP explanation and evidence

Cover RAW black level and defect correction, LSC, demosaic, channel gains/AWB,
CCM, gamma, median/bilateral denoising, sharpening, ROI, Area and Polyphase
resampling, RGB/YUV packing, CLAHE and adaptive tone. Distortion/HDR independent
components must remain separate from deployed features. Each guide needs pipeline
position, algorithm/precision, legal range/defaults, GenApi names, lock semantics,
history/persistence behavior, bandwidth/resource cost and verified limitations.
AI and model training are not currently deployed camera capabilities.

## Real before/after image examples

The authorized November 1 scene window produced 48 originals across the native
reference and 18 completed feature variants. Source/settings manifests and a
contact sheet are saved under `diagnostics/scene-originals-20261101/`; the bundle
was delivered privately to Library. 4x upscale failed and cleanup was subsequently
restored exactly. Additional CCM/LSC/BPC/HDR/adaptive examples need a new suitable
window. Synthetic RTL patterns must never appear as natural-scene quality evidence.

For every comparison preserve original captures plus a machine-readable manifest:

- Capture UTC, source frame identifier/timestamp, transport completeness/counters.
- Exact firmware/FPGA hashes, receiver version, pixel packing, input/output sizes,
  sensor ROI/orientation and all effective ISP parameters, including hidden LUTs.
- Sensor exposure/gain/white-balance state and whether fixed or allowed to adapt;
  settling/history interval and one deliberately changed feature.
- Scene/lens/focus/illumination description, comparison limitations and file hashes.
- Preview conversion command, color interpretation and any crop/display transform.

Reject motion/exposure changes that confound a claimed feature effect, clipped or
defocused scenes that hide detail, and resolution/packing mismatches. Explain the
specific deficiency and request a reshoot; do not manufacture an improved image.
Keep identifying/private scene content out of a public release unless approved.
Publication and license/rights review remain separate from local documentation.
