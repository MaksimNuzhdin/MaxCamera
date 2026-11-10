"""Single metadata source for GenApi feature nodes and their firmware register IDs."""
FEATURES = []
def add(key, name, group, access='RW', minimum=0, maximum=255, **kw):
    f = dict(key=key, name=name, group=group, access=access, minimum=minimum,
             maximum=maximum, address=0x11000+4*len(FEATURES), **kw)
    FEATURES.append(f)
    return f

def sensor(key,name,group='SensorControl',**kw):
    kw.setdefault('available','SensorInitialized')
    if kw.get('access','RW') != 'RO':
        kw.setdefault('locked','SensorBusy')
    return add(key,name,group,**kw)

add('SENSOR_READY','SensorInitialized','Diagnostics',access='RO',maximum=1,kind='Boolean')
add('SENSOR_BUSY','SensorBusy','Diagnostics',access='RO',maximum=1,kind='Boolean')
add('SENSOR_STATE','SensorState','Diagnostics',access='RO',maximum=4,
    entries={'Bound':0,'Ready':1,'Configured':2,'Streaming':3,'Fault':4})
add('SENSOR_ERROR','SensorLastError','Diagnostics',access='RO',minimum=-1000,maximum=0,signed=True)
add('I2C_ERROR','SensorTransportError','Diagnostics',access='RO',minimum=-1000,maximum=0,signed=True)
add('ERROR_REGISTER','SensorErrorRegister','Diagnostics',access='RO',maximum=65535,representation='HexNumber')
add('TEMPERATURE','DeviceTemperature','BoardControl',access='RO',minimum=-273150,maximum=230000,
    signed=True,kind='Float',scale=1000,unit='C',standard=True,description='FPGA die temperature from board ADC, not the image sensor temperature.')
add('FAN_THRESHOLD','FanStartTemperature','BoardControl',maximum=1023,kind='TemperatureADC',description='Hardware hysteresis upper threshold. Must exceed FanStopTemperature.')
add('FAN_FLOOR','FanStopTemperature','BoardControl',maximum=1023,kind='TemperatureADC',description='Hardware hysteresis lower threshold. Must be below FanStartTemperature.')
add('FAN_DUTY','FanDutyCycle','BoardControl',maximum=100,unit='%',description='PWM duty when hardware thermal hysteresis enables the fan. No tachometer/RPM feedback is wired.')
add('LED_STATE','StatusLEDs','BoardControl',access='RO',maximum=15,representation='HexNumber',description='Logical active-high status bits: 0 heartbeat, 1 sensor ready, 2 GigE ready, 3 streaming blink on images with independent video reset (version 10 and later). Physical LEDs are active-low.')
add('GIGE_READY','GigEVisionReady','Diagnostics',access='RO',maximum=1,kind='Boolean')
add('LINK_UP','EthernetLinkUp','Diagnostics',access='RO',maximum=1,kind='Boolean')
add('UPTIME','DeviceUptime','Diagnostics',access='RO',maximum=4294967295,unit='s')
for key,name in [('CAPTURED','CapturedFrames'),('DROPPED','DroppedFrames'),('CSI_ERRORS','CsiErrorEvents'),
                 ('SENT','TransmittedFrames'),('TX_ERRORS','StreamSendRetries'),('RX_PACKETS','EthernetReceivedPackets'),
                 ('TX_PACKETS','EthernetTransmittedPackets'),('GEM_ERRORS','EthernetErrors'),('RX_DROPS','EthernetReceiveDrops')]:
    add(key,name,'Diagnostics',access='RO',maximum=4294967295)
add('UART_BAUD','DiagnosticUartBaudRate','Diagnostics',access='RO',maximum=4294967295)
add('TIMER_HZ','SystemTimerFrequency','Diagnostics',access='RO',maximum=4294967295,unit='Hz')
add('I2C_HZ','SensorI2cFrequency','Diagnostics',access='RO',maximum=4294967295,unit='Hz')
add('SENSOR_ID','SensorChipID','Diagnostics',access='RO',maximum=65535,representation='HexNumber')
add('SENSOR_XCLK','SensorInputClock','SensorTiming',access='RO',maximum=4294967295,unit='Hz',description='Configured 12MHz module XCLK assumption, supported by measured frame timing.')
add('SENSOR_SCLK','SensorSystemClock','SensorTiming',access='RO',maximum=4294967295,unit='Hz')
add('SENSOR_HTS','SensorLineLength','SensorTiming',access='RO',maximum=65535)
add('SENSOR_VTS','SensorFrameLength','SensorTiming',access='RO',maximum=65535)
sensor('SENSOR_FPS','AcquisitionFrameRate',group='SensorTiming',minimum=200,maximum=30000,kind='Float',scale=1000,unit='Hz',standard=True,description='Sensor frame rate limit through vertical blanking only; clocks remain unchanged. Actual transmitted rate can be lower because of conversion or network bandwidth.')
add('LINE_PERIOD','SensorLinePeriod','SensorTiming',access='RO',maximum=4294967295,kind='Float',scale=1000,unit='us')
sensor('EXPOSURE_AUTO','ExposureAuto',maximum=2,entries={'Off':0,'Continuous':2},standard=True)
sensor('GAIN_AUTO','GainAuto',maximum=2,entries={'Off':0,'Continuous':2},standard=True)
sensor('EXPOSURE_LINES','ExposureTime',minimum=1,maximum=65531,kind='Float',scale=1/32,unit='us',standard=True,locked='ExposureLocked',description='Exposure in microseconds; Line period follows the active image mode. Stop acquisition and set ExposureAuto=Off to change.')
sensor('GAIN','GainFactor',minimum=16,maximum=1023,kind='Float',scale=16,unit='x',locked='GainLocked')
sensor('MAX_EXPOSURE_50','ExposureLimit50Hz',minimum=1,maximum=65531,unit='lines')
sensor('MAX_EXPOSURE_60','ExposureLimit60Hz',minimum=1,maximum=65531,unit='lines')
sensor('MAX_GAIN','AutoGainLimit',minimum=16,maximum=1023,kind='Float',scale=16,unit='x')
sensor('NIGHT_MODE','SensorNightMode',maximum=1,kind='Boolean')
sensor('BANDING','SensorBanding',maximum=60,entries={'Off':0,'Auto':1,'Hz50':50,'Hz60':60})
sensor('AWB_AUTO','BalanceWhiteAuto',group='WhiteBalance',maximum=2,entries={'Off':0,'Continuous':2},standard=True,description='Continuous AWB is available in ISP formats. RAW has no valid AWB statistics; entering RAW seeds manual unity gains. Legacy RAW presets load with AWB Off and their saved gains.')
for key,name in [('WB_RED','WhiteBalanceRed'),('WB_GREEN','WhiteBalanceGreen'),('WB_BLUE','WhiteBalanceBlue')]:
    sensor(key,name,group='WhiteBalance',minimum=1,maximum=4095,kind='Float',scale=1024,unit='x',locked='WhiteBalanceLocked')
