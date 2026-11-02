# OV5640 / ALINX AN5641

RAW10 is now repacked in the FPGA: BayerBG10p and BayerBG10 retain all 10 bits
without per-frame repacking on R5. `tools/run_gige_board.sh` selects the current
matching FPGA/firmware pair by default. Verification results and rate limits:
`diagnostics/raw10-fpga-20261026/README.md`.
Continuous AWB is unavailable in RAW because this path does not provide valid
AWB statistics. Entering RAW initializes manual gains to 1x; older RAW UserSets
load with AWB Off and their saved gains.

October 26 update: formats are integrated into production GenICam/GVSP.
15 PixelFormats and JPEG through ImageCompressionMode are available; 14 formats
were verified at 2592x1944, while YUV444/JPEG are limited to VGA. Native RGB888
remains unqualified; RGB8/BGR8 produce valid output through YUV422 conversion.
Current launch commands, packing, limitations, and evidence:
`diagnostics/genicam-formats-20261026/README.md`.

The driver is written in C with C++-compatible headers. `Ov5640.c` has no Xilinx
dependency: transport and delays are supplied through callbacks.
`Ov5640MaxIic.c` connects it to the existing MaxIIC driver. Use one persistent
context per sensor; callers must serialize access from multiple tasks.

## Startup

```c
OV5640 camera;
OV5640_CONFIG config;
OV5640_RESULT result = Ov5640_BindMaxIic(&camera, &g_MaxIicRef_0);
if (result == OV5640_OK) result = Ov5640_Init(&camera);
Ov5640_DefaultConfig(&config);
if (result == OV5640_OK) result = Ov5640_Configure(&camera, &config);
/* Configure the matching MIPI CSI-2 receiver and DMA first. */
if (result == OV5640_OK) result = Ov5640_Stream(&camera, 1);
```

`Init` performs reset, a 25 ms wait, software standby, and loading of the
original Linux analog/ISP table. The sensor core wakes at the end of the table;
external frame output has already been blocked through 4202/300E. The call
returns READY with output blocked. Use `InitWithClock` for another physical
XCLK; `Configure.clock.xclk_hz` must match it. The early bootstrap PLL has been
removed: only `Configure` programs the PLL, after the base table, as in Linux.
`Configure` also sets the window, scaling, format, interface, AE/AGC, and
anti-flicker settings. AE/AGC are disabled before a mode change and restored at
the end.

MIPI uses the Linux LP11 preparation: 300E=44, 4800[5,2]=1, 3019=70, and a
1 ms delay. Only `Stream(1)` enables frame output; continuous-clock mode clears
gating at startup. `Stream(0)` blocks MIPI and frame output while leaving the
sensor core running; DVP uses software standby. In this document, configuration
API "standby" means no frame output, not necessarily 3008=42.
The `main.cpp` integration finishes with output blocked because CSI-2 and DMA
are not configured yet. MaxIIC adapter transactions are bytewise, with an
explicit register address per byte, matching Linux ov5640_read/write_reg.

Startup mode: **MIPI CSI-2, 2 lanes, VC0, 640x480, YUV422 YUYV**.
Assuming **25 MHz** XCLK: PLL prediv=3, multiplier=92, sysdiv=2, MIPI divider=2;
SCLK=47,916,666 Hz; HTS=1600, VTS=998; calculated rate **30.007 Hz**. This is
nominal 30 fps, not a measured frame rate. The configured input clock must match
the physical sensor signal. An FPGA 25 MHz output alone does not measure XCLK
at the camera module.

