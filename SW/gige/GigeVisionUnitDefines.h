#ifndef GIGE_VISION_UNIT_DEFINES_H
#define GIGE_VISION_UNIT_DEFINES_H
/* Per-unit provisioning; example locally administered MAC, NOT a fleet default. */
#define GIGE_UNIT_MAC_HIGH           0x00000200U
#define GIGE_UNIT_MAC_LOW            0x00000001U
#define GIGE_UNIT_SERIAL             "00000001" /* <=15 ASCII bytes */
#define GIGE_UNIT_USER_NAME          "Camera-01" /* <=15 ASCII bytes */
/* Numeric IPv4 values in host arithmetic; serialize big endian on wire. */
#define GIGE_UNIT_PERSISTENT_IP      0xC0A80A02U /* 192.168.10.2 */
#define GIGE_UNIT_PERSISTENT_MASK    0xFFFFFF00U
#define GIGE_UNIT_PERSISTENT_GATEWAY 0x00000000U
#define GIGE_UNIT_IP_SUPPORTED       0x00000007U /* persistent, DHCP, link-local */
#define GIGE_UNIT_IP_CONFIG          0x00000006U /* DHCP with link-local fallback */
#define GIGE_UNIT_HEARTBEAT_MS       3000U
#define GIGE_UNIT_TIMESTAMP_HZ       1000000U /* requires actual 1us clock adapter */
#define GIGE_UNIT_PACKET_SIZE        1500U /* complete IPv4 packet, without Ethernet */
#if MAX_CAMERA_GIGE_RAW10_HW
#define GIGE_UNIT_PACKET_DELAY       0U /* line rate; host receive buffer >=32MiB */
#else
#define GIGE_UNIT_PACKET_DELAY       50U /* timestamp ticks */
#endif
#define GIGE_UNIT_STREAM_SOURCE_PORT 49152U
#define GIGE_UNIT_WIDTH_MIN          160U
#define GIGE_UNIT_HEIGHT_MIN         120U
#define GIGE_UNIT_WIDTH_MAX          2592U
#define GIGE_UNIT_HEIGHT_MAX         1944U
#define GIGE_UNIT_WIDTH              GIGE_UNIT_WIDTH_MAX
#define GIGE_UNIT_HEIGHT             GIGE_UNIT_HEIGHT_MAX
#define GIGE_UNIT_INPUT_STRIDE       (GIGE_UNIT_WIDTH_MAX * 2U) /* DDR Y0 U Y1 V */
/* Compensate the module's horizontal optical orientation for a natural view. */
/* Matching image-control FPGA reserves LED GPIO3 for a short video reset pulse. */
#define GIGE_UNIT_VIDEO_RESET_GPIO    0xA0010000U
#define GIGE_UNIT_VIDEO_RESET_MASK    8U
#define GIGE_UNIT_SENSOR_I2C_HZ       50000U
#define GIGE_UNIT_SENSOR_SDA_HOLD     400U
#define GIGE_UNIT_SENSOR_MIRROR      1U
#define GIGE_UNIT_SENSOR_FLIP        0U
/* Full sensor area, color YUYV with 8 bits per component. */
#define GIGE_UNIT_PIXEL_FORMAT       0x02100032U /* PFNC YUV422_8 (YUYV) */
#define GIGE_UNIT_PAYLOAD_SIZE       (GIGE_UNIT_WIDTH * GIGE_UNIT_HEIGHT * (((GIGE_UNIT_PIXEL_FORMAT >> 16U) & 255U) >> 3U))
#define GIGE_UNIT_ACQUISITION_MODE   1U /* project enum Continuous */
#define GIGE_UNIT_ACQUISITION_ACTIVE 0U
#define GIGE_UNIT_TL_PARAMS_LOCKED   0U
#define GIGE_UNIT_DESTINATION_IP     0U /* host must configure */
#define GIGE_UNIT_DESTINATION_PORT   0U /* host must configure */
/* Board control defaults; thermal thresholds in degrees Celsius. */
#define GIGE_UNIT_FAN_START_C         45.0f
#define GIGE_UNIT_FAN_STOP_C          35.0f
#define GIGE_UNIT_FAN_DUTY            75U
#define GIGE_UNIT_LED_HALF_PERIOD_US  500000U
#define GIGE_UNIT_VCM_AVAILABLE       0U /* fixed-focus module until verified otherwise */
#endif
