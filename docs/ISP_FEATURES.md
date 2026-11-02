# ISP feature guide

This guide describes actual processing, its controls and evidence limits.
The feature catalog and matching firmware enforce legal values and capability
availability; a GUI control alone does not prove that a particular FPGA contains
the block. All controls below are stopped-only unless explicitly noted. Stop,
wait for `SensorBusy=false`, configure, then restart to reset temporal history.

## RAW correction and color formation

**Black level.** `FpgaBlackLevel` subtracts a 0..1023 DN offset from RAW10 and
clips at zero before bad-pixel correction and gains. It removes a pedestal, not
random noise; excessive subtraction crushes shadow detail. The qualified live
path latches the requested scalar for a subsequent complete HLS frame without
restarting acquisition. Readback is not a frame-tagged applied value. Check
`FpgaBlackLevelLiveActive`; see [live conditions](FPGA_LIVE_BLACK_LEVEL.md).

**Bad-pixel correction.** `FpgaBadPixelCorrectionEnable` enables the AMD Bayer-domain
local correction before demosaic. It operates on neighboring CFA samples; it is
not a user-calibrated list of defective sensor addresses. A natural scene without
known defects cannot establish detection accuracy or removal of a real hot pixel.
Preserve both injected-defect numerical tests and separate optical evidence.

**Lens shading.** `FpgaLensShadingEnable` applies four independent BGGR-phase
7x9 gain grids after black/BPC. Bilinear interpolation uses Q12 gains, rounded
and saturated to RAW10. `FpgaLensShadingPhase`, row/column selectors and
`FpgaLensShadingGain` access the 252 gains; unity is 4096 internally, maximum
4x. The default grid is unity and disabled. This corrects brightness/color
falloff, not geometric distortion. A real flat-field calibration and held-out
validation are needed; see [calibration workflow](LSC_CALIBRATION.md).

**Demosaic and gains.** The color path uses AMD BGGR demosaic into 10-bit RGB.
`FpgaBalanceRed`, `FpgaBalanceGreen` and `FpgaBalanceBlue` use Q8.8 before color correction and RGB8 quantization.
The RAW-preserving path instead applies Bayer-phase gains without producing RGB.
Sensor RAW orientation is fixed for this pipeline; native sensor YUV can have a
different mirror setting. Compare geometry/orientation before pairing images.

**White balance.** `FpgaBalanceWhiteAuto=Continuous` uses admitted preceding-frame
RGB statistics with green fixed at unity. Red/blue target ratios are bounded
0.25..4x and quarter-step smoothed; inadequate dark/saturated samples hold gains.
Restart/entry into auto resets gains. This gray-world assumption can fail on a
strongly colored scene. Sensor `BalanceWhiteAuto` and FPGA white balance are
different processing paths; enabling one does not configure the other.

## Color and detail processing

**CCM.** `FpgaColorTransformationEnable` enables an optional 3x3 matrix plus offsets that transform 10-bit channels before
quantization. Indexed coefficient registers configure the complete transform;
do not leave a partly edited matrix active. Identity is a bypass-equivalent
starting point, while a diagonal red/blue demonstration is not calibrated color.
Reference exact fixed-point register ranges in `gige/FeatureReference.md`.

**Gamma.** `FpgaGammaEnable` selects three 256-entry RGB8 LUTs accessed through
channel/index/value nodes; all 768 bytes are part of the preset. Identity LUT is
the default. The working gamma1.8 example uses `round(255*(x/255)^(1/1.8))`.
This is a display mapping after precision reduction, not recovered dynamic range.

**Median.** `FpgaMedianEnable` applies a 3x3 per-channel median with replicated
borders after gamma. It can suppress isolated impulses and also erase small
texture. It is not a calibrated Gaussian-noise estimator.

**Bilateral.** `FpgaBilateralEnable` uses AMD 3x3 RGB bilateral filtering after
median. `FpgaBilateralSigmaColor` is 1..255 DN (default24);
`FpgaBilateralSigmaSpace` is 0.25..4 pixels (default1). Replicated borders and
range-dependent weights preserve some edges, while large sigma can blur texture.
Evaluate full-resolution crops, not only downsized contact-sheet thumbnails.

**Sharpening.** `FpgaSharpenStrength` ranges0..1. The AMD filter2D kernel has
center `1+4s`, four axial neighbors `-s`, and zero corners, with saturated RGB8
output. Zero bypasses. Constant-zero borders can brighten image edges; halos and
noise amplification are expected tradeoffs, not proof of new optical resolution.

## Geometry and output

