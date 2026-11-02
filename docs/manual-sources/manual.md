# Reading this manual

This manual explains how to operate, configure and diagnose the DinxTech Camera engineering platform, implemented by the MaxCamera project. It is intended for camera users, software integrators and firmware or FPGA engineers. Start with connection and acquisition, then use the processing chapters and complete control reference for a specific configuration.

The camera combines an ALINX AXU2CGB development board, an AN5641 module with an OV5640 sensor, FPGA image processing and standalone Cortex-R5 firmware. The DinxTech name is the document and product presentation identity; existing software may still identify the device as MaxCamera. Device identity strings have not been rewritten by this documentation.

This revision consolidates evidence available through 07:01 UTC on 2 November 2026. The accepted adaptive-tone baseline is firmware 0.3.13-adaptive-dev with FPGA v11. Natural-scene photographs were captured on firmware 0.3.12-upscale-dev with FPGA v10. Earlier format, CLAHE, live-control and UserSet tests retain their original version and scope. The corrected firmware 0.3.14-remap-dev / FPGA v12 pair subsequently passed bounded synthetic remap acceptance on 2 November 2026. The initial failed v12 run is retained separately; it does not describe the corrected pair. Remap acceptance is limited to 640x480 nearest sampling and plus/minus seven source rows with 40 us packet spacing, not optical calibration or endurance. Historical records describe a test configuration, not the camera's present runtime state.

## Evidence terms

**Implemented** means that the interface or algorithm exists in source. **Offline verified** means that a software, numerical, RTL or implementation check passed. **Bounded camera acceptance** means that a named hardware configuration passed a finite test. **Optically calibrated** would require suitable scenes, traceable settings and independent criteria; it is not implied by any of the preceding terms.

The platform is an engineering portfolio. No formal GigE Vision or GenICam certification, 24-hour endurance result, camera environmental rating or calibrated image-quality specification is asserted. SDK compatibility and attractive photographs cannot establish those properties.

## Navigation and conventions

The PDF contains a linked contents page and bookmarks. Feature names use exact case because host software may treat them as identifiers. RO means read only, RW means validated read/write, and WO means command or write only. A value shown in engineering units is different from the raw integer in a GVCP word. The reference lists both where the conversion is verified. Source locators refer to files in the project distribution and evidence bundles; they are not private workstation paths.

# Hardware and first connection

## Hardware identity and electrical boundary

The owner-confirmed combination is AXU2CGB plus AN5641. The manufacturer board manual identifies the XCZU2CG-1SFVC784E device, 2 GB of 64-bit PS DDR4 on the B variant, 256 Mbit QSPI and 8 GB eMMC. These are board capacities, not a promise that the application uses each peripheral. MaxCamera operates one camera stream through the implemented network path. It does not provide a USB camera service, DisplayPort viewer, dual-camera acquisition or an A53 inference service merely because the board contains related hardware.

The ALINX module manual identifies OV5640, a 1/4-inch optical format, manual focus and a 15-pin 1.0 mm pitch two-lane MIPI connection. The module includes supply and clock circuitry. The board manual specifies a 12 V DC adapter and a 2 A rating in its 2021 revision. Use the supply supplied or specified for the actual board revision and confirm connector polarity from that board; this manual does not infer a universal adapter rating or measured camera power consumption. The module's 3.3 V connector supply is not a 12 V input.

**Connection precaution.** Remove board power before inserting or removing the camera cable. Support the board on an insulated surface, avoid exposed conductors, protect connectors against static discharge and keep the heatsink/fan area clear. Inspect cable contacts and latch orientation at both ends. The module connector has upper contacts; the manufacturer illustration describes exposed contacts facing upward at the module. Do not assume the board end has the same physical orientation. Confirm connector identity against the matching board documentation, then close both latches without forcing the cable.

## Connection sequence

1. With power removed, connect the AN5641 module to the MIPI interface wired by the installed FPGA design. A second board socket is not an independently supported second camera.
2. Connect Ethernet to the host or an appropriate switch. Use a link that negotiates the intended Gigabit rate and avoid an overloaded shared path during diagnosis.
3. Connect the diagnostic USB-UART and JTAG only if required. A receiver and a programming tool have different ownership responsibilities; do not open a competing console or reset an active session.
4. Apply the board's verified supply. Confirm startup and discovery before starting capture. A programmed RAM-only image may be lost after power cycling; a previous successful volatile session does not establish the contents of the boot image.
5. Discover the camera by its actual MAC, serial and current IP address. The development network used camera 10.0.0.3 and host 10.0.0.7; these are examples, not universally assigned addresses.
6. Open a compatible GenICam host application, select one controlling client and read device version, readiness and processing capability nodes.

The manufacturer mechanical drawing gives a 100.00 x 85.00 mm board outline, 91.11 x 76.12 mm mounting-hole center spacing and 3.175 mm hole diameter. These are board drawing dimensions, not a finished camera enclosure or lens clearance specification. Verify actual revision and mechanical clearances before designing a housing. Camera mass, enclosure rating, lens calibration, cable-length limit and assembled thermal envelope remain unspecified.