sensor('MIRROR','ReverseX',maximum=1,kind='Boolean',standard=True,locked='OrientationLocked')
sensor('FLIP','ReverseY',maximum=1,kind='Boolean',standard=True,locked='OrientationLocked')
sensor('TEST_PATTERN','TestPattern',group='TestControl',maximum=4,
       entries={'Off':0,'ColorBars':1,'Random':2,'Square':3,'Black':4},standard=True)
sensor('TEST_STYLE','SensorTestPatternStyle',group='TestControl',maximum=3)
for key,name in [('TEST_ROLL','SensorTestRolling'),('TEST_TRANSPARENT','SensorTestTransparent'),('TEST_BW','SensorTestMonochrome')]:
    sensor(key,name,group='TestControl',maximum=1,kind='Boolean')
for key,name,maximum in [('ISP0','SensorIspControl0',167),('ISP1','SensorIspControl1',167),('ISP3','SensorIspControl3',7)]:
    sensor(key,name,group='IspControl',maximum=maximum,representation='HexNumber',description='Masked sensor ISP control. Reserved bits are preserved.')
for key,name,maximum in [('FX_ENABLE','EffectEnableBits',255),('FX_COS','EffectHueCos',255),('FX_SIN','EffectHueSin',255),
                         ('FX_U','EffectU',255),('FX_V','EffectV',255),('FX_Y','EffectYOffset',255),
                         ('FX_CONTRAST','EffectContrast',255),('FX_BRIGHTNESS','EffectBrightness',255),
                         ('FX_SIGNS','EffectSigns',63),('FX_SATURATION','EffectManualSaturation',1),
                         ('FX_UV1','EffectUvThreshold1',255),('FX_UV2','EffectUvThreshold2',511)]:
    sensor(key,name,group='ImageEffects',maximum=maximum,locked=None,description='Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters.')
sensor('FX_APPLY','EffectApply',group='ImageEffects',access='WO',maximum=1,kind='Command')
for key,name,maximum in [('GAMMA_INDEX','GammaPointSelector',15),('GAMMA_VALUE','GammaPointValue',255),('GAMMA_CONTROL','GammaControl',3),
                         ('LENC_INDEX','LensCorrectionSelector',61),('LENC_VALUE','LensCorrectionValue',255),('LENC_ENABLE','LensCorrectionEnable',1)]:
    sensor(key,name,group='ImageCalibration',maximum=maximum,locked=None,description='Staged table setting; execute the corresponding Apply command while stopped.')
sensor('GAMMA_APPLY','GammaApply',group='ImageCalibration',access='WO',maximum=1,kind='Command')
sensor('LENC_APPLY','LensCorrectionApply',group='ImageCalibration',access='WO',maximum=1,kind='Command')
for key,name,maximum in [('DRAW_LEFT','OverlayLeft',4095),('DRAW_RIGHT','OverlayRight',4095),('DRAW_TOP','OverlayTop',2047),
                         ('DRAW_BOTTOM','OverlayBottom',2047),('DRAW_U','OverlayU',255),('DRAW_V','OverlayV',255),('DRAW_ENABLE','OverlayEnable',1)]:
    sensor(key,name,group='SensorOverlay',maximum=maximum,locked=None)
sensor('DRAW_APPLY','OverlayApply',group='SensorOverlay',access='WO',maximum=1,kind='Command')
add('VCM_AVAILABLE','SensorVcmAvailable','SensorActuator',access='RO',maximum=1,kind='Boolean',description='Per-unit capability. Enable only for a module with a wired VCM actuator.')
for key,name,maximum in [('VCM_TARGET','VcmTarget',1023),('VCM_SLEW','VcmSlew',15),('VCM_DIVIDER','VcmDivider',4095),
                         ('VCM_CURRENT','VcmCurrent',7),('VCM_RIH','VcmRih',1),('VCM_POWERDOWN','VcmPowerDown',1)]:
    sensor(key,name,group='SensorActuator',maximum=maximum,available='SensorVcmAvailable',locked=None)
sensor('VCM_APPLY','VcmApply',group='SensorActuator',access='WO',maximum=1,kind='Command',available='SensorVcmAvailable')
sensor('REG_SELECTOR','SensorRegisterSelector',group='SensorRegisters',maximum=65535,kind='RegisterSelector',locked=None,description='Named register catalogue from Ov5640Registers.h. Register values require stopped acquisition.')
sensor('REG_VALUE','SensorRegisterValue',group='SensorRegisters',access='RO',available='SensorIdle',maximum=255,representation='HexNumber')
sensor('PATCH_MASK','SensorFeatureMask',group='SensorRegisters',maximum=255,locked=None,representation='HexNumber')
sensor('PATCH_VALUE','SensorFeatureValue',group='SensorRegisters',maximum=255,locked=None,representation='HexNumber')
sensor('PATCH_APPLY','SensorFeatureApply',group='SensorRegisters',access='WO',maximum=1,kind='Command',description='Stopped-only masked write to supported ISP/analog image-control ranges. Clock/reset/format/OTP/MCU registers are write-protected.')
sensor('OTP_INDEX','SensorOtpSelector',group='SensorIdentity',maximum=31,locked=None)
sensor('OTP_VALUE','SensorOtpValue',group='SensorIdentity',access='RO',maximum=255,available='SensorOtpValid')
sensor('OTP_READ','SensorOtpRefresh',group='SensorIdentity',access='WO',maximum=1,kind='Command',description='Refresh the 32-byte OTP shadow. Read only; never programs OTP.')
for key,name in [('JPEG_AVAILABLE','SensorJpegAvailable'),('FREX_AVAILABLE','SensorFrexAvailable'),('MCU_AVAILABLE','SensorMcuFirmwareAvailable')]:
    add(key,name,'SensorCapabilities',access='RO',maximum=1,kind='Boolean',description=('JPEG transport is implemented in the matching CSI-byte production image; select VGA YUYV and ImageCompressionMode=JPEG.' if key=='JPEG_AVAILABLE' else 'Unavailable until the corresponding board/transport integration is qualified.'))
