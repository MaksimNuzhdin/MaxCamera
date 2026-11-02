# Product definition and evidence scope

The DinxTech Camera is the MaxCamera engineering platform built around an ALINX AXU2CGB and an ALINX AN5641 OV5640 module. It combines configurable sensor capture, programmable FPGA image processing, a standalone Cortex-R5 control application and one Gigabit Ethernet image stream. This datasheet defines the implemented interface and the configurations supported by available evidence. It is intended for technical evaluation and system integration.

Revision 1.1 is dated 2 November 2026, with an evidence cut-off of 07:01 UTC. Accepted adaptive-tone evidence uses firmware 0.3.13-adaptive-dev and FPGA v11. The image examples use 0.3.12-upscale-dev/v10; older native-format, CLAHE, live-black and UserSet results identify their own image pair. The corrected 0.3.14-remap-dev/v12 pair subsequently passed its own bounded camera acceptance: 640x480 nearest sampling, plus/minus seven source rows, synthetic maps and 40 us packet spacing. The initial failed v12 pair remains a separate historical record. The corrected result does not qualify optical calibration or endurance.

**Reading a specification.** Implemented limits describe code and protocol guards. Measured values describe a finite test with stated geometry, format and pacing. Vendor component facts describe hardware, not delivered camera performance. Unverified fields are identified explicitly rather than populated with sensor marketing maxima or inferred design guarantees.

There is no formal GigE Vision/GenICam certification, 24-hour endurance qualification, camera safety or environmental certification, calibrated color accuracy, absolute latency guarantee or integrated HDR qualification in this revision. The live state of a laboratory camera can differ from the historical accepted baseline.

# Hardware and interfaces

## Component facts and implemented use

The manufacturer AXU2CGA/B manual identifies XCZU2CG-1SFVC784E, dual Cortex-A53 and dual Cortex-R5 processing resources. AXU2CGB has 2 GB 64-bit PS DDR4, 256 Mbit QSPI and 8 GB eMMC. The project uses standalone R5 #0 firmware plus PL; it does not deploy an A53 Linux or inference worker. Board USB, display, PCIe and additional camera sockets do not become application outputs by inclusion in a development board.

The AN5641 manual identifies an OV5640, 1/4-inch optical format, manual focus, two-lane MIPI and a 15-pin 1.0 mm pitch upper-contact FPC. Module power/clock circuitry is included. Pin 15 is 3.3 V. The board manual's power section specifies 12 V DC and a 2 A adapter in its 2021 revision; select the supply appropriate to the actual revision and supplied hardware. Adapter rating is not measured camera consumption.

The board drawing specifies 100.00 x 85.00 mm outline, mounting centers 91.11 x 76.12 mm and 3.175 mm mounting holes. A final enclosure, camera assembly dimensions/mass, optical axis datum, lens distortion model, ingress protection and mechanical tolerances have not been established. The module manual's component temperature guidance does not qualify the assembled camera over that range. Sources: [H1], [H2].

## Application interface boundary

The implemented external control/data interface is IPv4/UDP with GVCP discovery and control and GVSP image transport. One interface, one unicast stream and Continuous acquisition are exposed. There is no qualified external hardware trigger, multi-camera synchronization, PTP service, USB Video Class output, HDMI/DisplayPort live-view application or simultaneous multi-resolution output in this release.

The control application owns the sensor, capture lifecycle, network service and bounded telemetry work. Sensor SCCB uses the selected matching transport; the current GPIO path must not be treated as an AXI-IIC peripheral merely because a historical address matches. The source unit bus rate is 50 kHz. The active sensor configuration assumes 12 MHz XCLK and exposes derived timing; that assumption is supported by measured cadence, not a general external clock specification.

DeviceTemperature reports FPGA die temperature. Fan thresholds are hysteretic with start above stop; source defaults are 45 C/35 C and 75 percent duty. RPM is not measured. Status LEDs distinguish main loop, sensor ready, GigE ready and the v10/v11/v12 independent streaming indicator. The implementation exposes diagnostics rather than a certified thermal-control system. Sources: [D1], [D7].