## Indicators and thermal controls

On compatible v10/v11/v12 images, logical status bits are heartbeat, sensor ready, GigE ready and streaming. The physical outputs are active low. Heartbeat has a 500 ms half-period; the streaming indication has a 250 ms half-period. GigE ready means link, usable IP and bound control service; it does not mean that a client is connected or that an image is being captured.

Independent video reset on v10/v11/v12 is on a different GPIO channel from the four LEDs. Legacy v9 aliases a LED bit to video reset, so low-level LED tests must respect image identity. A historical generated reference that says the fourth LED is always off applies to older behavior.

DeviceTemperature is the FPGA die reading, not OV5640 temperature. Source defaults are fan start 45 C, stop 35 C and duty 75 percent. The start threshold must exceed the stop threshold. PWM is applied only when thermal hysteresis enables the fan. No tachometer, measured RPM or fan-stall guarantee is implemented. These defaults do not constitute a validated camera operating-temperature range. Sources: [H1], [H2], [D1], [D7].

# Start stop and control ownership

## First image in a GenICam viewer

Read SensorInitialized, SensorBusy, SensorState, SensorLastError and EthernetLinkUp. Wait for a ready, idle sensor; an available feature tree does not establish sensor readiness. Configure a receiver socket and buffers before AcquisitionStart. Use a native VGA YUYV configuration for a simple transport check, or the explicitly defined full-field FPGA preview recipe for processed RGB.

Stop acquisition before changing PixelFormat, dimensions, FPGA routing, geometry, coefficients or LUTs. Wait until SensorBusy is false because Stop can acknowledge while an in-flight frame drains. Release TLParamsLocked through the owning host's normal sequence. Write a complete coherent configuration, read back the values and only then start. Backend guards remain active even if a GUI node appears editable.

After Start, verify complete frames, expected PayloadSize, pixel format, width and height. Compare host reception counters with camera counters over the same time interval. A sensor frame-rate value is not measured delivered FPS. Finish by stopping, waiting for idle and releasing the control lease so another client can connect.

## Ownership and heartbeat

GVCP ownership is tied to the controller IP address and UDP source port. Control mode permits appropriate monitoring reads by others; Exclusive mode blocks their ordinary register/memory access while discovery remains available. Only the owner may perform protected writes and release its lease. A second process using the same computer but another socket is not automatically the same owner.

The source heartbeat default is 3000 ms, with accepted range 500 to 60000 ms. Valid owner GVCP traffic extends the lease. Heartbeat expiry stops acquisition/transmission and clears destination, locks and ownership. Preserve the original error when a later cleanup reports AccessDenied: automatic recovery may have revoked the old lease after the primary capture failure.

A write acknowledgement proves acceptance of that command, not successful later acquisition. Multi-register writes execute in order; earlier writes can remain applied if a later write fails. Do not treat a write batch as an atomic transaction. Read back after a failure and recover from a known configuration.

## Device reset and restoration

DeviceReset is an asynchronous return to the saved power-up configuration. It resets sensor/video and protocol state, loads UserSetDefault and applies saved network/identity. Acquisition and ownership are released, so rediscover and reconnect afterwards. It is not an in-place continuation of a stream and is not synonymous with a new FPGA/firmware load.

Before any diagnostic change, record scalar settings, selected tables, exposure thresholds, stream parameters and image identity. Restore the original runtime and compare it explicitly. Loading a preserved native UserSet can restore derived sensor details that a simple scalar JSON replay cannot reconstruct. Restoration success is separate from the primary test result. Sources: [D1], [D2], [P1].

# Network and transport setup

## Addressing and packet configuration

The implementation exposes one network interface, one unicast stream and no message channel. It supports discovery, temporary ForceIP, persistent addressing, DHCP with link-local fallback, register/memory access, packet resend and the implemented heartbeat behavior. Firmware defaults select DHCP plus link-local fallback; provisioned values may differ.

ForceIP changes the current address temporarily. Persistent address, identity and IP-policy writes can be journaled in nonvolatile storage while stopped and applied at reset/boot. Do not use persistent writes merely to establish a temporary test connection. Confirm DeviceConfigurationPending and DeviceConfigurationLastError if intentionally changing saved network configuration.

GevSCPSPacketSize denotes the complete IPv4 packet size, not just pixel data or the Ethernet frame. The implementation accepts 576 to 1500 bytes; it does not expose a jumbo-frame range. At 1500 bytes, an ordinary GVSP data packet carries 1464 image bytes after IPv4, UDP and GVSP headers. Do not increase the host MTU expecting this camera to emit larger supported packets.

GevSCPD uses timestamp-clock ticks. The advertised clock is 1 MHz, so one tick is one microsecond in the validated profile. The RAW10 hardware source default is zero delay; the historical legacy default is 50 ticks. The v11 acceptance used 20 microseconds to prevent host socket loss. This is a measured test profile, not a blanket requirement that every geometry use the same spacing.