add('OTP_VALID','SensorOtpValid','SensorIdentity',access='RO',maximum=1,kind='Boolean',description='The OTP read shadow becomes valid after SensorOtpRefresh succeeds.')

add("EXPOSURE_MAX_LINES","ExposureMaximumLines","SensorTiming",access="RO",minimum=1,maximum=65531)

# Append only: existing register IDs remain stable.
sensor('FRAME_RATE_ENABLE','AcquisitionFrameRateEnable',group='SensorTiming',maximum=1,kind='Boolean',standard=True)
add('FRAME_RATE_MIN','AcquisitionFrameRateMinimum','SensorTiming',access='RO',minimum=1,maximum=30000,kind='Float',scale=1000,unit='Hz')
add('FRAME_RATE_MAX','AcquisitionFrameRateMaximum','SensorTiming',access='RO',minimum=1,maximum=30000,kind='Float',scale=1000,unit='Hz')
add('RESULTING_FPS','ResultingFrameRate','SensorTiming',access='RO',maximum=30000,kind='Float',scale=1000,unit='Hz',description='Nominal sensor rate from current HTS/VTS, not network delivery FPS.')
sensor('AE_TARGET','AutoExposureTarget',minimum=8,maximum=240,description='Average-luminance target in sensor units; updates stable and fast AEC thresholds.')
for key,name in [('SHARPNESS','SensorSharpness'),('DENOISE','SensorDenoise')]:
    sensor(key,name,group='IspControl',maximum=255,description='Manual CIP strength in sensor register units. Has no effect on RAW output.')
for key,name in [('SHARPNESS_AUTO','SensorSharpnessAuto'),('DENOISE_AUTO','SensorDenoiseAuto')]:
    sensor(key,name,group='IspControl',maximum=2,entries={'Off':0,'Continuous':2},description='Selects automatic CIP threshold; manual strength takes effect when Off.')
add('LUMINANCE','SensorAverageLuminance','Diagnostics',access='RO',maximum=255,description='Sensor AEC weighted luminance readout; refreshed with live telemetry.')

add('USERSET_AVAILABLE','UserSetStorageAvailable','UserSetControl',access='RO',maximum=1,kind='Boolean')
add('USERSET_BUSY','UserSetBusy','UserSetControl',access='RO',maximum=1,kind='Boolean')
add('USERSET_ERROR','UserSetLastError','UserSetControl',access='RO',minimum=-1000,maximum=0,signed=True)
add('USERSET_SELECTOR','UserSetSelector','UserSetControl',maximum=7,entries={'Default':0,**{'UserSet'+str(i):i for i in range(1,8)}},standard=True,locked='ConfigurationLocked')
add('USERSET_LOAD','UserSetLoad','UserSetControl',access='RW',maximum=1,kind='Command',standard=True,locked='UserSetLoadLocked')
add('USERSET_SAVE','UserSetSave','UserSetControl',access='RW',maximum=1,kind='Command',standard=True,locked='UserSetSaveLocked',description='Save UserSet1..7. Default is read-only through this standard command; factory provisioning uses the password-protected FactoryUserSetSave command.')
add('USERSET_DEFAULT','UserSetDefault','UserSetControl',maximum=7,entries={'Default':0,**{'UserSet'+str(i):i for i in range(1,8)}},standard=True,locked='ConfigurationLocked')
add('USERSET_VALID','UserSetValid','UserSetControl',access='RO',maximum=1,kind='Boolean')
add('USERSET_MASK','UserSetSavedMask','UserSetControl',access='RO',maximum=255,representation='HexNumber')
add('FACTORY_UNLOCKED','FactoryAccessUnlocked','UserSetControl',access='RO',maximum=1,kind='Boolean')
add('FACTORY_LOCK','FeatureAccessLock','UserSetControl',access='WO',maximum=1,kind='Command')

add('NATIVE_FORMATS','NativePixelFormatsAvailable','Diagnostics',access='RO',maximum=1,kind='Boolean')
add('WIDTH_INC','WidthIncrement','SensorTiming',access='RO',minimum=2,maximum=16)
add('RAW_ACTIVE','RawPixelFormatActive','Diagnostics',access='RO',maximum=1,kind='Boolean')

sensor('COMPRESSION','ImageCompressionMode',group='SensorCapabilities',maximum=1,entries={'Off':0,'JPEG':1},standard=True,locked='CompressionConfigurationLocked',available='NativePixelFormatsAvailableRegister',description='JPEG requires VGA YUYV. Select 640x480 YUV422_YUYVPacked first. GVSP payload type JPEG; PayloadSize is maximum allocation, each leader carries actual encoded length.')
sensor('JPEG_QUALITY','SensorJpegQuality',group='SensorCapabilities',maximum=63,available='NativePixelFormatsAvailableRegister',description='OV5640 quantization scale, smaller values give higher quality/larger encoded frames.')

add('WIDTH_MAX','WidthMaximum','SensorTiming',access='RO',minimum=160,maximum=2592)
add('HEIGHT_MAX','HeightMaximum','SensorTiming',access='RO',minimum=120,maximum=1944)

# Industrial transport/device additions; append to preserve existing IDs.
for key,name in [('RESEND_REQUESTS','PacketResendRequests'),('RESENT_PACKETS','RetransmittedPackets'),('RESEND_MISSING','PacketResendUnavailable'),('RESEND_QUEUE_FULL','PacketResendQueueOverflows'),('RECOVERIES','CameraRecoveryCount')]:
    add(key,name,'Diagnostics',access='RO',maximum=4294967295)
