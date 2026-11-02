#ifndef GIGE_VISION_DEFAULTS_H
#define GIGE_VISION_DEFAULTS_H
#include "GigeVisionRegisters.h"
#include "GigeVisionVendorDefines.h"
#include "GigeVisionUnitDefines.h"
/* Standard-profile defaults. Unit values remain in the separate unit file. */
#define GIGE_DEFAULT_INTERFACE_COUNT 1U
#define GIGE_DEFAULT_MESSAGE_COUNT   0U
#define GIGE_DEFAULT_STREAM_COUNT    1U
#define GIGE_DEFAULT_CAPABILITY      0xC0000006U
#define GIGE_DEFAULT_CCP             GIGE_CCP_OPEN
#define GIGE_DEFAULT_TIMESTAMP_LATCH 0ULL
#define GIGE_HEARTBEAT_MIN_MS        500U
#define GIGE_HEARTBEAT_MAX_MS        60000U
#define GIGE_PACKET_SIZE_MIN         576U
#define GIGE_PACKET_SIZE_MAX         1500U
#define GIGE_READ_MEMORY_MAX         512U
#endif
