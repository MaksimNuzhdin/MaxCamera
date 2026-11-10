#ifndef GIGE_VISION_VENDOR_DEFINES_H
#define GIGE_VISION_VENDOR_DEFINES_H
/* Fill before release. ASCII, excluding NUL: 31/31/31/47 bytes respectively. */
#define GIGE_VENDOR_NAME             "DINX Tech Studio"
#define GIGE_VENDOR_MODEL            "MAX 5640"
#if defined(MAX_CAMERA_AI_IPC) && MAX_CAMERA_AI_IPC
#define GIGE_VENDOR_DEVICE_VERSION   "0.3.17-qa-sccb-dev"
#elif defined(MAX_CAMERA_GIGE_FPGA_ISP) && MAX_CAMERA_GIGE_FPGA_ISP
#define GIGE_VENDOR_DEVICE_VERSION   "0.3.17-qa-sccb-dev"
#else
#define GIGE_VENDOR_DEVICE_VERSION   "0.2.2"
#endif
#define GIGE_VENDOR_INFO             "DINX Tech Studio"
/* Stable product identity and XML-version namespace. */
#define GIGE_VENDOR_PRODUCT_GUID     "d12ac79e-f980-5e65-8e5e-28710411ed0a"
#define GIGE_VENDOR_XML_VERSION_GUID "e83b4c88-0de1-5dc1-a912-69b6923ed3a6"
#endif