`OV5640_CONFIG` explicitly controls PLL, geometry, YUV422 format (four byte
orders), RGB565, RGB555, RGB444, RGB888, RAW8, RAW10, JPEG, native Y8, and DVP/MIPI.
RGB555 uses format2 (4300=81); RGB444 uses format2 (4300=A1). On AN5641 both
modes transmit 16-bit words most significant byte first with CSI DT=2A:
RGB555 `0rrrrrgg gggbbbbb`, RGB444 `0000rrrr ggggbbbb`. The receiver must preserve
raw bytes and use `2*width` line length; standard CSI RGB555/444 types do not
match this output. These are separate diagnostic profiles, not new GigE XML
formats at this historical stage. Y8 selects ISP luminance with 4300=10; the
separate FPGA/DDR experiment and verification criteria are documented in
`diagnostics/pixel-formats-20261025/README.md`. It does not yet replace the GigE
Mono8 extracted from YUV422.
`Ov5640_Raw8VgaConfig` selects the verified AN5641 RAW8 profile: assumed physical
XCLK 12 MHz, unbinned crop, 640x480, HTS2844/VTS1968, nominal 8.933 fps. Call
`InitWithClock(12000000)` first and use an FPGA accepting RAW8. RAW DPC precedes
the ISP scaler: `Configure` rejects RAW8/RAW10 unless the window after
subsampling and subtraction of both offsets matches the output size. The old
scaled VGA profile is unsuitable for RAW8. Results and limitations:
`diagnostics/raw8-20261025/README.md` and
`diagnostics/raw8-stability-20261025/README.md`.
`Ov5640_Raw10VgaConfig` selects the same verified crop for RAW10, at 7.146 fps.
RAW10 uses 3034[3:0]=A. `Ov5640_ConfigSystemClockHz` accounts for bit depth;
the older `Ov5640_SystemClockHz` retains its 8-bit calculation. RAW8/RAW10
switching changes the divider even with the same PLL. Separate 10-bit FPGA/DDR
build, XY10 packing, and results: `diagnostics/raw10-20261025/README.md`.
RAW10 was not yet enabled in the production GigE PixelFormat at this stage.
New sizes require the appropriate analog settings for each mode; the initial
table targets low-resolution/binning. Full-resolution modes and alternative
formats are not verified presets at this historical stage. Single-lane MIPI
is not included in the ready configuration: documentation and practical 300E
values disagree, and the board uses two lanes.

## Common controls

- `SetExposure`: integer lines and `gain16` (16 = 1x), checked against VTS-4.
- `SetAutoExposure`, `SetExposureLimits`, `SetBanding`: AE/AGC, night mode,
  gain/exposure ceilings, 50/60 Hz lighting or autodetection.
- `SetWhiteBalance`: automatic or manual 12-bit R/G/B gains.
- `SetOrientation`, `SetIsp`, `SetEffect`: mirror/flip, ISP enable,
  hue/saturation/contrast/brightness, gray/negative/fixed YUV, UV thresholds.
- `SetGamma`, `SetLensCorrection`, `SetDrawWindow`, `SetTestPattern`.
- `SetJpegQuality`, `SetJpegQuantTable`, `SetJpegComment`, `GetJpegStatus`.
- `SetVcm`: 10-bit DAC, slew, divider/current, RIH, and power-down.
- `SetFrexExposure`, `RequestFrex`: FREX exposure and strobe pulse width.
- `ReadOtp`, `LoadMcu`, `McuCommand`, `SetAddress`, `GetStatus`.

During streaming, small AE/AWB/SDE/orientation updates use SRB group hold and
launch at a frame boundary. In standby the same writes are direct. The Group
API accepts up to 13 distinct registers, groups 0..2, checks for an existing
hold, and rejects reset/clock/OTP changes. Exposure/gain readback while automatic
control is running is not an atomic frame snapshot.

## Datasheet coverage

The driver has two layers: the functions above with units/validation, and
`ApplyFeature(feature, patches, count)` for precise updates to remaining table
fields. A patch contains an address, changed-bit mask, and value; the mask
preserves adjacent service bits. **This is a low-level API, not a complete
algorithm for each datasheet field**: the caller must honor valid values and
read-only/debug bits within the range. Named addresses are in
`Ov5640Registers.h`; multiline/range table entries are not exhaustive, and an
address can be provided directly.