# Imaging and format specification

## Geometry envelope

The public image canvas is 160x120 through 2592x1944, subject to format-dependent dynamic limits and alignments. Height is even. The documented native-profile width increments are 16 for RAW10, 8 for legacy YUV420, 4 for YUV444 and 2 for other formats. YUV444 and JPEG remain VGA-limited in the exposed profile. Read WidthMax, HeightMax and WidthIncrement after mode changes.

Native RAW output uses a fixed BGGR phase and unbinned crop; ReverseX/Y are locked. Sensor-ISP YUV has its own field-of-view and orientation behavior. With FPGA scaling enabled, input size, post-demosaic ROI and delivered output are independent validated geometries. ROI must fit input. With scaling disabled, output size can determine a narrower RAW acquisition crop.

Area performs reduction only. Polyphase uses six taps and 64 phases with Area prefiltering for large reductions; mixed axes have offline coverage. The public maximum enlargement is 4x per ROI axis, bounded by output canvas and alignment. Legal software geometry is broader than accepted hardware combinations: 2x RGB8 and one 4x YUYV case pass; the same 4x RGB8 test fails at CSI line-buffer-full. Sources: [F1], [D3], [E5].

## Exposure and frame timing

ExposureTime is in microseconds with one-line quantization and dynamic maximum. The raw register stores lines; the XML conversion uses the active line period. Set ExposureAuto=Off for manual writes. GainFactor uses raw/16, range 1 to 63.9375x; set GainAuto=Off for manual writes. This is an interface range rather than a calibrated imaging-gain specification.

AcquisitionFrameRate changes vertical blanking while keeping clocks and line cadence unchanged. ResultingFrameRate describes nominal sensor cadence. Delivered FPS depends on processing, payload, packet interval and the host receiver. Mode changes affect system clock/line period and integration bounds; the RAW10 route uses its corresponding 40 MHz sensor system-clock configuration in the documented native integration.

The 4x diagnostic used full RAW10 input 2592x1944, 35550 ns row cadence, fixed exposure 100 lines, raw gain 16 and test bars. Output 1280x960 was identical in raster dimensions for RGB8 and YUYV. Lowering average FPS by VTS had previously failed to resolve the RGB fault. No universal exposure-to-first-byte or trigger latency is specified. Sources: [F1], [E5].

## Output interpretation

Standard PFNC identifiers and explicitly custom layouts coexist. Five custom layouts carry bit 31 and require an appropriate host decoder. BayerBG10 is little-endian 16-bit storage with low ten bits valid. BayerBG10p is a continuous packed little-endian stream, different from native CSI RAW10 groups. DMA padding is stripped from transmitted payload.

FPGA YUV output uses BT.601 with Limited or Full range, independent of selected byte order. Native sensor YUV bypass retains its own level interpretation. JPEG uses ImageCompressionMode, not a fabricated pixel-format code; the selected raster is VGA YUYV, while each GVSP JPEG leader conveys the actual compressed size. The fixed maximum receive allocation is 614400 bytes. Firmware rejects malformed/truncated/overflowed encodings. Sources: [F1], [P1].

# Processing implementation

## Dataflow and numerical scope

The FPGA color path applies RAW black subtraction, BPC, four-phase lens shading, BGGR demosaic, manual/automatic gains, CCM, gamma, median, bilateral, sharpening, ROI/Area, VPSS Polyphase, CLAHE, adaptive tone, statistics and output packing. The corrected v12 remap stage sits between VPSS and CLAHE. Not every format traverses this entire sequence; native sensor and RAW-preserving bypasses are distinct.

Black subtraction is unsigned RAW10 clipping after a 0..1023 DN offset. Lens shading is four 7x9 grids, 252 Q12 gains, bilinear interpolation and saturated RAW10. Manual gains are Q8.8 with an exposed 0..16x range. FPGA automatic gains use a narrower 0.25..4x feedback bound, preceding-frame admitted samples and quarter-step smoothing.

