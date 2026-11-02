# ISP capability and acceptance guide

Scope: **v9 RAM-only functional acceptance, 2026-10-27**. This guide consolidates
existing evidence; writing it performs no camera test or optical calibration.
It addresses the integration/control/qualification checklist in TODO (archived development reference; see [verification scope](VERIFICATION.md)).
The image is not an unconditional production release or formal GenICam/GigE
Vision certification. Keep the v8 rollback available.

## Identify the accepted image

- Firmware: `0.3.9-contrast-dev`; PL signature `0x49535009`, capabilities `65535`.
- ELF SHA256: `6d2d240c78f5dd4bf758cce53f9ede1268c7b5880e45b33f7ed29b629ee82529`.
- Bitstream SHA256: `97400b23d8a13bbc0d9f4a32779650bd587b7baa781eaeae80138969b6e7c589`.
- Qualification and limits (archived development reference; see [verification scope](VERIFICATION.md)),
  frozen sources and artifacts (archived development reference; see [verification scope](VERIFICATION.md)),
  independent camera-evidence review (archived development reference; see [verification scope](VERIFICATION.md)).

Inspect `FpgaProcessingVersion`, `FpgaProcessingCapabilities` and each stage's
availability node before configuring a camera. Current working-tree features or
standalone HLS prototypes are not necessarily present in this image.

## Baseline completeness

All `Fpga...` names below are custom nodes. Their exact units, selectors, bounds
and availability are defined by the [feature catalog](../SW/tools/gige_feature_catalog.py)
and generated [camera XML](../SW/gige/MaxCamera.xml). Controls are configuration
values, not claims that a calibrated profile exists.

| Function | Implemented path and primary controls | Evidence / remaining work |
| --- | --- | --- |
| Black-level correction | Shared RAW10 subtraction; `FpgaBlackLevel` (0..1023 DN) | Exact RAW/color regression passes. Dark-frame offset versus exposure/gain/temperature is not calibrated. |
| Defect-pixel correction | Bayer BPC; `FpgaBadPixelCorrectionEnable` | RTL and deterministic camera behavior pass. Actual hot/dead-pixel, temperature and fine-detail campaign remains open. |
| Demosaic and manual WB | BGGR to RGB; `FpgaBalanceRed`, `FpgaBalanceGreen`, `FpgaBalanceBlue` | Shared preprocessing keeps color pre-gains unity; RGB gains follow demosaic. Legacy/shared pixel equivalence and camera checks pass. |
| Automatic WB | Sensor controls on supported sensor paths; separate FPGA gray-world `FpgaBalanceWhiteAuto` | Bounded FPGA feedback and statistics pass. Sensor Continuous AWB is unavailable on the current RAW path. Neither algorithm has scene-specific color calibration. |
| Exposure/gain automation | Sensor AE/AGC with cached telemetry | Lower-rate sensor-driven freshness and lifecycle pass. Maximum LE16 telemetry is deliberately deferred; do not infer freshness from host writes. |
| Color matrix and gamma | `FpgaColorTransformationEnable` with matrix/offset selectors; `FpgaGammaEnable` with channel/index/value LUT | Coefficient/LUT and exact camera regression pass. A color-target CCM and tonal profile remain uncalibrated. |
| Denoise and sharpening | `FpgaMedianEnable`, `FpgaBilateralEnable`, sigma controls, `FpgaSharpenStrength` | Integrated arithmetic, controls and FPS checks pass. Natural-scene noise, halos and detail retention remain unqualified. |
| Lens shading | Four Bayer phases, 7x9 Q12 grid; `FpgaLensShadingEnable`, phase/row/column/gain | Grid, migration and RAW/color tests pass. [Offline fitter](../SW/tools/calibrate_fpga_lsc.py) exists; actual optics require a supplied uniform field and independent validation. |
| Crop/downscale | `FpgaRoiEnable`, ROI/input dimensions, `FpgaScalerEnable`, `FpgaScalerInterpolation` | Area and six-tap/64-phase polyphase paths have exact reference and camera checks. Ratios above 4:1 use an area prefilter. Single output only; no added sensor detail. |
| RGB/YUV output | `PixelFormat`, `FpgaYuvRange` | RGB/BGR and supported YUV byte order, range/chroma and post-CLAHE packing pass. Legacy YUV420 is project-specific; inspect the advertised format. |
| Local contrast | `FpgaLocalContrastEnable`, `FpgaLocalContrastClipLimit` (1..8) | CLAHE arithmetic, bypass, geometry, restart/reset history and short FPS acceptance pass. Uses previous-frame statistics; real-scene noise/illumination stability is not qualified. |
| Quality/focus/motion statistics | `FpgaSceneAnalysisEnable`, `FpgaStatisticsLatch`, focus/motion/intensity/clipping/darkness outputs | Numerical statistics and coherent snapshots pass. Calibrated thresholds, regional events, covered-lens detection and autofocus are not implemented by these metrics. |

Implementation entry points: [RAW/color HLS](../HW/hls/camera_isp/camera_isp.cpp),
[CLAHE HLS](../HW/hls/camera_isp/local_contrast.cpp),
[firmware controls](../SW/gige/GigeFpgaIsp.c),
v9 control tests (archived development reference; see [verification scope](VERIFICATION.md)).

## Configuration and persistence contract

1. Stop acquisition and wait for `SensorBusy` to clear before changing image
   controls, tables, ROI, output format or dimensions. Streaming writes are
   locked; rejected writes must not be treated as successful configuration.
2. Select the supported processing/output path. `FpgaProcessingEnable` is the
   master control; `FpgaProcessingActive` reports whether processing actually
   participates in acquisition. Availability alone does not mean enabled.