| v2.03 section | API / feature |
|---|---|
| 7.1 system / IO | Init, Configure, Stream, PAD_SYSTEM patches (reset/PLL blocked) |
| 7.2 SCCB / PLL | Bind, SetAddress, Configure.clock |
| 7.3 SRB | GroupUpdate |
| 7.4 AWB gains | SetWhiteBalance |
| 7.5 AEC/AGC | SetExposure, SetAutoExposure, GetStatus |
| 7.6 VCM | SetVcm, VCM patches |
| 7.7 timing | Configure.window, SetOrientation |
| 7.8 AEC/AGC domain | SetExposureLimits, SetBanding, AEC_AGC patches |
| 7.9 strobe / FREX | SetFrexExposure, RequestFrex, STROBE_FREX patches |
| 7.10 light detector | SetBanding, LIGHT_DETECTOR patches |
| 7.11 OTP | ReadOtp, separately disabled ProgramOtp |
| 7.12 MC | LoadMcu, MC patches |
| 7.13 BLC | BLC patches |
| 7.14 frame | Stream, FRAME patches |
| 7.15 format | Configure.format, FORMAT patches |
| 7.16 JPEG | JPEG APIs, JPEG patches |
| 7.17 VFIFO | VFIFO patches |
| 7.18 DVP / CCIR656 | Configure, DVP patches, including JPEG compression modes 1..6 |
| 7.19 MIPI | Configure, MIPI patches for PHY timings/packet options |
| 7.20 ISP frame | ISP_FRAME patches |
| 7.21 ISP top | SetIsp, SetDrawWindow, SetTestPattern, ISP patches |
| 7.22 AWB tuning | AWB patches |
| 7.23 CIP | CIP patches (sharpness, denoise, thresholds) |
| 7.24 CMX | CMX patches (coefficients/signs) |
| 7.25 gamma | SetGamma, GAMMA patches; RAW GMA enable through SetIsp |
| 7.26 SDE | SetEffect, SDE patches |
| 7.27 scale | Configure automatic scaler, SCALE patches for manual DCW/ratio |
| 7.28 AVG | AVERAGE patches (window/weights), Read for statistics |
| 7.29 LENC | SetLensCorrection (62 gains), LENC patches through 5849 |
| 7.30 AFC | AFC patches, Read for focus statistics |

The same interface configures matrix, sharpness, and statistics:

```c
/* Standby only: change 5300[3:0] while preserving all other bits. */
const OV5640_PATCH sharpness[] = {{0x5300, 0x0f, 0x08}};
result = Ov5640_ApplyFeature(&camera, OV5640_FEATURE_CIP, sharpness, 1);
```

Do not change clocks/geometry through low-level patches behind `Configure`:
the cached config is not automatically reconstructed from registers. `Write`
is an escape hatch for any address and requires reinitialization/configuration
afterward. It is unavailable during streaming.

## Features with external prerequisites

Autofocus requires a suitable actuator and MCU firmware. `LoadMcu` uploads up
to 4 KiB of external firmware; `McuCommand` implements the common AF mailbox
3022..3028 with bounded waiting. This mailbox is a firmware ABI, not a datasheet
standard; verify compatibility before use. No ready firmware or AF algorithm is
included. FREX/flash require exposed physical signals and a suitable shutter/flash.

OTP programming is irreversible and requires physical **AVDD 2.1 V +/-5%**.
In a normal build `ProgramOtp` returns E_STATE without transactions. A production
build requires `OV5640_ENABLE_OTP_PROGRAM=1`, explicit `OV5640_OTP_CONFIRM`, and
measured AVDD. The first five manufacturer bytes are protected; only 0->1 changes
are allowed, followed by readback. The procedure is not qualified on AN5641 and
has never been run on the board. A voltage argument neither changes nor measures
the supply.

## Errors and verification