## Receiver tuning procedure

1. Start with a directly connected or otherwise quiet network and a supported packet size. Confirm destination IP and UDP port before Start.
2. Read PayloadSize and allocate enough buffers for the selected format. RGB8 full resolution is much larger than VGA YUYV.
3. Capture a short finite window and record missing, bad, duplicate, malformed and reordered packets, completed frames and camera CSI/drop/recovery deltas.
4. If packets are lost while camera CSI and capture counters remain healthy, inspect host UDP receive-buffer errors and receiver scheduling. Increase pacing while stopped and repeat the same geometry.
5. If CSI or DMA completion fails before any GVSP image packets, investigate the capture/processing path. Host socket tuning cannot repair an upstream line-buffer overflow.

The v11 unpaced 1280x960 experiments lost packets; one run recorded 1278 missing packets and exactly 1278 host UDP RcvbufErrors. The actual socket buffer was 425984 bytes. The subsequent 20 us profile passed 500 frames across eight cases without those errors. The tests did not modify host system receive limits. CPU load, NIC driver, socket size and application behavior are part of a throughput measurement.

## Resend timestamps and payload lifetime

GVSP uses classic 16-bit block IDs and 24-bit packet IDs in the implemented profile. Resend operates on retained immutable payloads with a bounded queue; it cannot recover a frame whose data is no longer retained. Monitor PacketResendRequests, RetransmittedPackets, PacketResendUnavailable and PacketResendQueueOverflows. Distinguish recovered reordering from missing or malformed payloads.

The device timestamp clock can be latched/reset through control registers. Frame timestamps are captured for an armed frame; resetting the device clock must not retroactively change an already armed image or the monotonic clock used for deadlines. A GVSP timestamp is not a verified sensor-effective exposure tag. No PTP synchronization or hardware-trigger synchronization is qualified by this revision. Sources: [P1], [E4].

# Pixel formats geometry and frame rate

## Native acquisition and FPGA processing

With FPGA processing disabled, sensor-native routes supply Y8, Bayer, YUV and supported packed color formats. Native RGB8/BGR8 use conversion from sensor YUYV. With FPGA color processing enabled, RAW10 feeds the demosaic and color pipeline and can be packed as RGB/BGR or the supported FPGA YUV formats. The same output format name therefore does not identify every upstream stage.

The maximum public canvas is 2592x1944, 5038848 pixels. Public output minima are 160x120. Height is even; width increment depends on packing. RAW10 requires a multiple of 16, legacy YUV420 a multiple of 8, YUV444 a multiple of 4 and other formats a multiple of 2 in the documented native profile. Read WidthMax, HeightMax and WidthIncrement for the active device. YUV444 and JPEG are restricted to VGA in the exposed format profile.

Native RAW uses an unbinned crop with fixed BGGR phase; ReverseX/Y are locked. Native sensor YUV can have a different orientation and ISP scaling. Do not compare an arbitrarily oriented native image against a FPGA capture as a one-variable before/after pair. The gallery makes this distinction explicit.

## RAW packing

BayerBG10 stores each 10-bit value in the low bits of a little-endian 16-bit word. BayerBG10p is a continuous little-endian packed 10-bit stream. CSI RAW10 wire groups are different: four high bytes plus a combined low-bit byte must be repacked. Passing CSI bytes to a PFNC10p decoder produces incorrect samples. DMA stride padding is not transmitted in the GVSP image payload.

At VGA, BayerBG10p is 384000 bytes and BayerBG10 is 614400 bytes. At full resolution these are 6298560 and 10077696 bytes. RGB8 is 921600 bytes at VGA and 15116544 bytes at full resolution. Payload calculations exclude protocol overhead. Consult the format table for custom codes, byte order and decoder limitations. Sources: [F1], [P1].

## Input ROI and output dimensions

FpgaInputWidth/Height specify capture input geometry when the FPGA scaler is enabled. FpgaRoiEnable and the ROI offset/size nodes select a region after demosaic. Width/Height specify delivered output. These are three separate coordinate systems. The ROI must fit inside input; requested output and all alignments must be valid.

Disabling the scaler does not necessarily preserve full field of view: output dimensions can become the RAW capture dimensions. A 640x480 image with scaler disabled can be a narrow crop, whereas 2592x1944 input scaled to 640x480 covers the full field. Disabling the master FPGA path changes to the sensor's own ISP and therefore also changes color and tone behavior.

Area interpolation supports reduction only. Polyphase uses a six-tap, 64-phase AMD VPSS. Reductions larger than 4:1 first use an Area prefilter; mixed up/down axes are supported by the planner and offline tests. Equal geometry bypasses VPSS and an unchanged axis uses identity coefficients. Enlarging pixels does not recover optical detail.

