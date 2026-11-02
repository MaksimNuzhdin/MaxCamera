# MaxCamera real-scene feature examples, November 1, 2026

Captured on firmware0.3.12-upscale-dev / FPGA v10. These are real saved camera
frames, not AI-generated images or host-simulated ISP effects. contact-sheet.png
contains one representative frame from each completed variant plus the native
sensor reference. Full-resolution PNGs and source metadata are in this folder.

Use fpga-linear-baseline as the comparison reference for FPGA feature variants.
The native YUYV reference uses a different sensor ISP/orientation and automatic
exposure; it is NOT a matched before image. FPGA RAW orientation is mirrored
relative to that reference. Matched series use the recorded manual exposure/gain.
AWB and CLAHE have temporal history; two saved frames do not establish endurance
or a statistically controlled optical study.

Visible effects include brightness/gamma, black subtraction, channel gain/color
balance, detail/noise processing and local contrast. Strong CLAHE also emphasizes
texture/noise and clips some highlights. The scene is not a calibration target;
small thumbnails do not establish denoising or defect-correction quality.
The 2x upscale is a centered ROI enlargement, not recovered optical detail.
The 4x attempt failed before saving frames; it is absent. No new adaptive-v11,
CCM, LSC, BPC or HDR result is claimed by this collection.

PNG generation only decodes explicit packing, using full-range YUYV conversion
for the native reference. Contact-sheet thumbnails preserve aspect ratio.
No automatic enhancement or per-image exposure normalization was applied.
Original binary frames are retained with SHA256 and frame/timestamp metadata.
See manifest.json for exact per-case parameters and source identities.

The 4x upscale attempt first timed out without a frame. Its cleanup then
encountered GVCP access-denied during stop and initial restoration. Initial cleanup failed; a later safe restoration matched the complete saved
configuration. Final readback: owner0, SensorReady1, SensorBusy0, native2592x1944
YUYV, FPGA processing off. See restoration-verification.json and
released-after-restoration.json in the archive. No heartbeat/wire trace proves
a single exclusive root cause; firmware recovery releases control privilege.
No Flash/boot change or persistent UserSet save was performed.