add('CONFIG_PENDING','DeviceConfigurationPending','NetworkControl',access='RO',maximum=1,kind='Boolean',description='Network/identity changes are journaled automatically while stopped. IP mode and identity apply on the next firmware boot.')
add('CONFIG_ERROR','DeviceConfigurationLastError','NetworkControl',access='RO',minimum=-1000,maximum=0,signed=True)
add('DEVICE_PROVISIONED','DeviceIdentityProvisioned','NetworkControl',access='RO',maximum=1,kind='Boolean')
add('RECOVER','DeviceReset','DeviceControl',access='WO',maximum=1,kind='Command',standard=True,description='Asynchronously return to the power-up configuration: reset sensor/video and protocol, load UserSetDefault, and apply saved network/identity. Acquisition and ownership are released. Rediscover/reconnect after completion.')
add('RECOVERY_BUSY','DeviceResetBusy','DeviceMaintenance',access='RO',maximum=1,kind='Boolean')
add('RECOVERY_ERROR','DeviceResetLastError','DeviceMaintenance',access='RO',minimum=-1000,maximum=0,signed=True)


add('TELEMETRY_DEFERRED','SensorTelemetryDeferred','Diagnostics',access='RO',maximum=1,kind='Boolean',description='Streaming defers SCCB telemetry above 105 MB/s or while software-converting images larger than 640x480 to avoid stranding sensor transactions during main-loop stalls. Cached values refresh during supported active acquisition or synchronous sensor operations; stopped background reads are disabled.')

add('FACTORY_SAVE','FactoryUserSetSave','UserSetControl',access='RW',maximum=1,kind='Command',locked='FactoryUserSetSaveLocked',description='Factory maintenance only: save the current profile into the factory slot. Requires UserSetSelector=Default and an unlocked factory password. Not part of standard UserSetSave.')

# Append only. PL controls are separate from the OV5640 ISP controls.
add('FPGA_VERSION','FpgaProcessingVersion','FpgaImageProcessing',access='RO',maximum=4294967295)
add('FPGA_CAPABILITIES','FpgaProcessingCapabilities','FpgaImageProcessing',access='RO',maximum=4294967295,representation='HexNumber',description='Integrated PL capabilities: bit 0 black-level subtraction, bit 1 Bayer bad-pixel correction, bit 2 manual gains, bit 3 RGB demosaic, bit 4 RGB median, bit 5 ROI/area downscale, bit 6 CCM, bit 7 gamma, bit 8 FPGA AWB/statistics, bit 9 bilateral filtering, bit 10 sharpening, bit 11 RAW lens shading, bit 12 focus/motion analysis, bit 13 RGB-to-YUV output, bit 14 Xilinx polyphase resampling, bit 15 local contrast with shared RAW preprocessing, bit 16 adaptive RGB display tone mapping, bit 17 bounded integer-map remapping, bit 18 four-byte-per-clock framebuffer writer. Capability bits describe the installed FPGA image.')
add('FPGA_AVAILABLE','FpgaProcessingAvailable','FpgaImageProcessing',access='RO',maximum=1,kind='Boolean')
for key,name in [('FPGA_ENABLE','FpgaProcessingEnable'),('FPGA_BPC','FpgaBadPixelCorrectionEnable')]:
    add(key,name,'FpgaImageProcessing',maximum=1,kind='Boolean',available='FpgaProcessingAvailableRegister',locked='ConfigurationLocked',description='Applied at the next AcquisitionStart while stopped. Processing supports BayerBG10/BayerBG10p and, when color capability is present, RGB8/BGR8. Other formats use byte bypass.')
add('FPGA_BLACK','FpgaBlackLevel','FpgaImageProcessing',maximum=1023,unit='DN',available='FpgaProcessingAvailableRegister',locked='FpgaBlackLevelLocked',description='10-bit RAW black offset before BPC and gains. On audited v9/v10 active RAW processing, a live write applies to a subsequent complete HLS frame without restarting acquisition. Readback is the requested setting, not a frame-tagged applied value. Otherwise configure while stopped; applies at next AcquisitionStart.')
for key,name in [('FPGA_RED','FpgaBalanceRed'),('FPGA_GREEN','FpgaBalanceGreen'),('FPGA_BLUE','FpgaBalanceBlue')]:
    add(key,name,'FpgaImageProcessing',maximum=4096,kind='Float',scale=256,unit='x',available='FpgaProcessingAvailableRegister',locked='ConfigurationLocked',description='Q8.8 Bayer gain, unity 1.0. Applied after BPC, saturated to 10 bits. Applies at next AcquisitionStart.')
add('FPGA_ACTIVE','FpgaProcessingActive','FpgaImageProcessing',access='RO',maximum=1,kind='Boolean',description='True during acquisition when a supported RAW10 or RGB processing path is selected.')
for key,name in [('FPGA_MEDIAN_AVAILABLE','FpgaMedianAvailable'),('FPGA_SCALER_AVAILABLE','FpgaScalerAvailable')]:
    add(key,name,'FpgaImageProcessing',access='RO',maximum=1,kind='Boolean',description='Reports whether the installed FPGA image includes this RGB processing block.')

# Append-only color-pipeline controls preserve all existing virtual addresses.
for key,name,available in [
    ('FPGA_MEDIAN','FpgaMedianEnable','FpgaMedianAvailableRegister'),
    ('FPGA_SCALER','FpgaScalerEnable','FpgaScalerAvailableRegister'),
    ('FPGA_ROI_ENABLE','FpgaRoiEnable','FpgaScalerAvailableRegister')]:
    add(key,name,'FpgaImageProcessing',maximum=1,kind='Boolean',available=available,
        locked='ConfigurationLocked',description='Applied at the next AcquisitionStart. Requires FPGA processing and RGB8/BGR8 or supported FPGA YUV output.')
for key,name,minimum,maximum,increment in [
    ('FPGA_INPUT_WIDTH','FpgaInputWidth',16,2592,16),
    ('FPGA_INPUT_HEIGHT','FpgaInputHeight',8,1944,2),
    ('FPGA_ROI_X','FpgaRoiOffsetX',0,2591,1),
    ('FPGA_ROI_Y','FpgaRoiOffsetY',0,1943,1),
    ('FPGA_ROI_WIDTH','FpgaRoiWidth',1,2592,1),
    ('FPGA_ROI_HEIGHT','FpgaRoiHeight',1,1944,1)]:
    add(key,name,'FpgaImageProcessing',minimum=minimum,maximum=maximum,increment=increment,
        available='FpgaScalerAvailableRegister',locked='ConfigurationLocked',unit='px',
        description='Input and ROI dimensions for FPGA color resampling. Width/Height select output size. ROI must fit the RAW input. Area mode only reduces; Polyphase permits up to 4x enlargement per ROI axis within the 2592x1944 output bounds. Mixed up/down axes are supported. Validated at AcquisitionStart.')