The public software limit is 4x enlargement per ROI axis within the maximum output canvas. Hardware evidence is narrower: centered 640x480 ROI to 1280x960 RGB8 passed; centered 320x240 ROI to 1280x960 YUYV passed 57 frames at 14.293775 FPS in the November 2 diagnostic. The equivalent RGB8 4x case produced no completed frames and raised CSI stream-line-buffer-full. Use only the qualified geometry/format combinations for dependable operation; an availability flag is not proof of every legal combination.

## Exposure and cadence

ExposureTime is expressed in microseconds but programmed in sensor lines. The XML conversion uses the active SensorLinePeriod: exposure_us = lines * line_period_ns / 1000. The minimum is one line and the maximum follows ExposureMaximumLines. A mode change can change line period and exposure limits. Read them after configuring geometry and format, then set ExposureAuto=Off before a manual exposure write.

GainFactor is linear gain with raw units of 1/16; raw 16 is 1x and the exposed register ceiling 1023 is 63.9375x. GainAuto must be Off for manual gain. This register range is not a calibrated low-noise operating range. Sensor white-balance gains use raw unity 1024 and are separate from FPGA Q8.8 gains.

AcquisitionFrameRate changes vertical blanking, not the sensor clock or line cadence. ResultingFrameRate is the nominal sensor rate from timing, not host-delivered FPS. A 1 FPS request did not resolve the 4x RGB overflow because instantaneous row/burst pressure remained. Query dynamic frame-rate limits rather than applying an advertised sensor maximum to every processing and transport route. Sources: [D3], [F1], [E5].

# FPGA image processing

## Processing order

The color path is RAW10 input, black subtraction, Bayer bad-pixel correction, Bayer lens shading, demosaic, manual gains or FPGA AWB, color matrix, RGB8 conversion/gamma, median, bilateral, sharpening, ROI/Area, Polyphase, CLAHE, adaptive tone, scene metrics and output packing. The corrected v12 image inserts remap after Polyphase and before CLAHE. Native sensor bypass and RAW-preserving output follow other routes. Enable and availability must both be checked for each stage.

## Black level and bad-pixel correction

FpgaBlackLevel subtracts 0 to 1023 DN from RAW10 and clips at zero before BPC and gains. This removes an offset; it is not a denoiser. Excessive subtraction destroys dark detail. Source default is zero. An exposure/gain/temperature-dependent dark calibration has not been established.

FpgaBadPixelCorrectionEnable invokes local Bayer-domain correction before demosaic. It uses neighboring CFA samples, not a stored defect-address list. Numerical and deterministic hardware tests do not measure hot/dead-pixel detection accuracy over temperature. The gallery does not contain a controlled defect example.

The special live Black Level route is permitted only during healthy, requested, active RAW10-based processing on an audited image. FpgaBlackLevelLiveActive reports eligibility. A live write is latched for a subsequent complete HLS invocation without restarting acquisition. Readback is the requested value, not a promise that the next delivered frame has applied it. Stop/drain, recovery, bypass and UserSet activity block the exception. Geometry, tables and other controls remain stopped-only.

## Lens shading and demosaic

Lens shading applies four independent BGGR-phase 7x9 grids after black/BPC. Gains use Q12, unity 4096, range 0 to 4x, with bilinear interpolation, rounding and RAW10 saturation. There are 252 gains, represented by 504 bytes. The two green phases have separate tables. The default is disabled with a unity grid. This changes brightness/color falloff; it does not straighten geometric distortion.

The color path demosaics BGGR into 10-bit RGB. Manual R/G/B gains are Q8.8 with unity 256 and exposed range 0 to 16x. The RAW-preserving route applies Bayer-phase gains without creating RGB. Very large gains clip, so the legal register range should not be mistaken for a quality recommendation.

## Manual and automatic white balance

FpgaBalanceWhiteAuto=Continuous uses admitted preceding-frame color statistics, holds green at unity, bounds red/blue automatic gains to 0.25..4x and moves a quarter step toward the target. Samples with a channel below 8 or at least 1000 DN are excluded from admission. Dark or saturated scenes can hold the previous gains. Restart or entry into automatic operation resets adaptation.

Gray-world balance assumes a sufficiently neutral aggregate scene and can fail on strongly colored subjects. A short restart sequence is not proof of convergence. Sensor BalanceWhiteAuto is different: it is valid on supported sensor ISP routes, while the current RAW route uses sensor AWB Off. Read FpgaAppliedBalanceRed/Green/Blue from a latched complete-frame snapshot to inspect actual FPGA gains instead of assuming the manual base values were applied.

## Color matrix and gamma

FpgaColorTransformationEnable controls a 3x3 matrix plus three offsets before RGB8 conversion. Coefficients/offsets are signed Q20 in [-8, 8), with raw limits -8388608 to 8388607. Matrix selection is row-major RGB output rows then RGB input columns; offsets use R/G/B channel order. Vendor Q8 product intermediates and truncation affect precision. Configure every coefficient and offset while stopped before enabling the transform. An identity matrix is neutral; it does not supply color-target calibration.

