# Verified full-field FPGA preview: UserSet7

Tested on the running FPGA version 10 / firmware 0.3.10-led-dev pair. UserSet7
was saved once and verified by native load from a deliberately different safe
configuration. Default/startup selection remains zero. No Default load, firmware
Flash write, boot change or factory unlock was performed. The only persistent
change is the explicitly requested UserSet7 record in the user-preset partition.

## Load the working image

Stop acquisition and wait until SensorBusy is false. Set UserSetSelector to
UserSet7, execute UserSetLoad, wait until UserSetBusy is false and check
UserSetLastError = 0. Start acquisition. Do not change UserSetDefault.

The preset selects RGB8 output 640x480, full-array input 2592x1944, ROI disabled,
Xilinx six-tap/64-phase Polyphase downscaling, BPC on, FPGA black level zero,
manual base RGB gains 1/1/1, FPGA gray-world AWB Continuous, a complete identical
per-channel display gamma-1.8 LUT, and scene statistics. Sensor AE/AGC are
Continuous; sensor AWB is Off on the RAW processing path. CCM, lens shading,
median, bilateral, sharpening and CLAHE are off in this conservative preview.
FPGA AWB applied gains can differ from the configured unity base gains and
settle over the first frames. This is a usable preview, not optical calibration.

The first three loaded frames were valid and visibly nonblack. The subsequent
six-second capture delivered 86 good frames at 14.297787 FPS, with no bad or
missing packets, camera drops or CSI errors. See the loaded frame (archived development reference; see [verification scope](VERIFICATION.md))
and save/load result (archived development reference; see [verification scope](VERIFICATION.md)).
The preset requires a firmware/profile codec supporting the ISP9 extension and
the corresponding FPGA processing path; validation applies to the current pair.
The firmware/FPGA deployment remains volatile; startup selection was not changed.

## Reproduce setup without inheriting dormant state

The tested implementation is configure_fpga_scaler.apply_working (archived development reference; see [verification scope](VERIFICATION.md)).
It requires stopped acquisition and performs no save/load itself. In order it:

1. Disables master processing, compression, frame-rate limiting and optional
   processing flags before staging the output geometry.
2. Stages 640x480 RGB8 through YUV422_8, then explicitly clears test patterns,
   rolling/transparent/monochrome test options and fixes RAW orientation.
3. Writes black level 0, base R/G/B gains 1/1/1, BPC enabled, full-array input,
   full-size zero-origin ROI parameters, known filter defaults and disabled ROI.
4. Writes every matrix/offset value, all 768 gamma LUT bytes and the complete
   unity lens-shading grid. The gamma curve is round(255*(x/255)^(1/1.8)) for
   x=0..255, repeated for red, green and blue. Enabling an identity LUT alone
   does not produce this display transform.
5. Enables scaler, Polyphase, gamma, FPGA AWB Continuous, scene analysis and
   master processing. It reapplies valid selected timing to activate the RAW
   full-array sensor geometry while stopped, then configures exposure/gain
   limits and Continuous AE/AGC. AcquisitionFrameRateEnable remains false.

The offline replay regression (archived development reference; see [verification scope](VERIFICATION.md)) seeds
old 0.5/0.75/1 gains, black level 31, stale matrix/gamma/lens tables and ROI flags.
It proves that the recipe replaces those states rather than relying on a prior
application run. It also proves that a busy camera is rejected before any write
and that the recipe does not issue UserSet save/load commands.

## Findings and limits

The reported state had red/green/blue gains 0.5/0.75/1, black subtraction 31 and
an enabled identity gamma LUT. Natural captures reproduced a blue/dark image.
Neutral gains, a display curve and FPGA AWB provide a visible full-field image.
A steady all-black transport failure was not reproduced: the original scaled
capture already contained a complete scene without packet/CSI errors. Do not
claim that every possible GUI/startup black-frame cause was diagnosed.

With scaler disabled, firmware uses output dimensions as RAW acquisition input;
FpgaInputWidth/Height apply when scaling is enabled. Therefore disabling the
scaler is not a neutral full-field bypass: the observed 640x480 RAW image has a
narrower field. Disabling master processing selects the sensor's own ISP path,
which has different color/tone behavior. The natural reference and FPGA preview
are not colorimetrically or tonally identical. Highlight clipping remains in
this high-contrast scene; gamma/AWB is not HDR or lens/color calibration.

## Backup and rollback

All seven non-default slots were exported to JSON before the save. Old UserSet7
and untouched UserSet1 were compared across all supported persistent sensor,
geometry, timing, FPGA scalar and table fields. Native UserSet1 load after a
valid RAM-only perturbation restored the old UserSet7 configuration exactly.
UserSet1 was not saved or overwritten; retain it as the native rollback anchor.

AE_TARGET is a derived scalar: the original six AE thresholds
[48,40,96,48,38,20] read as target 44, but writing target 44 does not reconstruct
those six bytes. The initial host replay correctly failed its exact check.
The corrected replay loads a preserved native anchor and does not regenerate
AE_TARGET; it retains exact comparison of the raw thresholds and timing.
No protected raw-register whitelist was bypassed. JSON alone is not advertised
as a universal independently restorable profile without the required anchor.

To recover the old UserSet7 configuration, stop acquisition and load untouched
UserSet1. Original runtime and per-slot JSON backups are in the evidence bundle.
Do not overwrite UserSet1 while it is needed as rollback. A saved preset does not
include stream packet settings; original packet length/delay were restored.