The original transport error and register address are retained. Ordinary write
NACK/timeout is not retried: some data may already have reached the sensor.
Partial configuration puts the context in FAULT and blocks Stream(1); Init is
required for recovery. For 3008 commands (reset/standby/wake), NACK is accepted
only after a 25 ms delay and successful readback of the expected 3008 state and
ID=5640. Such cases increment verified_power_nacks. This does not repeat the
write; other errors are fatal. PLL changes temporarily select SYS divider /16,
then program PLL parameters and the final divider.

Host tests: `bash tests/ov5640/run.sh`; cover packing/masks, clock/fps, bounds,
timeout, groups, recovery, and failure at each Configure transaction. This is a
register model; it does not emulate analog behavior, image quality, PHY, or ISP
processing of real frames. R5 build: `bash tools/build_app.sh`.

## Actual board verification result

2026-10-22: the ARM/R5 ELF built and ran, UART 115200 worked, CAM0 returned
ID=5640, and CAM1 was absent. **Full initialization had not passed**: after
sensor detection, NACKs occurred during sequential SCCB transactions. The first
failing address varied (3008, 3103, 3034/3035, 3631/3601); readback confirmed some
writes after NACK, while other registers retained their old values. Automatically
accepting such failures is invalid. Trials included 10/50 kHz, inter-write delays,
300 ms after reset, bootstrap PLL, and complete FPGA/PS restart. These checks
**do not confirm** configured or streaming camera output. Diagnostic hold/delay/
clock changes were removed from the production adapter; transport remains
standard MaxIIC at 50 kHz. That boot reports the error and enters the main loop
without starting MIPI. Log: `diagnostics/ov5640-sensor-bringup-uart.log`.

In the last run after reset/PLL experiments, CAM0 no longer answered even the
initial ID read (NACK at 300A). Earlier FOUND messages do not describe the final
run. The program remained in its main loop without a camera stream. The next
hardware check must restore the camera's initial state (hardware reset/power
cycle), then inspect SCCB with an oscilloscope or logic analyzer, including ACK,
actual SIOC/SIOD signals, and XCLK.

## Upstream Linux audit, 2026-10-23

Comparison with current torvalds/linux:
`diagnostics/ov5640-upstream-init-audit-20261023.md`.
Earlier results above are history, not the state of the new revision. Aligning
the sequence with Linux did not remove NACKs: the final hardware attempt stopped
at 3008, and the next launch at 300E, before the table load. A software cause is
not excluded; assigning all NACKs to wiring or power would be premature.


## Additional verification, October 23, 2026

The embedded ILA confirmed CAM0 address/data ACKs on digital SCL/SDA. An
independent FIFO test completed 1000 writes to 503D without errors, restored the
original value, and checked readback. The normal application passed Init,
Configure, and GetStatus: HTS=1600, VTS=998, MIPI 640x480 YUYV, nominal 30.007 fps.
This was a separate successful run, not proof that every earlier intermittent
NACK was eliminated. Frame reception was not yet checked; output was blocked.
A repeated full launch of the standard FPGA/FSBL again NACKed at 300E; the camera
was then in FAULT. Details: diagnostics/ov5640-ila-result-20261023.md.


Repeated Init check on October 24: nine full sequential Init/Configure/GetStatus
cycles passed without hardware reset, then a NACK occurred. The hypothesis that
every Init requires a hardware reset was not confirmed. Main/FIFO transport
comparisons and actual reset/wake NACKs are retained in
diagnostics/ov5640-reset-investigation-20261024.md.


## Real-video verification, October 24

A live 640x480 YUV422 image was received: 299 frames and 299 DMA writes in
10.000473 s, with no CRC/ECC/SoT/frame-sync errors during capture. Working AN5641
diagnostic profile: XCLK approximately 12 MHz (inferred from frame period and
vendor Linux DT), PLL200/pre3/sys2, MIPI divider1, continuous clock, sensor UYVY,
HTS1600, VTS1042. The earlier 25 MHz assumption described the FPGA output and
did not establish the clock reaching the sensor.