FpgaGammaEnable uses three 256-entry LUTs. FpgaGammaChannel, FpgaGammaIndex and FpgaGammaValue select the 768 bytes. Default LUTs are identity. The demonstrated display curve is round(255 * (x/255)^(1/1.8)), repeated for all three channels. Gamma changes display brightness after quantization; it cannot recover clipped highlights or increase sensor dynamic range.

## Median bilateral and sharpening

Median is a 3x3 per-channel filter after gamma with replicated borders. It can suppress isolated impulses and remove fine texture. Bilateral follows median with a 3x3 range/spatial filter and replicated borders. SigmaColor is 1..255 DN, default 24; SigmaSpace is 0.25..4 pixels, default 1, represented in Q8. Large values increase smoothing and can hide texture. Compare at consistent display scale and exposure.

SharpenStrength is 0..1 in Q8. The cross-Laplacian kernel has center 1+4s, four axial neighbors -s and zero corners, followed by RGB8 saturation. Zero bypasses. Constant-zero borders can brighten image edges. Halos and noise amplification are expected tradeoffs; sharpening is not new spatial resolution. The gallery includes 0.25 and 0.5 examples.

## Local contrast

CLAHE processes max(R,G,B) using 4x4 tiles after scaling, then rescales RGB using the mapped value ratio with fixed-point rounding. Black remains black. Clip multiplier is 1..8, default 4. Output dimensions must meet the public format limits as well as CLAHE's internal minimum 32, width multiple 8 and height multiple 4.

Previous-frame histograms drive the next output. The first frame after Start, geometry/clip change or reset bypasses correction while history is built. High clip limits can amplify noise and local texture and react to lighting changes. Natural-scene examples illustrate these effects; they do not establish universally better image quality.

## Adaptive display tone

The v11 stage follows CLAHE. It measures integer input brightness (R+2G+B)>>2 and uses the rounded previous-frame mean. Desired Q8 gain is rounded target*256/mean, clamped to 128..512, then changes at most 16 raw units per frame. Target is 16..224 DN, default 128; gain is 0.5..2x. Zero mean holds gain. The shared channel curve is round(255*g*x / (255+(g-1)*x)), with g in engineering units. Black and white endpoints remain fixed.

The first/start/reset/target/dimension-change frame uses unity. Enable and target are stopped-only. Gain and preceding mean are unlatched invocation status, not an atomic pair or a tagged GVSP frame measurement. A common nonlinear curve can alter RGB ratios. This is RGB8 display adaptation, not HDR radiance reconstruction. The 500-frame eight-case v11 synthetic acceptance verifies history, pixels and transport; no matching natural-scene optical-quality result is available.

## Scene measurements and YUV output

Execute FpgaStatisticsLatch before reading the common complete-frame snapshot. Sequence resets at acquisition reset and is not the GVSP FrameID. Valid-pixel count, admitted color means, applied gains, intensity, clipping, darkness, focus and motion have distinct sampling locations. Color means precede gains/CCM; final intensity is output RGB8 luma. Adaptive-tone mean/gain do not participate in this latch.

Focus is the mean absolute four-neighbor luma Laplacian over the output interior. Motion is the mean absolute difference between current and previous 32x32 luma-bin means. Motion is invalid on the first frame after restart or without valid history. Noise can raise focus and illumination changes can look like motion. No autofocus, object detection, calibrated threshold alarm or distance measurement follows from these statistics.

FPGA RGB-to-YUV uses BT.601 with explicit Limited or Full range. 422 averages chroma over pairs and legacy 420 uses 2x2 chroma. RGB/BGR channel order remains selected by PixelFormat. Native YUV bypass retains its own processing and level interpretation. Wrong byte order or range in a host decoder can imitate a sensor color or contrast problem. Sources: [D3], [D4], [E1], [E3], [E4].

# Recipes and reproducible configuration

## Conservative full-field RGB preview

The historical verified UserSet7 recipe is 2592x1944 RAW input to 640x480 RGB8, ROI disabled, Polyphase enabled, black 0, BPC on, manual base gains 1/1/1, FPGA AWB Continuous and a fully populated display gamma 1.8 LUT. Scene analysis is on; CCM, lens shading, median, bilateral, sharpening and CLAHE are off. Sensor AE/AGC are Continuous and sensor AWB is Off. Adaptive tone, when present, is explicitly off with target 128.

Stop and wait for idle. On the unit with the preserved verified slot, select UserSet7 and execute UserSetLoad. Wait for UserSetBusy=false and UserSetLastError=0, then read back geometry and tables before Start. Slot names are unit-specific; an arbitrary camera's UserSet7 need not contain this profile. Do not change UserSetDefault merely to test the preview.