add('SENSOR_RAW_ACTIVE','SensorRawActive','Diagnostics',access='RO',maximum=1,kind='Boolean',description='The sensor uses its Bayer RAW tap, including FPGA RGB conversion. Sensor AWB and orientation are unavailable in this mode.')

# Color v3: selectors affect table access only and are not image settings.
add('FPGA_COLOR_AVAILABLE','FpgaColorAdjustmentAvailable','FpgaImageProcessing',access='RO',maximum=1,kind='Boolean')
for key,name in [('FPGA_CCM','FpgaColorTransformationEnable'),('FPGA_GAMMA','FpgaGammaEnable')]:
    add(key,name,'FpgaImageProcessing',maximum=1,kind='Boolean',available='FpgaColorAdjustmentAvailableRegister',locked='ConfigurationLocked')
add('FPGA_AWB','FpgaBalanceWhiteAuto','FpgaImageProcessing',maximum=2,entries={'Off':0,'Continuous':2},available='FpgaColorAdjustmentAvailableRegister',locked='ConfigurationLocked',description='FPGA gray-world AWB: previous-frame admitted samples, gains 0.25x to 4x, quarter-step smoothing, green reference unity. Dark/saturated frames hold gains. Restart resets AWB to unity.')
for key,name,maximum,selected in [
    ('FPGA_MATRIX_INDEX','FpgaColorTransformationSelector',8,'FpgaColorTransformationValue'),
    ('FPGA_OFFSET_CHANNEL','FpgaColorOffsetSelector',2,'FpgaColorOffsetValue'),
    ('FPGA_GAMMA_CHANNEL','FpgaGammaChannel',2,'FpgaGammaValue'),
    ('FPGA_GAMMA_INDEX','FpgaGammaIndex',255,'FpgaGammaValue')]:
    add(key,name,'FpgaImageProcessing',maximum=maximum,available='FpgaColorAdjustmentAvailableRegister',locked='ConfigurationLocked',selected=[selected],description='Stopped-only table selector. Matrix order is RGB rows then RGB columns. Channel order is red, green, blue.')
for key,name in [('FPGA_MATRIX_VALUE','FpgaColorTransformationValue'),('FPGA_OFFSET_VALUE','FpgaColorOffsetValue')]:
    add(key,name,'FpgaImageProcessing',minimum=-8388608,maximum=8388607,signed=True,kind='Float',scale=1048576,available='FpgaColorAdjustmentAvailableRegister',locked='ConfigurationLocked',description='Signed Q20 coefficient or DN offset in [-8,8). Applied before RGB8 conversion; vendor Q8 product intermediates and truncation apply.')
add('FPGA_GAMMA_VALUE','FpgaGammaValue','FpgaImageProcessing',maximum=255,available='FpgaColorAdjustmentAvailableRegister',locked='ConfigurationLocked',description='Selected channel/index RGB8 LUT value. Default is identity. All 768 LUT bytes persist in UserSet.')
add('FPGA_STATS_LATCH','FpgaStatisticsLatch','FpgaImageProcessing',access='WO',maximum=1,kind='Command',available='FpgaColorAdjustmentAvailableRegister',description='Snapshot the latest complete FPGA frame statistics. Allowed during acquisition; read fields remain stable until the next latch.')
for key,name,maximum in [('FPGA_STATS_SEQUENCE','FpgaStatisticsSequence',4294967295),('FPGA_STATS_VALID','FpgaStatisticsValidPixels',5038848),('FPGA_STATS_CLIPPED','FpgaClippedPixels',5038848),('FPGA_STATS_DARK','FpgaDarkPixels',5038848)]:
    add(key,name,'FpgaImageProcessing',access='RO',maximum=maximum,available='FpgaColorAdjustmentAvailableRegister',description='Latched complete-frame statistics. Sequence restarts at acquisition reset and is not the GVSP FrameID.')
for key,name in [('FPGA_MEAN_RED','FpgaBalanceMeanRed'),('FPGA_MEAN_GREEN','FpgaBalanceMeanGreen'),('FPGA_MEAN_BLUE','FpgaBalanceMeanBlue'),('FPGA_MEAN_INTENSITY','FpgaIntensityMean')]:
    add(key,name,'FpgaImageProcessing',access='RO',maximum=67043328,available='FpgaColorAdjustmentAvailableRegister',kind='Float',scale=65536,unit='DN',description='Latched Q16 mean. Color means precede gains/CCM and exclude samples with any channel below 8 or at least 1000 DN. Intensity is output RGB8 luma.')
for key,name in [('FPGA_APPLIED_RED','FpgaAppliedBalanceRed'),('FPGA_APPLIED_GREEN','FpgaAppliedBalanceGreen'),('FPGA_APPLIED_BLUE','FpgaAppliedBalanceBlue')]:
    add(key,name,'FpgaImageProcessing',access='RO',maximum=4096,available='FpgaColorAdjustmentAvailableRegister',kind='Float',scale=256,unit='x',description='Latched gains actually applied to the completed frame, independent of configured manual gains during AWB.')