3. For CLAHE, choose color processing and output dimensions at least the public
   160x120 minimum, with height divisible by four and width divisible by eight.
   Configure clip limit while stopped. Unsupported geometry is rejected.
4. Start acquisition. The first CLAHE frame after acquisition/reset bypasses
   correction while building history. Subsequent frames use the previous frame's
   histogram. Do not compare that first frame to steady-state mapped output.
5. Use `FpgaStatisticsLatch` for a coherent snapshot, and inspect sequence/valid
   fields. Motion is invalid until a preceding frame exists. A focus score is
   not a calibrated focus/quality verdict.

The [preset codec](../SW/gige/GigePreset.c) stores ISP9 configuration, CCM/offsets,
gamma tables and lens-shading grid. Older profiles decode with newer fields at
defaults; pre-v9 profiles disable CLAHE and use clip limit four. Read-only
statistics and capabilities are not persistent configuration. Offline migration,
journal/CRC and torn-save model tests passed. **Actual v9 UserSet save/load was
not exercised**; protected Default/factory operations remain outside this
acceptance. Do not equate codec tests with a board persistence campaign.

For future profile work, retain the exact image identity, starting settings,
selected controls/tables and capture conditions. The accepted campaign restored
original runtime settings with processing disabled and preserved all eight valid
UserSets, without Flash writes.

## Performance and budget

| Tested geometry/format | CLAHE disabled FPS | Enabled FPS | Window per capture |
| --- | ---: | ---: | ---: |
| Full-sensor downscale to 640x480 RGB8 | 14.293891 | 14.294144 | 5 s |
| 2592x1944 RGB8 | 6.613756 | 6.613535 | 5 s |
| 2592x1944 YUV422 | 9.922358 | 9.922096 | 5 s |
| 2592x1944 legacy YUV420 | 13.224857 | 13.224542 | 5 s |

No material CLAHE FPS reduction, camera drops or CSI errors occurred in these
tests. They are short bounded measurements, not endurance or unrestricted
performance guarantees. Native VGA sensor YUV near 30 FPS is a different path
from full-sensor FPGA downscale near 14.29 FPS. Full-resolution RGB/YUV delivery
is constrained by the configured 1GbE payload budget; do not advertise the sensor
frame rate as delivered RGB frame rate.

The routed 200 MHz design uses **39,045/47,232 LUTs, 106/150 BRAM36 equivalents
and 233/240 DSPs**: remaining 8,187 LUTs, 44 BRAM36 and only seven DSPs. Setup,
hold and pulse-width slack are +0.451/+0.003/+0.145 ns. These margins belong to
this build, not to a future image with another stage. Older RAW-only resource
figures are historical, not current headroom. Any additional stage needs a new
combined resource/timing assessment.

CLAHE has one frame of adaptation history. The standalone maximum frame interval
estimate is 10,081,822 cycles (50.409 ms at 200 MHz); this is not measured
camera-to-host latency. Acceptance captures record first-complete-frame and
command timing, but those include sensor setup, transport and host scheduling.
No general end-to-end latency guarantee has been established. The single-output
implementation does not qualify simultaneous resolutions or an independent AI
branch's DDR/buffer/backpressure budget.

## Remaining baseline gates and next work

The highest-value remaining baseline work is calibration and usability of the
existing stages, not adding more unbudgeted PL blocks. Existing TODO gates are:

- Optics-specific sensor tuning, dark-level/CCM/flat-field calibration and
  independent validation. No suitable calibration scenes or reference values
  were established by the color-bar campaign. The LSC fitter does not capture
  images or program the camera; use supplied captures only when that work is
  authorized and available.
- Real-scene BPC/detail/CLAHE quality, illumination changes and noise behavior;
  numerical correctness alone cannot close these gates.
- Separate global tone mapping, lens distortion correction and synchronized
  multi-exposure HDR remain unfinished. CLAHE is neither calibrated HDR capture
  nor completion of all tone mapping. Standalone feasibility is not deployment.
- Maximum-rate telemetry freshness, actual v9 persistence acceptance and
  longer-duration qualification remain explicitly open under their existing
  deferrals/authorization boundaries.

Next actionable software milestone: review the existing calibration workflow's
input provenance, validation outputs and profile handoff before any calibration
claim. Keep it offline until suitable owner-supplied captures and reference
conditions exist. This is distinct from collecting a training dataset.
The [offline LSC workflow](LSC_CALIBRATION.md) now records content provenance and
optional held-out prediction metrics; neither constitutes optical qualification.

Pretrained AI assessment follows baseline consolidation. No model is adopted,
downloaded or executed by this guide; no dataset or custom training is authorized
here. Seven spare DSPs do not establish usable inference capacity. Model/operator,
license, quantization, memory, runtime ownership and latency assessment must
precede any architecture or deployment decision.

## November 2 PPC4 integration checkpoint

The0.3.15/v13 pair retains bounded remap, adaptive-tone, live-black and logical
stream-LED regressions. RGB4x320x240 to1280x960 now passes57 frames at14.293804
FPS with20 us packet spacing and one saved-frame exact Q12 oracle. Seven saved
native/scaled format frames match v12. This does not qualify every enlargement
geometry, optical calibration or endurance. Routed resources:45832 LUT,126 BRAM36,
235 DSP at200 MHz; WNS+0.429 ns and WHS+0.011 ns. Original runtime restored and
camera released; no persistent writes. See [bounded upscale evidence](FPGA_UPSCALE.md)
and board state (archived development reference; see [verification scope](VERIFICATION.md)).