To configure without a saved slot, use tools/configure_fpga_scaler.py's documented apply_working implementation in a properly owned session. It disables dependent stages first, stages output, clears patterns, writes all matrix/offset/gamma/LSC entries, sets the complete known scalar state and enables the master last. It does not save/load UserSets by itself. Neutral gains with an identity LUT do not reproduce the display-gamma recipe. The saved historical test delivered 86 frames at 14.297787 FPS over six seconds with no reported missing/bad packets, drops or CSI errors.

## Controlled feature comparison

Choose a stationary scene and fix lens, illumination, camera position, exposure, gain and sensor WB. Configure the linear baseline explicitly. Change one FPGA feature at a time while stopped, restart, record settling/history and retain both raw frames and settings. Keep automatic modes off unless adaptation itself is the subject. A changed exposure or gain can overwhelm the feature being demonstrated.

For the supplied comparison baseline, RGB8 output is 640x480 from full input 2592x1944 with Polyphase, no ROI, black 0, gains 1/1/1, scene analysis on and optional corrections off. Manual exposure is 1686 lines at 35.55 us/line, approximately 59937.3 us by the recorded configuration; raw gain 240 is 15x. These are recorded settings, not immutable per-frame exposure metadata. Each variant changes the parameters printed in its plate.

## Twofold digital ROI zoom

Configure full input 2592x1944, ROI x=976, y=732, width=640 and height=480, output 1280x960, scaler on, Polyphase and compatible RGB8 processing. Ensure the entire ROI fits and configure while stopped. This expands a centered subregion; it narrows field of view. Do not use the native YUV reference as a pixel-aligned before image. For 4x, the 320x240 centered ROI and 1280x960 output have only a bounded YUYV diagnostic pass; RGB remains an open failure.

## JPEG capture

Stop, disable compression if necessary, select 640x480 YUV422_8 (legacy alias YUV422_YUYVPacked), then set ImageCompressionMode=JPEG. Geometry/PixelFormat are locked while compressed mode is active. SensorJpegQuality is 0..63; smaller values mean higher quality and typically larger encoded frames.

PayloadSize is the maximum allocation, 614400 bytes, while each JPEG leader carries actual encoded length. Firmware validates marker/entropy syntax and strips bytes after EOI; malformed, truncated or overflowing frames are dropped. The host must understand GVSP JPEG payload type 6. Aravis 0.8 can configure this mode but does not implement its JPEG GVSP receiver; tools/gige_jpeg_check.py provides an independent receiver in the project. Disable compression before returning to another geometry. Sources: [R1], [F1], [E2].

# UserSets and calibration data

## Runtime load persistent save and startup choice

UserSetSelector selects Default or UserSet1..7. UserSetLoad applies an existing profile to runtime. UserSetSave writes a non-default slot. UserSetDefault chooses the profile used at startup/reset. These are different actions with different persistence effects. Default is read only through ordinary UserSetSave; factory provisioning uses a separate protected command. A preview recipe should not require factory unlock.

Check UserSetStorageAvailable, UserSetValid, UserSetSavedMask, UserSetBusy and UserSetLastError. Preserve the previous record before an intentional save, keep a known rollback slot and verify a load from a deliberately different runtime. Do not infer Flash integrity from an encoder unit test. The v10 UserSet7 save/load was exercised; a complete new v11 hardware persistence campaign was not performed by its adaptive-tone acceptance.

Current ISPA records store 30 FPGA scalars in 1772 bytes, including earlier matrix, offsets, gamma and LSC data. Genuine ISP9 28-field 1764-byte records decode with adaptive tone off/target128. Older schema defaults and native-YUV bypass behavior are retained. Schema identity and processing version are different concepts. Unsupported tone-enabled profiles are rejected on older hardware. Statistics, transport destination/packet parameters and volatile distortion maps are not saved image settings.

## Why scalar replay can fail

AutoExposureTarget is derived from several sensor thresholds. The historical six bytes [48,40,96,48,38,20] read as target 44, but writing 44 does not reconstruct those bytes. The verified restoration procedure used an unchanged native UserSet anchor and compared the raw thresholds. Treat a scalar JSON export as a record, not a universal exact-restoration mechanism. Do not bypass protected register ranges to force a replay.

## Flat-field lens-shading workflow

The offline fitter expects little-endian uint16 BGGR values in 0..1023, exactly width*height*2 bytes without headers or padding. Dimensions are even and at least 32. PFNC10p input must first be correctly decoded. Use matching lens/focus, geometry, phase and black level, fixed exposure/gain/WB, uniform illumination and disabled sensor/FPGA LSC.

Fit and validation inputs must be distinct in content; renamed duplicates are rejected. Keep a held-out capture that does not participate in fitting. Evaluate phase means, coefficient of variation, percentile spread and saturated fraction. Clipping can make a field appear more uniform, so a reduced spread alone is not success. The tool models BLC plus the implemented interpolation/rounding/saturation; it does not model BPC or every downstream ISP stage.

