#ifndef GIGE_FPGA_CONTRAST_H
#define GIGE_FPGA_CONTRAST_H
#include "GigeFpgaIsp.h"
#include "GigeFpgaContrastRegisters.h"
#define GIGE_FPGA_CONTRAST_BASE 0xA0130000U
#define GIGE_FPGA_CONTRAST_CAPABILITY 32768U
static inline int GigeFpgaContrast_Geometry(uint32_t rows,uint32_t cols)
{return rows>=32 && rows<=1944 && cols>=32 && cols<=2592 && !(rows&3U) && !(cols&7U);}
/* The caller validates stopped acquisition and advances epoch on each start. */
static inline int GigeFpgaContrast_Program(const GigeFpgaIO *io,uint32_t capabilities,
    uint32_t rows,uint32_t cols,int enabled,uint32_t clip,uint32_t epoch)
{
    if(!(capabilities&GIGE_FPGA_CONTRAST_CAPABILITY))return enabled?-1:0;
    if(!io || !io->write || rows<8 || rows>1944 || cols<8 || cols>2592 ||
       clip<1 || clip>8 || (enabled && !GigeFpgaContrast_Geometry(rows,cols)))return -1;
    const uint32_t offsets[]={XCAMERA_LOCAL_CONTRAST_CONTROL_ADDR_ROWS_DATA,
        XCAMERA_LOCAL_CONTRAST_CONTROL_ADDR_COLS_DATA,XCAMERA_LOCAL_CONTRAST_CONTROL_ADDR_ENABLE_DATA,
        XCAMERA_LOCAL_CONTRAST_CONTROL_ADDR_CLIP_DATA,XCAMERA_LOCAL_CONTRAST_CONTROL_ADDR_EPOCH_DATA};
    const uint32_t values[]={rows,cols,(uint32_t)(enabled!=0),clip,epoch};
    for(unsigned i=0;i<5;++i)io->write(io->user,GIGE_FPGA_CONTRAST_BASE+offsets[i],values[i]);
    io->write(io->user,GIGE_FPGA_CONTRAST_BASE,0x81);return 0;
}
#endif