CCM uses nine signed Q20 coefficients and three offsets, with exposed [-8,8) values, vendor product intermediates and truncation before RGB8. Gamma uses three independent 256-byte LUTs. Median and bilateral are 3x3; bilateral has 1..255 DN range sigma and 0.25..4 pixel spatial sigma. Sharpening is the saturated cross-Laplacian kernel with 0..1 strength. Full algorithm interpretation, configuration and scene limitations appear in the companion User Manual.

CLAHE maps RGB value over 4x4 tiles using previous-frame histograms and clip multiplier 1..8. Internal minimum is 32 pixels per axis, width multiple 8 and height multiple 4, in addition to public image limits. First/start/reset/geometry/clip-change frame bypasses. This temporal dependency is separate from measured camera-to-host latency.

Adaptive tone uses previous-input mean (R+2G+B)/4 and a common monotonic channel curve. Q8 gain is 128..512, slewed by at most 16 per frame; target is 16..224, default 128. Endpoints are fixed and first/reset frame uses unity. It is an RGB8 display curve, not radiometric HDR, and can alter color ratios. Natural-scene benefit has not been qualified.

## Control and statistics contract

Structural geometry, packing, routing and table changes require stopped/drained acquisition and valid ownership. The live Black Level exception requires the audited image, healthy active RAW10 processing and no recovery/UserSet conflict. It applies at a subsequent complete HLS invocation without restart; readback is requested state rather than a tagged applied frame.

FpgaStatisticsLatch stabilizes complete-frame color/scene fields. Sequence is not the GVSP FrameID. Color means precede gains/CCM and exclude low/saturated admissions; output intensity follows processing. Focus measures spatial luma variation and motion measures differences of 32x32 bin means. First-frame motion is invalid. Adaptive gain/mean are unlatched and outside the common latch. Sensor telemetry is cached, with high-bandwidth deferral above 105 MB/s; it is not an effective-exposure frame tag. Sources: [D3], [D4], [E1], [E3], [E4].

# Performance and implementation resources

## Interpreting the measurements

Performance tables identify finite windows, format, geometry and version. Approximately 14.29 FPS full-input processed preview is a different path from native sensor VGA near 30 FPS. Full-resolution network output is constrained by the configured 100 MB/s payload budget and the receiver. Numbers are observations, not minimum/maximum guarantees over temperature, all settings or indefinite runtime.

The v9 CLAHE comparison used five-second windows and observed no material throughput reduction in four stated cases. The v10 live Black Level test used two six-second 86-frame series. The v11 acceptance used 20 us packet spacing, deterministic patterns and 500 completed frames across eight cases. It verified exact first/last pixels and history behavior, plus preserved 2x/live-black/LED behavior. No real-scene tone quality or endurance follows from those patterns.

The November 2 diagnostic delivered 57 YUYV 4x frames at 14.293775 FPS with no new CSI/drop/recovery. RGB4x delivered zero, raised the line-buffer-full mask and recovered. Its CSI value 12115 counts error-bearing polls, not damaged frames. The evidence supports a downstream byte-pressure hypothesis but does not identify the first stalled stage or prove a particular hardware remedy.

## FPGA capacity and timing

The accepted v11 routed design at 200 MHz uses 41706 of 47232 LUTs, 44420 of 94464 FFs, 107.5 of 150 BRAM36 equivalents and 235 of 240 DSPs. Setup/hold/pulse-width slack is +0.382/+0.010/+0.145 ns. Only five DSPs remain; small nominal capacity does not imply an unbudgeted additional ISP or AI stage will meet timing.

The corrected v12 combined design routed at 200 MHz with 43841 LUTs, 47211 FFs, 124 BRAM36 and 235 DSPs. Setup/hold/pulse slack is +0.477/+0.010/+0.145ns; all 88667 nets route without route errors or DRC errors/critical warnings. Its own bounded camera acceptance passes. The initial failed candidate used43830 LUTs and different timing; its reports are not substituted for the corrected image. [E7]