The 504-byte output and adjacent report preserve hashes and supplied conditions. Offline prediction is not camera programming or independent optical qualification. A suitable real flat-field pair and agreed criteria are still required. After a future controlled hardware application, compare a separately captured corrected field and retain original captures, table and exact image identity. Sources: [R1], [D5], [E4].

# Distortion correction and HDR boundaries

## Bounded distortion correction

The v12 design adds an inverse-map RGB8 reader after VPSS and before CLAHE. Its accepted design profile is nearest-neighbor sampling on an unchanged canvas no larger than 640x480, with source row displacement at most plus/minus seven. Other camera modes retain up to 2592x1944 bypass. An identity map or synthetic shift can test transport/pixels but does not calibrate a lens.

Map words are little-endian uint32: valid bit 31, source x bits 9:0, source y bits 18:10, reserved bits 30:19 zero. A zero word represents a black sample. Valid coordinates must be in bounds and within the row window; unsupported words are rejected. CRC32 uses reflected polynomial 0xEDB88320 with conventional initial/final XOR. The map compiler preserves geometry/calibration IDs and source hashes.

The volatile interface stages width, height and expected CRC, begins an inactive-bank upload, accepts sequential Index/Data pairs, then commits incremental validation. Ready is required before Enable. Identical earlier-word retries are allowed. Uploading/Validating prevents acquisition; structural reconfiguration, preset load and sensor reset invalidate the map. The two dedicated 64-byte-aligned DDR banks total 2457600 bytes. Maps and enable state do not persist in UserSets or Flash.

The first v12 board run passed bypass equivalence and invalid-map protection but enabled identity capture failed. The reproduced VPSS TSTRB contract mismatch was corrected at the output boundary, preserving KEEP, SOF and EOL checks. The corrected pair then passed exact identity, restart and translations of (+5,+7) and (-5,-7) source coordinates, including black borders. Each map run delivered 57 complete frames at approximately 14.294 FPS with 40 us packet spacing and no new CSI/drop/recovery or packet loss. The original failed candidate remains preserved. This qualifies the bounded functional remap path, not calibrated lens correction, endurance or other geometries. [E7]

## Map preparation example

The following commands describe offline preparation only. Replace symbolic arguments with a genuine map for the intended geometry. The uploader without its explicit upload switch validates locally and does not contact the camera.

```sh
python3 experiments/remap_integer/compile_map.py FLOAT_MAP_DIRECTORY COMPILED_DIRECTORY
python3 tools/upload_distortion_map.py COMPILED_DIRECTORY
```

Actual camera upload requires a matching qualified image, stopped/drained state and control ownership. For the accepted pair, the ordinary upload sequence is: stop/drain and release transport locks; set matching dimensions and expected CRC; Begin; send sequential Index/Data; Commit; wait for Ready; enable; start a bounded capture. The supported explicit client command is `python3 tools/upload_distortion_map.py COMPILED_DIRECTORY --upload --ip CAMERA_IP --bind HOST_IP --enable`. It does not change dimensions, save presets or acquire images by itself. The map's calibration must match the physical lens, ROI and scaler path; a firmware generation token cannot prove those optical facts. A reader cancellation or Stop acknowledgement is not proof that DDR accesses have quiesced. A stuck reader can require system recovery beyond a video-only reset.

## HDR and inference

The repository contains offline HDR session, lease and CPU merge components. Sensor-effective exposure association, acquisition integration and frame ownership remain unproven for a deployable camera HDR mode. A cached exposure value is not an immutable capture tag. The camera does not gain true HDR because gamma, CLAHE or adaptive tone makes shadows more visible.

No AI model, hardware-trigger service, dataset acquisition campaign or custom training is deployed by this release. Available CPU cores and unused DDR capacity do not establish a usable inference path. End-to-end memory ownership, operator support, timing and accuracy would each need separate evidence. Sources: [D6], [E6], [E7].

# Diagnostics troubleshooting and recovery

## Record the first failure

Read sensor readiness/busy/state, last sensor/transport error and failing sensor register. Record CapturedFrames, TransmittedFrames, DroppedFrames, CsiErrorEvents, CameraRecoveryCount and host receiver counters before and after the same interval. Include dimensions, format, input/ROI/output, processing version/capabilities, enabled stages, exposure/gain settings, packet size/delay and software image hashes.

Keep the first UART or receiver error and the later recovery outcome separately. CSI error-bearing poll counts are not damaged-frame counts. The 4x RGB first retained mask 0x00040000 is stream-line-buffer-full; 0x00020000 in the associated interrupt word is detect-stop-state rather than another error. The observed 174504 us from DMA arm is a software polling timestamp, not cycle-accurate overflow or exposure timing.

## Common symptoms

**No discovery.** Verify power, physical link, host interface and actual address policy. Use discovery/temporary ForceIP according to ownership rules. Check GigEVisionReady and EthernetLinkUp through available diagnostics. A saved static address can differ from the development subnet.

