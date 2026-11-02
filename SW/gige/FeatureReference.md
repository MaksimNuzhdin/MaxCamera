# GenICam feature reference

Generated from `tools/gige_feature_catalog.py`. Addresses are virtual GVCP words.
Sensor writes that touch hardware require stopped acquisition. Staged tables can be edited while streaming and committed after Stop.
Hardware status/auto-control telemetry is cached and refreshed one sensor register per20ms.
Fan thresholds use the hardware10-bit thermal ADC. PWM applies when thermal hysteresis enables the fan; no RPM feedback is available.

| Feature | Group | Address | Access | Values / notes |
|---|---|---|---|---|
| SensorInitialized | Diagnostics | 0x00011000 | RO | 0…1 raw |
| SensorBusy | Diagnostics | 0x00011004 | RO | 0…1 raw |
| SensorState | Diagnostics | 0x00011008 | RO | Bound, Ready, Configured, Streaming, Fault |
| SensorLastError | Diagnostics | 0x0001100C | RO | -1000…0 raw |
| SensorTransportError | Diagnostics | 0x00011010 | RO | -1000…0 raw |
| SensorErrorRegister | Diagnostics | 0x00011014 | RO | 0…65535 raw |
| DeviceTemperature | BoardControl | 0x00011018 | RO | FPGA die temperature from board ADC, not the image sensor temperature. |
| FanStartTemperature | BoardControl | 0x0001101C | RW | Hardware hysteresis upper threshold. Must exceed FanStopTemperature. |
| FanStopTemperature | BoardControl | 0x00011020 | RW | Hardware hysteresis lower threshold. Must be below FanStartTemperature. |
| FanDutyCycle | BoardControl | 0x00011024 | RW | PWM duty when hardware thermal hysteresis enables the fan. No tachometer/RPM feedback is wired. |
| StatusLEDs | BoardControl | 0x00011028 | RO | Logical active-high status bits: 0 heartbeat, 1 sensor ready, 2 GigE ready, 3 streaming blink on images with independent video reset (version 10 and later). Physical LEDs are active-low. |
| GigEVisionReady | Diagnostics | 0x0001102C | RO | 0…1 raw |
| EthernetLinkUp | Diagnostics | 0x00011030 | RO | 0…1 raw |
| DeviceUptime | Diagnostics | 0x00011034 | RO | 0…4294967295 raw |
| CapturedFrames | Diagnostics | 0x00011038 | RO | 0…4294967295 raw |
| DroppedFrames | Diagnostics | 0x0001103C | RO | 0…4294967295 raw |
| CsiErrorEvents | Diagnostics | 0x00011040 | RO | 0…4294967295 raw |
| TransmittedFrames | Diagnostics | 0x00011044 | RO | 0…4294967295 raw |
| StreamSendRetries | Diagnostics | 0x00011048 | RO | 0…4294967295 raw |
| EthernetReceivedPackets | Diagnostics | 0x0001104C | RO | 0…4294967295 raw |
| EthernetTransmittedPackets | Diagnostics | 0x00011050 | RO | 0…4294967295 raw |
| EthernetErrors | Diagnostics | 0x00011054 | RO | 0…4294967295 raw |
| EthernetReceiveDrops | Diagnostics | 0x00011058 | RO | 0…4294967295 raw |
| DiagnosticUartBaudRate | Diagnostics | 0x0001105C | RO | 0…4294967295 raw |
| SystemTimerFrequency | Diagnostics | 0x00011060 | RO | 0…4294967295 raw |
| SensorI2cFrequency | Diagnostics | 0x00011064 | RO | 0…4294967295 raw |
| SensorChipID | Diagnostics | 0x00011068 | RO | 0…65535 raw |
| SensorInputClock | SensorTiming | 0x0001106C | RO | Configured 12MHz module XCLK assumption, supported by measured frame timing. |
| SensorSystemClock | SensorTiming | 0x00011070 | RO | 0…4294967295 raw |
| SensorLineLength | SensorTiming | 0x00011074 | RO | 0…65535 raw |
| SensorFrameLength | SensorTiming | 0x00011078 | RO | 0…65535 raw |
| AcquisitionFrameRate | SensorTiming | 0x0001107C | RW | Sensor frame rate limit through vertical blanking only; clocks remain unchanged. Actual transmitted rate can be lower because of conversion or network bandwidth. |
| SensorLinePeriod | SensorTiming | 0x00011080 | RO | 0…4294967295 raw |
| ExposureAuto | SensorControl | 0x00011084 | RW | Off, Continuous |
| GainAuto | SensorControl | 0x00011088 | RW | Off, Continuous |
| ExposureTime | SensorControl | 0x0001108C | RW | Exposure in microseconds; Line period follows the active image mode. Stop acquisition and set ExposureAuto=Off to change. |
| GainFactor | SensorControl | 0x00011090 | RW | 16…1023 raw |
| ExposureLimit50Hz | SensorControl | 0x00011094 | RW | 1…65531 raw |
| ExposureLimit60Hz | SensorControl | 0x00011098 | RW | 1…65531 raw |
| AutoGainLimit | SensorControl | 0x0001109C | RW | 16…1023 raw |
| SensorNightMode | SensorControl | 0x000110A0 | RW | 0…1 raw |
| SensorBanding | SensorControl | 0x000110A4 | RW | Off, Auto, Hz50, Hz60 |
| BalanceWhiteAuto | WhiteBalance | 0x000110A8 | RW | Off, Continuous |
| WhiteBalanceRed | WhiteBalance | 0x000110AC | RW | 1…4095 raw |
| WhiteBalanceGreen | WhiteBalance | 0x000110B0 | RW | 1…4095 raw |
| WhiteBalanceBlue | WhiteBalance | 0x000110B4 | RW | 1…4095 raw |
| ReverseX | SensorControl | 0x000110B8 | RW | 0…1 raw |
| ReverseY | SensorControl | 0x000110BC | RW | 0…1 raw |
| TestPattern | TestControl | 0x000110C0 | RW | Off, ColorBars, Random, Square, Black |
| SensorTestPatternStyle | TestControl | 0x000110C4 | RW | 0…3 raw |
| SensorTestRolling | TestControl | 0x000110C8 | RW | 0…1 raw |
| SensorTestTransparent | TestControl | 0x000110CC | RW | 0…1 raw |
| SensorTestMonochrome | TestControl | 0x000110D0 | RW | 0…1 raw |
| SensorIspControl0 | IspControl | 0x000110D4 | RW | Masked sensor ISP control. Reserved bits are preserved. |
| SensorIspControl1 | IspControl | 0x000110D8 | RW | Masked sensor ISP control. Reserved bits are preserved. |
| SensorIspControl3 | IspControl | 0x000110DC | RW | Masked sensor ISP control. Reserved bits are preserved. |
| EffectEnableBits | ImageEffects | 0x000110E0 | RW | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectHueCos | ImageEffects | 0x000110E4 | RW | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectHueSin | ImageEffects | 0x000110E8 | RW | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectU | ImageEffects | 0x000110EC | RW | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectV | ImageEffects | 0x000110F0 | RW | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectYOffset | ImageEffects | 0x000110F4 | RW | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectContrast | ImageEffects | 0x000110F8 | RW | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectBrightness | ImageEffects | 0x000110FC | RW | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectSigns | ImageEffects | 0x00011100 | RW | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectManualSaturation | ImageEffects | 0x00011104 | RW | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectUvThreshold1 | ImageEffects | 0x00011108 | RW | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectUvThreshold2 | ImageEffects | 0x0001110C | RW | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectApply | ImageEffects | 0x00011110 | WO | 0…1 raw |
| GammaPointSelector | ImageCalibration | 0x00011114 | RW | Staged table setting; execute the corresponding Apply command while stopped. |
| GammaPointValue | ImageCalibration | 0x00011118 | RW | Staged table setting; execute the corresponding Apply command while stopped. |
| GammaControl | ImageCalibration | 0x0001111C | RW | Staged table setting; execute the corresponding Apply command while stopped. |
| LensCorrectionSelector | ImageCalibration | 0x00011120 | RW | Staged table setting; execute the corresponding Apply command while stopped. |
| LensCorrectionValue | ImageCalibration | 0x00011124 | RW | Staged table setting; execute the corresponding Apply command while stopped. |
| LensCorrectionEnable | ImageCalibration | 0x00011128 | RW | Staged table setting; execute the corresponding Apply command while stopped. |
| GammaApply | ImageCalibration | 0x0001112C | WO | 0…1 raw |
| LensCorrectionApply | ImageCalibration | 0x00011130 | WO | 0…1 raw |
| OverlayLeft | SensorOverlay | 0x00011134 | RW | 0…4095 raw |
| OverlayRight | SensorOverlay | 0x00011138 | RW | 0…4095 raw |
| OverlayTop | SensorOverlay | 0x0001113C | RW | 0…2047 raw |
| OverlayBottom | SensorOverlay | 0x00011140 | RW | 0…2047 raw |
| OverlayU | SensorOverlay | 0x00011144 | RW | 0…255 raw |
| OverlayV | SensorOverlay | 0x00011148 | RW | 0…255 raw |
| OverlayEnable | SensorOverlay | 0x0001114C | RW | 0…1 raw |
| OverlayApply | SensorOverlay | 0x00011150 | WO | 0…1 raw |
| SensorVcmAvailable | SensorActuator | 0x00011154 | RO | Per-unit capability. Enable only for a module with a wired VCM actuator. |
| VcmTarget | SensorActuator | 0x00011158 | RW | 0…1023 raw |
| VcmSlew | SensorActuator | 0x0001115C | RW | 0…15 raw |
| VcmDivider | SensorActuator | 0x00011160 | RW | 0…4095 raw |
| VcmCurrent | SensorActuator | 0x00011164 | RW | 0…7 raw |
| VcmRih | SensorActuator | 0x00011168 | RW | 0…1 raw |
| VcmPowerDown | SensorActuator | 0x0001116C | RW | 0…1 raw |
| VcmApply | SensorActuator | 0x00011170 | WO | 0…1 raw |
| SensorRegisterSelector | SensorRegisters | 0x00011174 | RW | Named register catalogue from Ov5640Registers.h. Register values require stopped acquisition. |
| SensorRegisterValue | SensorRegisters | 0x00011178 | RO | 0…255 raw |
| SensorFeatureMask | SensorRegisters | 0x0001117C | RW | 0…255 raw |
| SensorFeatureValue | SensorRegisters | 0x00011180 | RW | 0…255 raw |
| SensorFeatureApply | SensorRegisters | 0x00011184 | WO | Stopped-only masked write to supported ISP/analog image-control ranges. Clock/reset/format/OTP/MCU registers are write-protected. |
| SensorOtpSelector | SensorIdentity | 0x00011188 | RW | 0…31 raw |
| SensorOtpValue | SensorIdentity | 0x0001118C | RO | 0…255 raw |
| SensorOtpRefresh | SensorIdentity | 0x00011190 | WO | Refresh the 32-byte OTP shadow. Read only; never programs OTP. |
| SensorJpegAvailable | SensorCapabilities | 0x00011194 | RO | JPEG transport is implemented in the matching CSI-byte production image; select VGA YUYV and ImageCompressionMode=JPEG. |
| SensorFrexAvailable | SensorCapabilities | 0x00011198 | RO | Unavailable until the corresponding board/transport integration is qualified. |
| SensorMcuFirmwareAvailable | SensorCapabilities | 0x0001119C | RO | Unavailable until the corresponding board/transport integration is qualified. |
| SensorOtpValid | SensorIdentity | 0x000111A0 | RO | The OTP read shadow becomes valid after SensorOtpRefresh succeeds. |
| ExposureMaximumLines | SensorTiming | 0x000111A4 | RO | 1…65531 raw |
| AcquisitionFrameRateEnable | SensorTiming | 0x000111A8 | RW | 0…1 raw |
| AcquisitionFrameRateMinimum | SensorTiming | 0x000111AC | RO | 1…30000 raw |
| AcquisitionFrameRateMaximum | SensorTiming | 0x000111B0 | RO | 1…30000 raw |
| ResultingFrameRate | SensorTiming | 0x000111B4 | RO | Nominal sensor rate from current HTS/VTS, not network delivery FPS. |
| AutoExposureTarget | SensorControl | 0x000111B8 | RW | Average-luminance target in sensor units; updates stable and fast AEC thresholds. |
| SensorSharpness | IspControl | 0x000111BC | RW | Manual CIP strength in sensor register units. Has no effect on RAW output. |
| SensorDenoise | IspControl | 0x000111C0 | RW | Manual CIP strength in sensor register units. Has no effect on RAW output. |
| SensorSharpnessAuto | IspControl | 0x000111C4 | RW | Off, Continuous |
| SensorDenoiseAuto | IspControl | 0x000111C8 | RW | Off, Continuous |
| SensorAverageLuminance | Diagnostics | 0x000111CC | RO | Sensor AEC weighted luminance readout; refreshed with live telemetry. |
| UserSetStorageAvailable | UserSetControl | 0x000111D0 | RO | 0…1 raw |
| UserSetBusy | UserSetControl | 0x000111D4 | RO | 0…1 raw |
| UserSetLastError | UserSetControl | 0x000111D8 | RO | -1000…0 raw |
| UserSetSelector | UserSetControl | 0x000111DC | RW | Default, UserSet1, UserSet2, UserSet3, UserSet4, UserSet5, UserSet6, UserSet7 |
| UserSetLoad | UserSetControl | 0x000111E0 | RW | 0…1 raw |
| UserSetSave | UserSetControl | 0x000111E4 | RW | Save UserSet1..7. Default is read-only through this standard command; factory provisioning uses the password-protected FactoryUserSetSave command. |
| UserSetDefault | UserSetControl | 0x000111E8 | RW | Default, UserSet1, UserSet2, UserSet3, UserSet4, UserSet5, UserSet6, UserSet7 |
| UserSetValid | UserSetControl | 0x000111EC | RO | 0…1 raw |
| UserSetSavedMask | UserSetControl | 0x000111F0 | RO | 0…255 raw |
| FactoryAccessUnlocked | UserSetControl | 0x000111F4 | RO | 0…1 raw |
| FeatureAccessLock | UserSetControl | 0x000111F8 | WO | 0…1 raw |
| NativePixelFormatsAvailable | Diagnostics | 0x000111FC | RO | 0…1 raw |
| WidthIncrement | SensorTiming | 0x00011200 | RO | 2…16 raw |
| RawPixelFormatActive | Diagnostics | 0x00011204 | RO | 0…1 raw |
| ImageCompressionMode | SensorCapabilities | 0x00011208 | RW | Off, JPEG |
| SensorJpegQuality | SensorCapabilities | 0x0001120C | RW | OV5640 quantization scale, smaller values give higher quality/larger encoded frames. |
| WidthMaximum | SensorTiming | 0x00011210 | RO | 160…2592 raw |
| HeightMaximum | SensorTiming | 0x00011214 | RO | 120…1944 raw |
| PacketResendRequests | Diagnostics | 0x00011218 | RO | 0…4294967295 raw |
| RetransmittedPackets | Diagnostics | 0x0001121C | RO | 0…4294967295 raw |
| PacketResendUnavailable | Diagnostics | 0x00011220 | RO | 0…4294967295 raw |
| PacketResendQueueOverflows | Diagnostics | 0x00011224 | RO | 0…4294967295 raw |
| CameraRecoveryCount | Diagnostics | 0x00011228 | RO | 0…4294967295 raw |
| DeviceConfigurationPending | NetworkControl | 0x0001122C | RO | Network/identity changes are journaled automatically while stopped. IP mode and identity apply on the next firmware boot. |
| DeviceConfigurationLastError | NetworkControl | 0x00011230 | RO | -1000…0 raw |
| DeviceIdentityProvisioned | NetworkControl | 0x00011234 | RO | 0…1 raw |
| DeviceReset | DeviceControl | 0x00011238 | WO | Asynchronously return to the power-up configuration: reset sensor/video and protocol, load UserSetDefault, and apply saved network/identity. Acquisition and ownership are released. Rediscover/reconnect after completion. |
| DeviceResetBusy | DeviceMaintenance | 0x0001123C | RO | 0…1 raw |
| DeviceResetLastError | DeviceMaintenance | 0x00011240 | RO | -1000…0 raw |
| SensorTelemetryDeferred | Diagnostics | 0x00011244 | RO | High-bandwidth streaming defers SCCB telemetry above 105 MB/s to preserve delivered FPS. Cached values refresh during supported active acquisition or synchronous sensor operations; stopped background reads are disabled. |
| FactoryUserSetSave | UserSetControl | 0x00011248 | RW | Factory maintenance only: save the current profile into the factory slot. Requires UserSetSelector=Default and an unlocked factory password. Not part of standard UserSetSave. |
| FpgaProcessingVersion | FpgaImageProcessing | 0x0001124C | RO | 0…4294967295 raw |
| FpgaProcessingCapabilities | FpgaImageProcessing | 0x00011250 | RO | Integrated PL capabilities: bit 0 black-level subtraction, bit 1 Bayer bad-pixel correction, bit 2 manual gains, bit 3 RGB demosaic, bit 4 RGB median, bit 5 ROI/area downscale, bit 6 CCM, bit 7 gamma, bit 8 FPGA AWB/statistics, bit 9 bilateral filtering, bit 10 sharpening, bit 11 RAW lens shading, bit 12 focus/motion analysis, bit 13 RGB-to-YUV output, bit 14 Xilinx polyphase resampling, bit 15 local contrast with shared RAW preprocessing, bit 16 adaptive RGB display tone mapping, bit 17 bounded integer-map remapping, bit 18 four-byte-per-clock framebuffer writer. Capability bits describe the installed FPGA image. |
| FpgaProcessingAvailable | FpgaImageProcessing | 0x00011254 | RO | 0…1 raw |
| FpgaProcessingEnable | FpgaImageProcessing | 0x00011258 | RW | Applied at the next AcquisitionStart while stopped. Processing supports BayerBG10/BayerBG10p and, when color capability is present, RGB8/BGR8. Other formats use byte bypass. |
| FpgaBadPixelCorrectionEnable | FpgaImageProcessing | 0x0001125C | RW | Applied at the next AcquisitionStart while stopped. Processing supports BayerBG10/BayerBG10p and, when color capability is present, RGB8/BGR8. Other formats use byte bypass. |
| FpgaBlackLevel | FpgaImageProcessing | 0x00011260 | RW | 10-bit RAW black offset before BPC and gains. On audited v9/v10 active RAW processing, a live write applies to a subsequent complete HLS frame without restarting acquisition. Readback is the requested setting, not a frame-tagged applied value. Otherwise configure while stopped; applies at next AcquisitionStart. |
| FpgaBalanceRed | FpgaImageProcessing | 0x00011264 | RW | Q8.8 Bayer gain, unity 1.0. Applied after BPC, saturated to 10 bits. Applies at next AcquisitionStart. |
| FpgaBalanceGreen | FpgaImageProcessing | 0x00011268 | RW | Q8.8 Bayer gain, unity 1.0. Applied after BPC, saturated to 10 bits. Applies at next AcquisitionStart. |
| FpgaBalanceBlue | FpgaImageProcessing | 0x0001126C | RW | Q8.8 Bayer gain, unity 1.0. Applied after BPC, saturated to 10 bits. Applies at next AcquisitionStart. |
| FpgaProcessingActive | FpgaImageProcessing | 0x00011270 | RO | True during acquisition when a supported RAW10 or RGB processing path is selected. |
| FpgaMedianAvailable | FpgaImageProcessing | 0x00011274 | RO | Reports whether the installed FPGA image includes this RGB processing block. |
| FpgaScalerAvailable | FpgaImageProcessing | 0x00011278 | RO | Reports whether the installed FPGA image includes this RGB processing block. |
| FpgaMedianEnable | FpgaImageProcessing | 0x0001127C | RW | Applied at the next AcquisitionStart. Requires FPGA processing and RGB8/BGR8 or supported FPGA YUV output. |
| FpgaScalerEnable | FpgaImageProcessing | 0x00011280 | RW | Applied at the next AcquisitionStart. Requires FPGA processing and RGB8/BGR8 or supported FPGA YUV output. |
| FpgaRoiEnable | FpgaImageProcessing | 0x00011284 | RW | Applied at the next AcquisitionStart. Requires FPGA processing and RGB8/BGR8 or supported FPGA YUV output. |
| FpgaInputWidth | FpgaImageProcessing | 0x00011288 | RW | Input and ROI dimensions for FPGA color resampling. Width/Height select output size. ROI must fit the RAW input. Area mode only reduces; Polyphase permits up to 4x enlargement per ROI axis within the 2592x1944 output bounds. Mixed up/down axes are supported. Validated at AcquisitionStart. |
| FpgaInputHeight | FpgaImageProcessing | 0x0001128C | RW | Input and ROI dimensions for FPGA color resampling. Width/Height select output size. ROI must fit the RAW input. Area mode only reduces; Polyphase permits up to 4x enlargement per ROI axis within the 2592x1944 output bounds. Mixed up/down axes are supported. Validated at AcquisitionStart. |
| FpgaRoiOffsetX | FpgaImageProcessing | 0x00011290 | RW | Input and ROI dimensions for FPGA color resampling. Width/Height select output size. ROI must fit the RAW input. Area mode only reduces; Polyphase permits up to 4x enlargement per ROI axis within the 2592x1944 output bounds. Mixed up/down axes are supported. Validated at AcquisitionStart. |
| FpgaRoiOffsetY | FpgaImageProcessing | 0x00011294 | RW | Input and ROI dimensions for FPGA color resampling. Width/Height select output size. ROI must fit the RAW input. Area mode only reduces; Polyphase permits up to 4x enlargement per ROI axis within the 2592x1944 output bounds. Mixed up/down axes are supported. Validated at AcquisitionStart. |
| FpgaRoiWidth | FpgaImageProcessing | 0x00011298 | RW | Input and ROI dimensions for FPGA color resampling. Width/Height select output size. ROI must fit the RAW input. Area mode only reduces; Polyphase permits up to 4x enlargement per ROI axis within the 2592x1944 output bounds. Mixed up/down axes are supported. Validated at AcquisitionStart. |
| FpgaRoiHeight | FpgaImageProcessing | 0x0001129C | RW | Input and ROI dimensions for FPGA color resampling. Width/Height select output size. ROI must fit the RAW input. Area mode only reduces; Polyphase permits up to 4x enlargement per ROI axis within the 2592x1944 output bounds. Mixed up/down axes are supported. Validated at AcquisitionStart. |
| SensorRawActive | Diagnostics | 0x000112A0 | RO | The sensor uses its Bayer RAW tap, including FPGA RGB conversion. Sensor AWB and orientation are unavailable in this mode. |
| FpgaColorAdjustmentAvailable | FpgaImageProcessing | 0x000112A4 | RO | 0…1 raw |
| FpgaColorTransformationEnable | FpgaImageProcessing | 0x000112A8 | RW | 0…1 raw |
| FpgaGammaEnable | FpgaImageProcessing | 0x000112AC | RW | 0…1 raw |
| FpgaBalanceWhiteAuto | FpgaImageProcessing | 0x000112B0 | RW | Off, Continuous |
| FpgaColorTransformationSelector | FpgaImageProcessing | 0x000112B4 | RW | Stopped-only table selector. Matrix order is RGB rows then RGB columns. Channel order is red, green, blue. |
| FpgaColorOffsetSelector | FpgaImageProcessing | 0x000112B8 | RW | Stopped-only table selector. Matrix order is RGB rows then RGB columns. Channel order is red, green, blue. |
| FpgaGammaChannel | FpgaImageProcessing | 0x000112BC | RW | Stopped-only table selector. Matrix order is RGB rows then RGB columns. Channel order is red, green, blue. |
| FpgaGammaIndex | FpgaImageProcessing | 0x000112C0 | RW | Stopped-only table selector. Matrix order is RGB rows then RGB columns. Channel order is red, green, blue. |
| FpgaColorTransformationValue | FpgaImageProcessing | 0x000112C4 | RW | Signed Q20 coefficient or DN offset in [-8,8). Applied before RGB8 conversion; vendor Q8 product intermediates and truncation apply. |
| FpgaColorOffsetValue | FpgaImageProcessing | 0x000112C8 | RW | Signed Q20 coefficient or DN offset in [-8,8). Applied before RGB8 conversion; vendor Q8 product intermediates and truncation apply. |
| FpgaGammaValue | FpgaImageProcessing | 0x000112CC | RW | Selected channel/index RGB8 LUT value. Default is identity. All 768 LUT bytes persist in UserSet. |
| FpgaStatisticsLatch | FpgaImageProcessing | 0x000112D0 | WO | Snapshot the latest complete FPGA frame statistics. Allowed during acquisition; read fields remain stable until the next latch. |
| FpgaStatisticsSequence | FpgaImageProcessing | 0x000112D4 | RO | Latched complete-frame statistics. Sequence restarts at acquisition reset and is not the GVSP FrameID. |
| FpgaStatisticsValidPixels | FpgaImageProcessing | 0x000112D8 | RO | Latched complete-frame statistics. Sequence restarts at acquisition reset and is not the GVSP FrameID. |
| FpgaClippedPixels | FpgaImageProcessing | 0x000112DC | RO | Latched complete-frame statistics. Sequence restarts at acquisition reset and is not the GVSP FrameID. |
| FpgaDarkPixels | FpgaImageProcessing | 0x000112E0 | RO | Latched complete-frame statistics. Sequence restarts at acquisition reset and is not the GVSP FrameID. |
| FpgaBalanceMeanRed | FpgaImageProcessing | 0x000112E4 | RO | Latched Q16 mean. Color means precede gains/CCM and exclude samples with any channel below 8 or at least 1000 DN. Intensity is output RGB8 luma. |
| FpgaBalanceMeanGreen | FpgaImageProcessing | 0x000112E8 | RO | Latched Q16 mean. Color means precede gains/CCM and exclude samples with any channel below 8 or at least 1000 DN. Intensity is output RGB8 luma. |
| FpgaBalanceMeanBlue | FpgaImageProcessing | 0x000112EC | RO | Latched Q16 mean. Color means precede gains/CCM and exclude samples with any channel below 8 or at least 1000 DN. Intensity is output RGB8 luma. |
| FpgaIntensityMean | FpgaImageProcessing | 0x000112F0 | RO | Latched Q16 mean. Color means precede gains/CCM and exclude samples with any channel below 8 or at least 1000 DN. Intensity is output RGB8 luma. |
| FpgaAppliedBalanceRed | FpgaImageProcessing | 0x000112F4 | RO | Latched gains actually applied to the completed frame, independent of configured manual gains during AWB. |
| FpgaAppliedBalanceGreen | FpgaImageProcessing | 0x000112F8 | RO | Latched gains actually applied to the completed frame, independent of configured manual gains during AWB. |
| FpgaAppliedBalanceBlue | FpgaImageProcessing | 0x000112FC | RO | Latched gains actually applied to the completed frame, independent of configured manual gains during AWB. |
| FpgaDetailProcessingAvailable | FpgaImageProcessing | 0x00011300 | RO | 0…1 raw |
| FpgaBilateralEnable | FpgaImageProcessing | 0x00011304 | RW | AMD 3x3 RGB bilateral filter after gamma/median and before ROI/scaling. Replicate border; disabled by default. |
| FpgaBilateralSigmaColor | FpgaImageProcessing | 0x00011308 | RW | RGB bilateral range sigma; default 24 DN. |
| FpgaBilateralSigmaSpace | FpgaImageProcessing | 0x0001130C | RW | RGB bilateral spatial sigma; Q8 value, default 1 pixel. |
| FpgaSharpenStrength | FpgaImageProcessing | 0x00011310 | RW | RGB cross-Laplacian strength [0,1] using AMD filter2D, saturated per channel. Zero bypasses. Constant-zero filter border can brighten image edges; assess halos and noise amplification. |
| FpgaLensShadingAvailable | FpgaImageProcessing | 0x00011314 | RO | 0…1 raw |
| FpgaLensShadingEnable | FpgaImageProcessing | 0x00011318 | RW | Programmable RAW10 lens shading after black/BPC and before demosaic or RAW manual gains. Four-phase 7x9 bilinear Q12 grid; default disabled with unity table. A real uniform-field calibration is required for optical correction. |
| FpgaLensShadingPhase | FpgaImageProcessing | 0x0001131C | RW | Blue, GreenBlueRow, GreenRedRow, Red |
| FpgaLensShadingRow | FpgaImageProcessing | 0x00011320 | RW | 0…6 raw |
| FpgaLensShadingColumn | FpgaImageProcessing | 0x00011324 | RW | 0…8 raw |
| FpgaLensShadingGain | FpgaImageProcessing | 0x00011328 | RW | Selected grid gain in [0,4], Q12 rounded interpolation and saturated RAW10 output. Default 1x. All 252 gains persist in UserSet; selectors do not. |
| FpgaSceneAnalysisAvailable | FpgaImageProcessing | 0x0001132C | RO | 0…1 raw |
| FpgaSceneAnalysisEnable | FpgaImageProcessing | 0x00011330 | RW | Post-ROI RGB luma focus/motion diagnostics. Both output dimensions must be at least 32 for analysis; smaller images pass with zero/invalid metrics. First frame after acquisition restart has invalid motion. Illumination changes can trigger motion, and noise can increase focus. |
| FpgaFocusMean | FpgaImageProcessing | 0x00011334 | RO | Mean absolute four-neighbor luma Laplacian over the output interior, Q16 DN. Sensitive to ISP settings and noise. Latched with the common completed-frame statistics sequence. |
| FpgaMotionMean | FpgaImageProcessing | 0x00011338 | RO | Mean absolute difference of current/previous 32x32 luma-bin means, Q16 DN. First frame is invalid. Latched with the common completed-frame statistics sequence. |
| FpgaMotionValid | FpgaImageProcessing | 0x0001133C | RO | Latched valid preceding-frame comparison. False when analysis is disabled, dimensions are below 32, or history was reset. |
| FpgaYuvOutputAvailable | FpgaImageProcessing | 0x00011340 | RO | 0…1 raw |
| FpgaYuvRange | FpgaImageProcessing | 0x00011344 | RW | Limited, Full |
| FpgaPolyphaseAvailable | FpgaImageProcessing | 0x00011348 | RO | 0…1 raw |
| FpgaScalerInterpolation | FpgaImageProcessing | 0x0001134C | RW | Area, Polyphase |
| FpgaLocalContrastAvailable | FpgaImageProcessing | 0x00011350 | RO | 0…1 raw |
| FpgaLocalContrastEnable | FpgaImageProcessing | 0x00011354 | RW | Four-by-four-tile Xilinx CLAHE on RGB value, after optional scaling. Preserves RGB ratios with bounded fixed-point rounding; black remains black. Requires color FPGA processing, output dimensions at least 32, height multiple of 4 and width multiple of 8. Uses the preceding frame histogram; first frame after Start, geometry/clip change or reset bypasses. Can amplify noise or react to illumination changes. Default off; no optical calibration is implied. |
| FpgaLocalContrastClipLimit | FpgaImageProcessing | 0x00011358 | RW | Integer Xilinx CLAHE histogram clip multiplier, 1 through 8, default 4. Higher values allow stronger contrast/noise amplification. Configure while stopped. |
| FpgaBlackLevelLiveActive | FpgaImageProcessing | 0x0001135C | RO | True only while healthy audited v9/v10 FPGA RAW processing accepts frame-latched live Black Level writes. False during stop/drain/recovery, bypass and legacy processing. Other controls retain their existing locks. |
| FpgaUpscaleAvailable | FpgaImageProcessing | 0x00011360 | RO | Firmware supports bounded enlargement using the installed VPSS (processing v8 or later). Requires processing/scaler enabled and Polyphase selected. No additional FPGA resources; camera FPS acceptance is separate. |
| FpgaUpscaleMaximum | FpgaImageProcessing | 0x00011364 | RO | Maximum supported enlargement per ROI axis: 4 with VPSS, otherwise 1 (no enlargement). Output remains at most 2592x1944 and subject to PixelFormat alignment and link bandwidth. Area never enlarges. |
| FpgaAdaptiveToneAvailable | FpgaImageProcessing | 0x00011368 | RO | Matching v11 image includes the adaptive RGB display curve after CLAHE and before final scene statistics/output packing. This is not HDR radiance reconstruction. |
| FpgaAdaptiveToneEnable | FpgaImageProcessing | 0x0001136C | RW | Opt-in shared per-channel rational RGB8 curve driven by preceding input-frame weighted luma. Black/white endpoints fixed; gain 0.5x to 2x changes by at most 1/16 per frame. Can change color ratios and temporal brightness. First/start/reset/dimension/target-change frame bypasses. Configure while stopped. Default off; requires color FPGA processing. |
| FpgaAdaptiveToneTarget | FpgaImageProcessing | 0x00011370 | RW | Desired preceding-input weighted-luma mean, (R+2G+B)/4, default 128. Target is a bounded adaptation reference, not guaranteed output brightness. Configure while stopped; new acquisition resets history. |
| FpgaAdaptiveToneAppliedGain | FpgaImageProcessing | 0x00011374 | RO | Unlatched current HLS invocation gain; unity before first valid invocation. Not associated with a specific GVSP frame or common statistics latch. |
| FpgaAdaptiveTonePreviousMean | FpgaImageProcessing | 0x00011378 | RO | Unlatched preceding input-frame weighted-luma mean used by the current HLS invocation. First/reset invocation reports zero. Not frame-tagged; gain/mean reads are not an atomic pair. |
| FpgaDistortionAvailable | FpgaImageProcessing | 0x0001137C | RO | Matching image/control support for bounded640x480 nearest correction, +/-7 source rows. Does not imply calibrated lens parameters. |
| FpgaDistortionEnable | FpgaImageProcessing | 0x00011380 | RW | Volatile correction enable. Requires a completely validated map matching current color geometry. Default off; full-resolution bypass remains supported. Not saved in UserSet. |
| FpgaDistortionMapWidth | FpgaImageProcessing | 0x00011384 | RW | Staged map dimension. Set only after Abort, before Begin, with correction disabled. Must match output geometry; no implicit scaling. |
| FpgaDistortionMapHeight | FpgaImageProcessing | 0x00011388 | RW | Staged map dimension. Set only after Abort, before Begin, with correction disabled. Must match output geometry; no implicit scaling. |
| FpgaDistortionExpectedCrc32 | FpgaImageProcessing | 0x0001138C | RW | CRC32 of raster little-endian32-bit map words, polynomial0xedb88320, standard initial/final XOR. Set before Begin; integrity check is not optical calibration. |
| FpgaDistortionMapBegin | FpgaImageProcessing | 0x00011390 | WO | Volatile map transfer command. Begin selects a staging bank; Commit starts bounded asynchronous validation; Abort invalidates staging without releasing a bank the reader may still access. Configure while stopped and disabled. |
| FpgaDistortionMapCommit | FpgaImageProcessing | 0x00011394 | WO | Volatile map transfer command. Begin selects a staging bank; Commit starts bounded asynchronous validation; Abort invalidates staging without releasing a bank the reader may still access. Configure while stopped and disabled. |
| FpgaDistortionMapAbort | FpgaImageProcessing | 0x00011398 | WO | Volatile map transfer command. Begin selects a staging bank; Commit starts bounded asynchronous validation; Abort invalidates staging without releasing a bank the reader may still access. Configure while stopped and disabled. |
| FpgaDistortionMapIndex | FpgaImageProcessing | 0x0001139C | RW | Raster map word index. Upload sequentially; explicit index/value pairs make identical retries idempotent. |
| FpgaDistortionMapData | FpgaImageProcessing | 0x000113A0 | RW | Map word: valid bit31, source x bits9:0, source y bits18:10, reserved bits30:19 zero. Zero means black. Coordinates must be in bounds and within seven source rows; unsupported entries fail validation. |
| FpgaDistortionMapState | FpgaImageProcessing | 0x000113A4 | RO | Empty, Uploading, Validating, Ready, Error |
| FpgaDistortionWordsUploaded | FpgaImageProcessing | 0x000113A8 | RO | 0…307200 raw |
| FpgaDistortionWordsValidated | FpgaImageProcessing | 0x000113AC | RO | 0…307200 raw |
| FpgaDistortionMapError | FpgaImageProcessing | 0x000113B0 | RO | None, InvalidCoordinate, ChecksumMismatch |
| FpgaDistortionHardwareStatus | FpgaImageProcessing | 0x000113B4 | RO | Last observed reader status: bit0 geometry, bit1 map encoding/window, bit2 input framing. Zero alone does not prove successful acquisition. |

## Register catalogue

SensorRegisterSelector provides 628 named choices from Ov5640Registers.h. SensorRegisterValue reads the selected byte while stopped.
SensorFeatureApply uses FeatureMask/FeatureValue and accepts stopped-only masked writes to AWB, CIP, CMX, gamma, SDE, LENC and BLC image-control ranges.
PLL, reset, geometry, transport, OTP programming and MCU execution stay write-protected. The complete named register catalogue remains available for inspection.
VCM controls are available only when GIGE_UNIT_VCM_AVAILABLE is provisioned for a verified actuator. JPEG uses the qualified CSI-byte production path, VGA YUYV and ImageCompressionMode=JPEG. FREX/MCU remain unavailable.

LED1: main-loop heartbeat (500ms high/500ms low). LED2: sensor/pipeline initialized and healthy. LED3: GVCP bound, Ethernet link up and usable IP address assigned.
LED3 does not depend on whether a client is connected. LED4 blinks while acquiring on compatible version 10 and later images with independent video reset; it is off when stopped. Legacy images reserve LED4 for video reset.
