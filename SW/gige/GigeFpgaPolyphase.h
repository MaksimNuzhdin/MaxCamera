#ifndef GIGE_FPGA_POLYPHASE_H
#define GIGE_FPGA_POLYPHASE_H
#include <stdint.h>
#define GIGE_FPGA_POLYPHASE_BASE 0xA0140000U
#define GIGE_FPGA_POLYPHASE_MAX_UPSCALE 4U
/* Conservative firmware qualification bound, not a new PL capability bit. */
static inline int GigeFpgaPolyphase_Dimensions(uint32_t rows,uint32_t cols,
    uint32_t out_rows,uint32_t out_cols)
{
    return rows && rows<=1944 && cols && cols<=2592 && out_rows &&
        out_rows<=1944 && out_cols && out_cols<=2592;
}
static inline int GigeFpgaPolyphase_Supported(uint32_t rows,uint32_t cols,
    uint32_t out_rows,uint32_t out_cols)
{
    return GigeFpgaPolyphase_Dimensions(rows,cols,out_rows,out_cols) &&
        out_rows<=GIGE_FPGA_POLYPHASE_MAX_UPSCALE*rows &&
        out_cols<=GIGE_FPGA_POLYPHASE_MAX_UPSCALE*cols &&
        rows<=4*out_rows && cols<=4*out_cols;
}
/* The existing area stage prefilters ratios beyond the VPSS four-to-one range. */
typedef struct { uint32_t rows,cols; int active; } GigeFpgaPolyphasePlan;
static inline int GigeFpgaPolyphase_Plan(uint32_t rows,uint32_t cols,
    uint32_t out_rows,uint32_t out_cols,int selected,GigeFpgaPolyphasePlan *p)
{
    if(!p || !GigeFpgaPolyphase_Dimensions(rows,cols,out_rows,out_cols) ||
       ((!selected) && (out_rows>rows || out_cols>cols)) ||
       out_rows>GIGE_FPGA_POLYPHASE_MAX_UPSCALE*rows ||
       out_cols>GIGE_FPGA_POLYPHASE_MAX_UPSCALE*cols)return -1;
    p->active=selected && (rows!=out_rows || cols!=out_cols);
    p->rows=out_rows;p->cols=out_cols;
    if(p->active) {
        p->rows=rows;p->cols=cols;
        while(p->rows>4*out_rows)p->rows=(p->rows+1)/2;
        while(p->cols>4*out_cols)p->cols=(p->cols+1)/2;
        /* Upscaled axes stay at input size through the area-only HLS stage. */
    }
    return 0;
}
#ifdef __cplusplus
extern "C" {
#endif
/* Call only with the matching v8 capability, while acquisition is stopped. */
int GigeFpgaPolyphase_Program(uint32_t rows,uint32_t cols,uint32_t out_rows,uint32_t out_cols);
#ifdef __cplusplus
}
#endif
#endif