**Access denied or controls locked.** Confirm one controller and the owning socket, Stop and wait for SensorBusy=false, then unlock transport parameters through the host. Check UserSetBusy and automatic exposure/gain conditions. Following automatic recovery, the old lease can be invalid; inspect ownership before reacquiring it. Do not force takeover of another active client.

**Black or very dark complete images.** Check test pattern, black offset, manual/applied gains, gamma table contents, exposure and route. Black1023 deliberately drives RAW10 subtraction to zero. A stale identity LUT with gamma enabled is still identity. A complete dark frame and no frame arriving are different failures.

**Blue or unnatural color.** Confirm native versus FPGA path, RGB/BGR order, YUV byte order/range, sensor AWB availability, FPGA AWB state and non-unity dormant gains or matrix. Use a neutral configuration with the correct display LUT before attempting calibration.

**Wrong field of view or mirrored picture.** Inspect full input, ROI, scaler and RAW orientation. A disabled scaler can select a crop; native sensor orientation differs from the FPGA gallery. Do not apply an undocumented host mirror and then claim equivalent source geometry.

**Missing packets with healthy capture.** Inspect host UDP errors, socket buffer and CPU scheduling, then adjust pacing while stopped. Resend only helps while the source payload remains retained. Do not declare packet integrity from FPS alone.

**No DMA completion and rising CSI errors.** Retain the hardware fault before recovery clears it. The reproduced 4x RGB path is an example; lower average FPS alone did not repair it. Return to a bounded known configuration and keep the failed mode outside acceptance.

**CLAHE or tone differs on first frames.** This can be intended history initialization. Retain frame sequence and the number of settling frames. Do not compare first-frame bypass with steady-state output as though both had identical temporal state.

**Statistics stale or inconsistent.** Use the common latch and sequence for supported metrics. Sensor telemetry is cached and can be deferred above 105 MB/s; stopped background sensor reads are disabled. Adaptive-tone status is separately unlatched. Neither is a universal per-frame metadata stream.

## Recovery ladder

First stop through the current owner and wait for drain. Preserve failure logs and verify no other owner before opening a fresh control session. Restore a complete known runtime or preserved native preset, then capture a short bounded control case. If sensor/video reset is required, understand that DeviceReset loads the startup profile and releases ownership. If a PL/DDR reader is stuck, do not assume a video-only reset frees its memory lease.

A full JTAG reload is disruptive and requires the compatible sealed bitstream, ELF, XSA and platform startup dependencies. Inspect uncertain outcomes before retrying. Never erase a failed result just because a later baseline frame succeeds. No diagnostic example in this manual requires saving a UserSet or changing Flash boot firmware. Sources: [P1], [D1], [E3], [E4], [E5], [E6].

# Software build and integration reference

## Build environment and dependencies

The verified local environment uses AMD Vivado, Vitis HLS and Vitis 2024.2, the R5 toolchain/BSP, native C/C++ compilers and Python 3. Vitis Vision headers are pinned by tools/fetch_camera_vision.sh. Aravis/eBUS-dependent checks need their installed runtimes and applicable licenses.

The application directory alone is not a proven clean-checkout release. Builds reference sibling drivers and platform/BSP trees, exported IP and generated hardware artifacts. Preserve dependency versions and third-party notices; license review and a clean-machine reproduction remain publication gates. The application is standalone C/C++ on Cortex-R5 #0 with lwIP, not an A53 Linux camera/inference distribution.

## Offline commands

Run commands from the application root in the configured environment. These checks do not acquire the camera. Regenerating XML changes generated project files and should be coordinated with the owner of that source tree.

```sh
bash tests/gige/run.sh
bash tests/polyphase/run_native.sh
bash tests/adaptive_integration/run_native.sh
python3 tools/check_adaptive_tone_abi.py
python3 tests/test_distortion_maps.py
bash tests/remap/run.sh
python3 tools/check_project_language.py
```

The FPGA build entry points include tools/build_adaptive_tone.tcl and tools/build_fpga_color_v11.tcl; the v12 candidate has its own integration script. Run the corresponding 2024.2 executable from a configured tool environment. Full routing, resource, setup/hold, pulse-width, unconstrained-path, bus-skew and DRC checks are required for a combined design. An isolated HLS estimate is not final FPGA utilization.

## Volatile load contract

The existing tools/run_board.tcl loader always issues a system reset, programs PL, runs the sibling platform FSBL on A53 #0 and resets/loads R5 #0. Its cable selector and FSBL path are workspace-specific dependencies. The bitstream, ELF and XSA alone are not a standalone update package. Use a verified matching set, preserve rollback and obtain an exclusive maintenance session. A RAM load interrupts streaming and does not update Flash boot firmware.

Record SHA256 of bitstream, ELF, XSA and source snapshot. A version string alone is not a sufficient artifact identity. Do not mix a v11 bitstream with firmware that cannot recognize its signature or touch optional banks on older hardware. Driver and memory ownership boundaries in the following appendix are part of integration correctness, not optional optimization. Sources: [D2], [D7], [E4], [E6].
