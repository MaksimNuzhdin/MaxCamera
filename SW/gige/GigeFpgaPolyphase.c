/* Program the matching Xilinx VPSS using its official layer-two drivers. */
#include "GigeFpgaPolyphase.h"
#include "xv_hscaler_l2.h"
#include "xv_vscaler_l2.h"
#include "xil_io.h"
#include <string.h>
int GigeFpgaPolyphase_Program(uint32_t rows,uint32_t cols,uint32_t out_rows,uint32_t out_cols)
{
    if(!GigeFpgaPolyphase_Supported(rows,cols,out_rows,out_cols))return -1;
    static XV_Hscaler_l2 h;static XV_Vscaler_l2 v;
    memset(&h,0,sizeof(h));memset(&v,0,sizeof(v));
    h.Hsc.Config.BaseAddress=GIGE_FPGA_POLYPHASE_BASE;h.Hsc.Config.PixPerClk=1;h.Hsc.Config.NumVidComponents=3;
    h.Hsc.Config.MaxWidth=2592;h.Hsc.Config.MaxHeight=1944;h.Hsc.Config.MaxDataWidth=8;
    h.Hsc.Config.PhaseShift=6;h.Hsc.Config.ScalerType=2;h.Hsc.Config.NumTaps=6;h.Hsc.IsReady=XIL_COMPONENT_IS_READY;
    v.Vsc.Config.BaseAddress=GIGE_FPGA_POLYPHASE_BASE+0x20000;v.Vsc.Config.PixPerClk=1;v.Vsc.Config.NumVidComponents=3;
    v.Vsc.Config.MaxWidth=2592;v.Vsc.Config.MaxHeight=1944;v.Vsc.Config.MaxDataWidth=8;
    v.Vsc.Config.PhaseShift=6;v.Vsc.Config.ScalerType=2;v.Vsc.Config.NumTaps=6;v.Vsc.IsReady=XIL_COMPONENT_IS_READY;
    Xil_Out32(GIGE_FPGA_POLYPHASE_BASE+0x10004,0);Xil_Out32(GIGE_FPGA_POLYPHASE_BASE+0x10000,0);Xil_Out32(GIGE_FPGA_POLYPHASE_BASE+0x10000,2);
    short identity[64*6]={0};
    for(unsigned p=0;p<64;++p)identity[p*6+2]=4096;
    if(rows==out_rows)XV_VScalerLoadExtCoeff(&v,64,6,identity);
    if(cols==out_cols)XV_HScalerLoadExtCoeff(&h,64,6,identity);
    if(XV_VScalerSetup(&v,cols,rows,out_rows,XVIDC_CSF_RGB)!=XST_SUCCESS)return -1;
    if(XV_HScalerSetup(&h,out_rows,cols,out_cols,XVIDC_CSF_RGB,XVIDC_CSF_RGB)!=XST_SUCCESS)return -1;
    XV_HScalerStart(&h);XV_VScalerStart(&v);Xil_Out32(GIGE_FPGA_POLYPHASE_BASE+0x10000,3);
    return 0;
}