# Color v4: supported only by the combined detail-processing FPGA image.
add('FPGA_DETAIL_AVAILABLE','FpgaDetailProcessingAvailable','FpgaImageProcessing',access='RO',maximum=1,kind='Boolean')
add('FPGA_BILATERAL','FpgaBilateralEnable','FpgaImageProcessing',maximum=1,kind='Boolean',available='FpgaDetailProcessingAvailableRegister',locked='ConfigurationLocked',description='AMD 3x3 RGB bilateral filter after gamma/median and before ROI/scaling. Replicate border; disabled by default.')
add('FPGA_SIGMA_COLOR','FpgaBilateralSigmaColor','FpgaImageProcessing',minimum=1,maximum=255,available='FpgaDetailProcessingAvailableRegister',locked='ConfigurationLocked',unit='DN',description='RGB bilateral range sigma; default 24 DN.')
add('FPGA_SIGMA_SPACE','FpgaBilateralSigmaSpace','FpgaImageProcessing',minimum=64,maximum=1024,kind='Float',scale=256,available='FpgaDetailProcessingAvailableRegister',locked='ConfigurationLocked',unit='px',description='RGB bilateral spatial sigma; Q8 value, default 1 pixel.')
add('FPGA_SHARPEN','FpgaSharpenStrength','FpgaImageProcessing',maximum=256,kind='Float',scale=256,available='FpgaDetailProcessingAvailableRegister',locked='ConfigurationLocked',description='RGB cross-Laplacian strength [0,1] using AMD filter2D, saturated per channel. Zero bypasses. Constant-zero filter border can brighten image edges; assess halos and noise amplification.')

# Color/RAW v5: unity calibration tables are neutral, not optical calibration.
add('FPGA_LSC_AVAILABLE','FpgaLensShadingAvailable','FpgaImageProcessing',access='RO',maximum=1,kind='Boolean')
add('FPGA_LSC','FpgaLensShadingEnable','FpgaImageProcessing',maximum=1,kind='Boolean',available='FpgaLensShadingAvailableRegister',locked='ConfigurationLocked',description='Programmable RAW10 lens shading after black/BPC and before demosaic or RAW manual gains. Four-phase 7x9 bilinear Q12 grid; default disabled with unity table. A real uniform-field calibration is required for optical correction.')
add('FPGA_LSC_PHASE','FpgaLensShadingPhase','FpgaImageProcessing',maximum=3,entries={'Blue':0,'GreenBlueRow':1,'GreenRedRow':2,'Red':3},available='FpgaLensShadingAvailableRegister',locked='ConfigurationLocked',selected=['FpgaLensShadingGain'],description='BGGR calibration plane; the two green CFA phases have independent tables.')
for key,name,maximum in [('FPGA_LSC_ROW','FpgaLensShadingRow',6),('FPGA_LSC_COLUMN','FpgaLensShadingColumn',8)]:
    add(key,name,'FpgaImageProcessing',maximum=maximum,available='FpgaLensShadingAvailableRegister',locked='ConfigurationLocked',selected=['FpgaLensShadingGain'])
add('FPGA_LSC_GAIN','FpgaLensShadingGain','FpgaImageProcessing',maximum=16384,kind='Float',scale=4096,unit='x',available='FpgaLensShadingAvailableRegister',locked='ConfigurationLocked',description='Selected grid gain in [0,4], Q12 rounded interpolation and saturated RAW10 output. Default 1x. All 252 gains persist in UserSet; selectors do not.')

# Scene v6 results share the complete-frame latch and sequence with color stats.
add('FPGA_SCENE_AVAILABLE','FpgaSceneAnalysisAvailable','FpgaImageProcessing',access='RO',maximum=1,kind='Boolean')
add('FPGA_SCENE','FpgaSceneAnalysisEnable','FpgaImageProcessing',maximum=1,kind='Boolean',available='FpgaSceneAnalysisAvailableRegister',locked='ConfigurationLocked',description='Post-ROI RGB luma focus/motion diagnostics. Both output dimensions must be at least 32 for analysis; smaller images pass with zero/invalid metrics. First frame after acquisition restart has invalid motion. Illumination changes can trigger motion, and noise can increase focus.')
for key,name,maximum,description in [('FPGA_FOCUS_MEAN','FpgaFocusMean',66846720,'Mean absolute four-neighbor luma Laplacian over the output interior, Q16 DN. Sensitive to ISP settings and noise.'),('FPGA_MOTION_MEAN','FpgaMotionMean',16711680,'Mean absolute difference of current/previous 32x32 luma-bin means, Q16 DN. First frame is invalid.')]:
    add(key,name,'FpgaImageProcessing',access='RO',maximum=maximum,kind='Float',scale=65536,unit='DN',available='FpgaSceneAnalysisAvailableRegister',description=description+' Latched with the common completed-frame statistics sequence.')
add('FPGA_MOTION_VALID','FpgaMotionValid','FpgaImageProcessing',access='RO',maximum=1,kind='Boolean',available='FpgaSceneAnalysisAvailableRegister',description='Latched valid preceding-frame comparison. False when analysis is disabled, dimensions are below 32, or history was reset.')

# Output v7: PixelFormat selects packing; level range is independently controlled.
add('FPGA_OUTPUT_AVAILABLE','FpgaYuvOutputAvailable','FpgaImageProcessing',access='RO',maximum=1,kind='Boolean')
add('FPGA_YUV_RANGE','FpgaYuvRange','FpgaImageProcessing',maximum=1,entries={'Limited':0,'Full':1},available='FpgaYuvOutputAvailableRegister',locked='ConfigurationLocked',description='BT.601 output levels for the FPGA RGB-to-YUV path. PixelFormat selects YUV444, four averaged-chroma YUV422 orders or legacy 2x2-chroma YUV420. Requires FPGA processing enabled and RAW10 sensor acquisition; native sensor YUV bypass retains its own timing and levels. RGB/BGR bypass is unchanged. Default Limited.')

# Output v8: opt-in six-tap Xilinx VPSS with bounded area prefiltering.
add('FPGA_POLYPHASE_AVAILABLE','FpgaPolyphaseAvailable','FpgaImageProcessing',access='RO',maximum=1,kind='Boolean')
add('FPGA_INTERPOLATION','FpgaScalerInterpolation','FpgaImageProcessing',maximum=1,entries={'Area':0,'Polyphase':1},available='FpgaPolyphaseAvailableRegister',locked='ConfigurationLocked',description='Resample RGB/BGR or FPGA YUV using Area (default, downscale only) or six-tap, 64-phase Xilinx VPSS. Polyphase allows up to 4x enlargement per ROI axis, within 2592x1944 output and PixelFormat alignment limits. Larger downscale ratios use an area prefilter. Mixed up/down axes are supported. Equal geometry bypasses VPSS; an unchanged axis uses identity coefficients. Configure while stopped; output pixel rate and Ethernet payload can limit FPS.')

