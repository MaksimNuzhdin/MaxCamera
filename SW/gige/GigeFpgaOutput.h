#ifndef GIGE_FPGA_OUTPUT_H
#define GIGE_FPGA_OUTPUT_H
#include "GigeFpgaIsp.h"
#define GIGE_FPGA_OUTPUT_BASE 0xA0120000U
#define GIGE_FPGA_OUTPUT_CAPABILITY 8192U
#ifdef __cplusplus
extern "C" {
#endif
int GigeFpgaOutput_Supported(uint32_t pixel_format);
int GigeFpgaOutput_Program(const GigeFpgaIO*,uint32_t capabilities,
                          uint32_t rows,uint32_t cols,uint32_t pixel_format,int full_range);
#ifdef __cplusplus
}
#endif
#endif
