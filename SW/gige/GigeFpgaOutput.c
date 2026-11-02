#include "GigeFpgaOutput.h"
#include "GigeFpgaOutputRegisters.h"
#include "GigeImage.h"
static int output_format(uint32_t pixel_format)
{
    switch(pixel_format) {
    case GIGE_PIXEL_RGB8:case GIGE_PIXEL_BGR8:return 0;
    case GIGE_PIXEL_YUYV8:return 1;
    case GIGE_PIXEL_UYVY8:return 2;
    case GIGE_PIXEL_YVYU8:return 3;
    case GIGE_PIXEL_VYUY8:return 4;
    case GIGE_PIXEL_YUV444:return 5;
    case GIGE_PIXEL_YUV420:return 6;
    default:return -1;
    }
}
int GigeFpgaOutput_Supported(uint32_t pixel_format)
{return output_format(pixel_format)>=0;}
int GigeFpgaOutput_Program(const GigeFpgaIO *io,uint32_t capabilities,
                          uint32_t rows,uint32_t cols,uint32_t pixel_format,int full_range)
{
    int format=output_format(pixel_format);
    if(format<0 || full_range<0 || full_range>1 || !rows || rows>1944 || !cols || cols>2592)return -1;
    if(format>=1 && format<=4 && (cols&1U))return -1;
    if(format==6 && ((rows|cols)&1U))return -1;
    /* A legacy bitstream must never receive an absent-bank transaction. */
    if(!(capabilities&GIGE_FPGA_OUTPUT_CAPABILITY))return format==0?0:-1;
    if(!io || !io->write)return -1;
    io->write(io->user,GIGE_FPGA_OUTPUT_BASE+XCAMERA_RGB_OUTPUT_CONTROL_ADDR_ROWS_DATA,rows);
    io->write(io->user,GIGE_FPGA_OUTPUT_BASE+XCAMERA_RGB_OUTPUT_CONTROL_ADDR_COLS_DATA,cols);
    io->write(io->user,GIGE_FPGA_OUTPUT_BASE+XCAMERA_RGB_OUTPUT_CONTROL_ADDR_FORMAT_DATA,(uint32_t)format);
    io->write(io->user,GIGE_FPGA_OUTPUT_BASE+XCAMERA_RGB_OUTPUT_CONTROL_ADDR_FULL_RANGE_DATA,(uint32_t)full_range);
    io->write(io->user,GIGE_FPGA_OUTPUT_BASE+XCAMERA_RGB_OUTPUT_CONTROL_ADDR_AP_CTRL,0x81);
    return 0;
}