CLAHE's standalone maximum-frame interval estimate of 10081822 cycles, 50.409 ms at 200 MHz, is not end-to-end camera latency. Similarly, tone's HLS pixel II=1 and small-frame RTL cycle count do not establish delivered network throughput. Sources: [E1], [E3], [E4], [E5], [E6].

# Protocol and persistent configuration

## Transport contract

GVCP uses UDP port 3956 and exposes a virtual device register map with big-endian numeric serialization. GenApi XML is served through the bootstrap Local URL/READMEM path. Standard bootstrap locations and vendor Fpga nodes coexist. The virtual addresses are not unrestricted ARM MMIO or DDR access.

Packet size is 576..1500 IPv4 bytes; normal payload at 1500 is 1464 bytes. Source stream port is 49152, destination/port are assigned by the receiver. Timestamp frequency is 1 MHz. Classic block and packet IDs and bounded resend retention are implemented. No jumbo, extended-ID, multicast, synchronized PTP or multi-stream guarantee is implied by the advertised protocol revision.

The controller is identified by IP and source port. The source heartbeat default is 3000 ms with 500..60000 ms limits. Valid owner requests renew the lease. Expiry stops/clears capture, destination, locks and ownership. Duplicate control requests are handled without intentionally repeating side effects; multi-register writes remain sequential and can partially apply before an error.

## Storage behavior

UserSet selection, load, save and default-at-boot selection are separate operations. Default is protected against standard UserSetSave; UserSet1..7 are ordinary saved slots. Current ISPA 1772-byte profiles carry 30 FPGA scalars plus table data; older ISP9 1764-byte records decode with adaptive off/target128. An incompatible tone-enabled import is rejected on older hardware. A successful codec test does not replace hardware Flash acceptance.

Image presets do not include stream packet settings or volatile remap maps. Persistent network/identity settings use a CRC journal and apply on reset/boot. Temporary ForceIP does not write the saved policy. DeviceReset applies startup state and releases acquisition/ownership. Firmware/FPGA RAM loading and Flash boot programming are distinct maintenance operations. Sources: [P1], [R1], [E4].

# Distortion qualification and remaining gaps

## Bounded distortion qualification

The bounded profile is RGB8, unchanged output canvas, nearest sampling, at most 640x480, plus/minus seven source rows. Full-size bypass remains supported. Two dedicated DDR map banks total 2457600 bytes. Packed uint32 inverse coordinates carry validity, 10-bit x, 9-bit y and reserved-zero bits; sequential upload and CRC validation precede enable. Maps are volatile and invalidated by structural changes, preset load or sensor reset.

The initial enabled identity-map acquisition failed, and its rejected DMA pixels matched baseline. The corrected VPSS byte-qualifier boundary then passed 11 scaling RTL cases, stalled-stream tests and its own board remap sequence. Identity, same-map restart and (+5,+7)/(-5,-7) synthetic translations pass exact pixels/borders at approximately 14.294 FPS,57 frames per map case and 40 us packet spacing, with no observed packet loss, CSI errors or recovery. CRC rejection, access guards, stop/start and geometry invalidation pass. Native full 2592x1944 RGB bypass has six frames at a requested 1 FPS; it is not maximum native FPS acceptance. No optical lens-calibration claim follows. [E7]

## Remaining measured quantities

The release does not specify dynamic range, SNR, read noise, quantum efficiency, color error, MTF, defect-removal probability, absolute exposure accuracy, lens distortion residual, end-to-end latency, sustained packet-loss rate, power consumption or thermal drift. Those require calibrated capture conditions and measurement procedures. Real photographs are provided to illustrate feature behavior, not to create substitute numeric specifications.

True multi-exposure HDR remains offline pending effective exposure/frame association and integrated ownership. Sensor VCM/MCU/FREX remain unavailable unless their capability is explicitly provisioned and qualified. No inference or custom training is deployed. A 24-hour campaign is postponed; short finite tests cannot close it. The source release also retains clean-checkout and third-party packaging gates. Sources: [D5], [D6], [E6], [E7].