# V9 adds previous-frame Xilinx CLAHE after scaling and before final statistics.
add('FPGA_CLAHE_AVAILABLE','FpgaLocalContrastAvailable','FpgaImageProcessing',access='RO',maximum=1,kind='Boolean')
add('FPGA_CLAHE','FpgaLocalContrastEnable','FpgaImageProcessing',maximum=1,kind='Boolean',available='FpgaLocalContrastAvailableRegister',locked='ConfigurationLocked',description='Four-by-four-tile Xilinx CLAHE on RGB value, after optional scaling. Preserves RGB ratios with bounded fixed-point rounding; black remains black. Requires color FPGA processing, output dimensions at least 32, height multiple of 4 and width multiple of 8. Uses the preceding frame histogram; first frame after Start, geometry/clip change or reset bypasses. Can amplify noise or react to illumination changes. Default off; no optical calibration is implied.')
add('FPGA_CLAHE_CLIP','FpgaLocalContrastClipLimit','FpgaImageProcessing',minimum=1,maximum=8,available='FpgaLocalContrastAvailableRegister',locked='ConfigurationLocked',description='Integer Xilinx CLAHE histogram clip multiplier, 1 through 8, default 4. Higher values allow stronger contrast/noise amplification. Configure while stopped.')

# Append-only ABI: live eligibility includes health, route and drain/recovery state.
add('FPGA_BLACK_LIVE_ACTIVE','FpgaBlackLevelLiveActive','FpgaImageProcessing',access='RO',maximum=1,kind='Boolean',description='True only while healthy audited v9/v10 FPGA RAW processing accepts frame-latched live Black Level writes. False during stop/drain/recovery, bypass and legacy processing. Other controls retain their existing locks.')

# Firmware exposes the bounded existing-VPSS upscale path without changing old addresses.
add('FPGA_UPSCALE_AVAILABLE','FpgaUpscaleAvailable','FpgaImageProcessing',access='RO',maximum=1,kind='Boolean',description='Firmware supports bounded enlargement using the installed VPSS (processing v8 or later). Requires processing/scaler enabled and Polyphase selected. No additional FPGA resources; camera FPS acceptance is separate.')
add('FPGA_UPSCALE_MAXIMUM','FpgaUpscaleMaximum','FpgaImageProcessing',access='RO',minimum=1,maximum=4,unit='x',description='Maximum supported enlargement per ROI axis: 4 with VPSS, otherwise 1 (no enlargement). Output remains at most 2592x1944 and subject to PixelFormat alignment and link bandwidth. Area never enlarges.')

# V11 adds an opt-in previous-input-frame RGB display curve after CLAHE.
add('FPGA_TONE_AVAILABLE','FpgaAdaptiveToneAvailable','FpgaImageProcessing',access='RO',maximum=1,kind='Boolean',description='Matching v11 image includes the adaptive RGB display curve after CLAHE and before final scene statistics/output packing. This is not HDR radiance reconstruction.')
add('FPGA_ADAPTIVE_TONE','FpgaAdaptiveToneEnable','FpgaImageProcessing',maximum=1,kind='Boolean',available='FpgaAdaptiveToneAvailableRegister',locked='ConfigurationLocked',description='Opt-in shared per-channel rational RGB8 curve driven by preceding input-frame weighted luma. Black/white endpoints fixed; gain 0.5x to 2x changes by at most 1/16 per frame. Can change color ratios and temporal brightness. First/start/reset/dimension/target-change frame bypasses. Configure while stopped. Default off; requires color FPGA processing.')
add('FPGA_TONE_TARGET','FpgaAdaptiveToneTarget','FpgaImageProcessing',minimum=16,maximum=224,unit='DN',available='FpgaAdaptiveToneAvailableRegister',locked='ConfigurationLocked',description='Desired preceding-input weighted-luma mean, (R+2G+B)/4, default 128. Target is a bounded adaptation reference, not guaranteed output brightness. Configure while stopped; new acquisition resets history.')
add('FPGA_TONE_APPLIED_GAIN','FpgaAdaptiveToneAppliedGain','FpgaImageProcessing',access='RO',minimum=128,maximum=512,kind='Float',scale=256,unit='x',available='FpgaAdaptiveToneAvailableRegister',description='Unlatched current HLS invocation gain; unity before first valid invocation. Not associated with a specific GVSP frame or common statistics latch.')
add('FPGA_TONE_PREVIOUS_MEAN','FpgaAdaptiveTonePreviousMean','FpgaImageProcessing',access='RO',maximum=255,unit='DN',available='FpgaAdaptiveToneAvailableRegister',description='Unlatched preceding input-frame weighted-luma mean used by the current HLS invocation. First/reset invocation reports zero. Not frame-tagged; gain/mean reads are not an atomic pair.')

# Append-only vendor ABI for volatile precomputed integer-map correction.
add('REMAP_AVAILABLE','FpgaDistortionAvailable','FpgaImageProcessing',access='RO',maximum=1,kind='Boolean',description='Matching image/control support for bounded640x480 nearest correction, +/-7 source rows. Does not imply calibrated lens parameters.')
add('REMAP_ENABLE','FpgaDistortionEnable','FpgaImageProcessing',maximum=1,kind='Boolean',available='FpgaDistortionAvailableRegister',locked='ConfigurationLocked',description='Volatile correction enable. Requires a completely validated map matching current color geometry. Default off; full-resolution bypass remains supported. Not saved in UserSet.')
for key,name,maximum,default in [('REMAP_WIDTH','FpgaDistortionMapWidth',640,640),('REMAP_HEIGHT','FpgaDistortionMapHeight',480,480)]:
    add(key,name,'FpgaImageProcessing',minimum=1,maximum=maximum,available='FpgaDistortionAvailableRegister',locked='ConfigurationLocked',description='Staged map dimension. Set only after Abort, before Begin, with correction disabled. Must match output geometry; no implicit scaling.')