This was checked with independent FIFO transport and a diagnostic ILA FPGA.
The standard image could still stop at I2C Init. Production defaults were not
replaced by an unverified port. Report, frames, and exact reproduction:
diagnostics/ov5640-video-20261024.md.

The initial black frames were captured with the lens covered, as the user later
confirmed. An image appeared after uncovering it: another 299 frames in 10 s
without CSI-2 errors. The image is blurred; focus/color quality is not yet
qualified. Files and logs are linked in the report.

Native YUV444 (`4300=20`, Y/U/V, CSI DT24) and MIPI-only legacy YUV420
(`4300=5F`, UYY/VYY on adjacent lines, CSI DT1A) were added to the API. YUV420
requires even width/height; DVP for this variant is rejected before register
writes. The experimental 32-bit no-VFB receiver requires swapping byte lanes
1/2 in each DDR word before YUV420 parsing. This is a measured DDR layout and
must not be assumed for another RX. Both modes passed minute-long VGA captures
with color checks. Full table, failed SCCB preparations, and limitations:
`diagnostics/pixel-format-status-20261025.md`.

## Format and GPIO SCCB qualification, October 25, 2026

Current status and limitations are consolidated in
`diagnostics/pixel-format-status-20261025.md`; earlier logs above are historical.
GPIO SCCB (`AxiGpioSccb`, base 0xA00D0000) requires the matching new FPGA image:
separate open-drain SCL/SDA with ACK checking and bounded clock-stretch waiting.
RAW10 and RGB555 passed 10/10 complete preparations without errors, followed by
minute-long capture. Ordinary operations are not retried after NACK. Standard
AXI-IIC remains available for older FPGA images.

`Ov5640_Raw8VgaConfig` / `Ov5640_Raw10VgaConfig` select an unbinned VGA crop;
`Ov5640_Raw8FullConfig` / `Ov5640_Raw10FullConfig` select 2592x1944. Full RAW8 and
RAW10 passed minute-long captures without errors; RAW10 was also checked on a
live scene with 10-bit precision preserved. Full RAW10 uses DMA stride 3248 and
3240 active bytes per line, requiring the full-width diagnostic writer.

RGB565/555/444, YUV444/420, and JPEG passed minute-long test-pattern and live
captures over GPIO transport. JPEG uses `Ov5640_SetJpegFixedFrame` only after
Configure JPEG and before Stream, specifying canvas bytes/lines and padding.
Mode2 was verified with a 1280x480 canvas for JPEG640x480 and zero padding.
The received files decode successfully at approximately 15 fps. All four native
YUV422 byte orders passed separate color checks.

Direct RGB888 is not yet qualified: the red channel repeats blue in the bars.
At this stage the new formats are available through driver/FPGA diagnostics;
GenICam publication and GigE delivery require separate integration/testing.


GigE passed the full 23-case size/six-legacy-format matrix, including
2592x1944 RGB8/BGR8/RGB565, without bad buffers or CSI errors. The binned-to-full
array transition was fixed: repeated Configure puts the sensor core into
standby, Stream wakes it, and dimensions/HTS/VTS are read back. The FPGA
preserves CSI/D-PHY reset sequencing and separately resets both sides of VFB.
`bash tools/run_gige_board.sh` now loads the matching GPIO-SCCB pair; `--legacy`
keeps the previous AXI-IIC variant available. Exact build and logs:
`diagnostics/gige-gpio-20261025/README.md`.

October 26: typed AE target, manual/automatic sharpness and denoise, average
luminance, and VTS-based FPS controls were added with verified dynamic bounds.
They are exposed through GenICam and saved in Flash UserSet with the other image
controls and gamma/LENC. Seven user profiles and one protected factory profile
are implemented; FPGA design and camera clock setup were unchanged. The hardware
critical cycle passed, including preset restoration after a complete board
restart. Data and limitations: `diagnostics/usersets-20261026/README.md`.