`FpgaInputWidth/Height` select the captured input geometry for the FPGA route;
`FpgaRoiEnable` and ROI offset/size nodes crop **after demosaic**, so their phase
rules differ from a RAW sensor crop. Output `Width/Height` describe delivered
payload dimensions. Disabling scaling can change sensor field of view rather
than simply showing the same full-field image at native resolution.

`FpgaScalerInterpolation=Area` integrates source overlap weights for antialiased
reduction. `Polyphase` selects the instantiated six-tap, 64-phase AMD VPSS;
large reductions first use an Area prefilter. The software limit is4x enlargement
per axis within2592x1944 and format alignment; mixed axes are supported in RTL.
The November1 camera captured640x480 centered ROI to1280x960 (2x) successfully.
320x240 ROI to1280x960 (4x) timed out: that mode is **not hardware-qualified**.
An enlarged raster interpolates pixels and does not recover new scene detail.
See [upscale implementation](FPGA_UPSCALE.md) and the current capture error evidence.

RGB/BGR packing preserves channel order as requested. FPGA YUV uses BT.601 with
`FpgaYuvRange=Limited` or `Full`, averaged chroma for422 and2x2 chroma for420.
The native sensor YUV bypass has its own timing and level interpretation. A wrong
PFNC/byte order or range can create false color/brightness in a viewer.

## Contrast, statistics and new tone candidate

**CLAHE.** `FpgaLocalContrastEnable` applies a bounded AMD-derived 4x4 tile CLAHE
to RGB value `max(R,G,B)` after scaling. RGB channels are rescaled by the mapped
value ratio with fixed-point rounding; black remains black. Clip multiplier1..8
defaults4. Enable requires dimensions at least32, height multiple4 and width
multiple8. Histograms from the preceding frame drive the next frame; the first
frame after start, clip/geometry change or reset bypasses. High strengths can
amplify noise and react to illumination changes. The real-scene examples show
that behavior, not a universal image-quality improvement.

**Statistics.** `FpgaStatisticsLatch` snapshots complete frame statistics for
stable multi-register reads. Focus/motion are scene diagnostics, not an AI model;
illumination changes can look like motion and noise can increase focus metrics.
First-frame motion is invalid after restart. Do not infer identities or calibrated
distance from these metrics.

**Adaptive tone (v11).** The previous-input-frame weighted luma mean
drives a common rational RGB8 curve with gain0.5..2 and maximum1/16 gain change
per frame. Target16..224, endpoints fixed, first/reset frame unity. Controls are
`FpgaAdaptiveToneEnable` and `FpgaAdaptiveToneTarget`; mean/gain status is unlatched
and not an atomic frame-associated pair. Full routing and a subsequent bounded synthetic board profile pass. It was
not loaded for the November 1 natural-scene photo collection; those images do
not demonstrate this block. Board checks use 20 us packet spacing, first/last
pixel oracles and approximately 14.293 FPS. See [algorithm/tests](ADAPTIVE_TONE.md).

## Evidence and examples

[Real camera contact sheet](images/contact-sheet.png)
and [parameter manifest](images/manifest.json)
contain19 representative images from48 saved originals. The README compares images 02 and 03: both are 640x480 RGB8; saved FPGA
settings differ only in gamma enable and sensor settings match. This is a useful
display-curve demonstration, not a calibrated color or dynamic-range comparison.
Only two frames were retained per variant, so auto/temporal modes are not proven
converged. Fine denoise/sharpen differences require native-resolution inspection.
Use the linear FPGA baseline for fixed-parameter variants; the native YUV reference differs in
orientation, sensor processing and auto exposure. 4x failure and initial cleanup
failure are preserved separately. The collection does not demonstrate CCM/LSC/BPC,
true HDR or v11 adaptive tone. Additional images need a new usable scene/window.

Bounded distortion control/lifecycle is now linked in firmware0.3.14, with
640x480 nearest-only +/-7-row maps and native full-resolution bypass. Ten short
RTL calls and a delayed-read quiescence monitor pass. Combined v12 routing passes
at200 MHz with43841 LUT/124 BRAM36/235 DSP. Corrected v12 board identity and
+/-5 horizontal,+/-7 vertical maps pass exact pixels/borders at about14.294 FPS
with zero losses and no new CSI/recovery events. Restart, disable and geometry
invalidation pass. These short synthetic checks do not establish optical
calibration or endurance. See
[remap usage and limits](REMAP_CONTROL.md). HDR's generation/lease contract and
bounded CPU worker pass offline tests, but sensor-effective exposure association
is unproven; see [capture gates](OFFLINE_CAPTURE_GATES.md).
No synthetic or AI-generated image is presented here as a camera photograph.