add('REMAP_EXPECTED_CRC','FpgaDistortionExpectedCrc32','FpgaImageProcessing',maximum=4294967295,representation='HexNumber',available='FpgaDistortionAvailableRegister',locked='ConfigurationLocked',description='CRC32 of raster little-endian32-bit map words, polynomial0xedb88320, standard initial/final XOR. Set before Begin; integrity check is not optical calibration.')
for key,name in [('REMAP_BEGIN','FpgaDistortionMapBegin'),('REMAP_COMMIT','FpgaDistortionMapCommit'),('REMAP_ABORT','FpgaDistortionMapAbort')]:
    add(key,name,'FpgaImageProcessing',access='WO',maximum=1,kind='Command',available='FpgaDistortionAvailableRegister',locked='ConfigurationLocked',description='Volatile map transfer command. Begin selects a staging bank; Commit starts bounded asynchronous validation; Abort invalidates staging without releasing a bank the reader may still access. Configure while stopped and disabled.')
add('REMAP_INDEX','FpgaDistortionMapIndex','FpgaImageProcessing',maximum=307199,available='FpgaDistortionAvailableRegister',locked='ConfigurationLocked',description='Raster map word index. Upload sequentially; explicit index/value pairs make identical retries idempotent.')
add('REMAP_DATA','FpgaDistortionMapData','FpgaImageProcessing',maximum=4294967295,representation='HexNumber',available='FpgaDistortionAvailableRegister',locked='ConfigurationLocked',description='Map word: valid bit31, source x bits9:0, source y bits18:10, reserved bits30:19 zero. Zero means black. Coordinates must be in bounds and within seven source rows; unsupported entries fail validation.')
add('REMAP_STATE','FpgaDistortionMapState','FpgaImageProcessing',access='RO',maximum=4,entries={'Empty':0,'Uploading':1,'Validating':2,'Ready':3,'Error':4},available='FpgaDistortionAvailableRegister')
for key,name in [('REMAP_UPLOADED','FpgaDistortionWordsUploaded'),('REMAP_CHECKED','FpgaDistortionWordsValidated')]:
    add(key,name,'FpgaImageProcessing',access='RO',maximum=307200,available='FpgaDistortionAvailableRegister')
add('REMAP_ERROR','FpgaDistortionMapError','FpgaImageProcessing',access='RO',maximum=2,entries={'None':0,'InvalidCoordinate':1,'ChecksumMismatch':2},available='FpgaDistortionAvailableRegister')
add('REMAP_HARDWARE_STATUS','FpgaDistortionHardwareStatus','FpgaImageProcessing',access='RO',maximum=7,available='FpgaDistortionAvailableRegister',description='Last observed reader status: bit0 geometry, bit1 map encoding/window, bit2 input framing. Zero alone does not prove successful acquisition.')


# Append-only experimental A53 analytics controls; bitmask order is documented.
add('AI_AVAILABLE','MaxAIAvailable','MaxAI',access='RO',maximum=1,kind='Boolean')
add('AI_SUPPORTED','MaxAISupportedFeatures','MaxAI',access='RO',maximum=255)
add('AI_ENABLE','MaxAIEnabledFeatures','MaxAI',maximum=255,description='Volatile bitmask: face detection, face recognition, objects, plate candidates/OCR, motion, person HOG, QR, tracking. Unsupported bits are rejected. Defaults off. Results use the captured output image coordinates; no accuracy or real-time guarantee.')
add('AI_STATE','MaxAIWorkerState','MaxAI',access='RO',maximum=3,entries={'Unavailable':0,'Idle':1,'Ready':2,'Copying':3})
for key,name in [('ERROR','ErrorCode'),('CONFIG','ConfigGeneration'),('LATENCY','InferenceTimeUs')]:
    add('AI_'+key,'MaxAI'+name,'MaxAI',access='RO',maximum=4294967295)
add('AI_SCORE','MaxAIMinimumScore','MaxAI',minimum=1,maximum=1000000,description='Score threshold in millionths. Model confidence is not calibrated probability.')
add('AI_PERIOD','MaxAISamplePeriodUs','MaxAI',minimum=100000,maximum=60000000,unit='us')
add('AI_FACE_THRESHOLD','MaxAIFaceCosineThreshold','MaxAI',minimum=1,maximum=1000000,description='Cosine similarity threshold in millionths; default0.45 is a starting point requiring validation on your enrollment data.')
for key,name in [('LATCH','ResultLatch'),('RELEASE','ResultRelease')]:
    add('AI_'+key,'MaxAI'+name,'MaxAI',access='WO',maximum=1,kind='Command')
add('AI_VALID','MaxAIResultValid','MaxAI',access='RO',maximum=1,kind='Boolean')
add('AI_COUNT','MaxAIObjectCount','MaxAI',access='RO',maximum=32)
for key,name in [('SEQUENCE','ResultSequence'),('CAPTURE','CaptureSequence'),('TIMESTAMP_LOW','TimestampLow'),('TIMESTAMP_HIGH','TimestampHigh')]:
    add('AI_'+key,'MaxAI'+name,'MaxAI',access='RO',maximum=4294967295)
add('AI_SELECTOR','MaxAIObjectSelector','MaxAI',maximum=31)
for key,name,maximum in [('OBJECT_FEATURE','ObjectFeature',7),('OBJECT_CLASS','ObjectClass',4294967295),('OBJECT_SCORE','ObjectScore',1000000),('OBJECT_X','ObjectX',2592),('OBJECT_Y','ObjectY',1944),('OBJECT_WIDTH','ObjectWidth',2592),('OBJECT_HEIGHT','ObjectHeight',1944),('OBJECT_TRACK','ObjectTrackID',4294967295)]:
    add('AI_'+key,'MaxAI'+name,'MaxAI',access='RO',maximum=maximum)
for i in range(8):
    add('AI_TEXT_'+str(i),'MaxAIObjectTextWord'+str(i),'MaxAI',access='RO',maximum=4294967295,representation='HexNumber',description='Four UTF-8 bytes packed least-significant-byte first. Eight words form a NUL-terminated32-byte label. Applies to identity, OCR or QR; labels may be truncated.')

for key,name,maximum in [('RESULT_WIDTH','ResultWidth',2592),('RESULT_HEIGHT','ResultHeight',1944),('RESULT_PFNC','ResultPixelFormat',4294967295),('RESULT_CONFIG','ResultConfigGeneration',4294967295),('RESULT_AGE','ResultAgeUs',4294967295)]:
    add('AI_'+key,'MaxAI'+name,'MaxAI',access='RO',maximum=maximum,description='Latched source geometry/configuration, or live elapsed microseconds since that capture was offered. Age saturates at32-bit maximum and is not wall-clock time.')
