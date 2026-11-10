#include "GigeFpgaIsp.h"
#include "GigeFpgaV9Registers.h"
#include "GigeFpgaSceneV8Registers.h"
#include "GigeFpgaRawV5Registers.h"
#include "GigeFpgaContrast.h"
#include "GigeVisionRegisters.h"
#include "GigeImage.h"
#include <assert.h>
#include <stdio.h>
static uint32_t signature=GIGE_FPGA_V9_SIGNATURE;
static uint32_t raw[16384],color[16384],scene[16384],output[16384],contrast[16384],gpio;
static unsigned raw_writes,contrast_writes,programs;
static uint32_t last_rows,last_cols,last_oh,last_ow;
int GigeFpgaPolyphase_Program(uint32_t rows,uint32_t cols,uint32_t oh,uint32_t ow)
{++programs;last_rows=rows;last_cols=cols;last_oh=oh;last_ow=ow;return 0;}
static uint32_t *bank(uint32_t address) {
    if(address>=GIGE_FPGA_ISP_BASE && address<GIGE_FPGA_ISP_BASE+sizeof(raw))return raw+(address-GIGE_FPGA_ISP_BASE)/4;
    if(address>=GIGE_FPGA_COLOR_BASE && address<GIGE_FPGA_COLOR_BASE+sizeof(color))return color+(address-GIGE_FPGA_COLOR_BASE)/4;
    if(address>=GIGE_FPGA_SCENE_BASE && address<GIGE_FPGA_SCENE_BASE+sizeof(scene))return scene+(address-GIGE_FPGA_SCENE_BASE)/4;
    if(address>=0xA0120000U && address<0xA0120000U+sizeof(output))return output+(address-0xA0120000U)/4;
    if(address>=GIGE_FPGA_CONTRAST_BASE && address<GIGE_FPGA_CONTRAST_BASE+sizeof(contrast)) {
        assert(signature==GIGE_FPGA_V9_SIGNATURE || signature==GIGE_FPGA_LED_SIGNATURE);return contrast+(address-GIGE_FPGA_CONTRAST_BASE)/4;
    }
    assert(!"Unmapped FPGA access");return 0;
}
static uint32_t rd(void *u,uint32_t a) {(void)u;return a==GIGE_FPGA_ISP_GPIO?signature:*bank(a);}
static void wr(void *u,uint32_t a,uint32_t v) {
    (void)u;if(a==GIGE_FPGA_ISP_GPIO+8){gpio=v;return;}
    if(a>=GIGE_FPGA_ISP_BASE && a<GIGE_FPGA_ISP_BASE+sizeof(raw))++raw_writes;
    if(a>=GIGE_FPGA_CONTRAST_BASE && a<GIGE_FPGA_CONTRAST_BASE+sizeof(contrast))++contrast_writes;
    *bank(a)=v;
}
#define C(k) color[XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_##k##_DATA/4]
#define S(k) scene[XCAMERA_SCENE_METRICS_V8_CONTROL_ADDR_##k##_DATA/4]
#define L(k) contrast[XCAMERA_LOCAL_CONTRAST_CONTROL_ADDR_##k##_DATA/4]
int main(void) {
    GigeFpgaIO io={0,rd,wr};GigeFpgaIsp state;uint32_t value;
    assert(!GigeFpgaIsp_Init(&state,&io,1) && state.capabilities==65535);
    assert(!GigeFpgaIsp_Read(&state,GIGE_F_FPGA_VERSION,&value,0,0) && value==9);
    assert(!GigeFpgaIsp_Read(&state,GIGE_F_FPGA_CLAHE_AVAILABLE,&value,0,0) && value==1);
    assert(state.config.value[GIGE_ISP_CLAHE_CLIP]==4);
    assert(GigeFpgaIsp_Write(&state,GIGE_F_FPGA_CLAHE_CLIP,0,0)==GIGE_STATUS_INVALID_PARAM);
    assert(GigeFpgaIsp_Write(&state,GIGE_F_FPGA_CLAHE_CLIP,9,0)==GIGE_STATUS_INVALID_PARAM);
    assert(GigeFpgaIsp_Write(&state,GIGE_F_FPGA_CLAHE,2,0)==GIGE_STATUS_INVALID_PARAM);
    assert(GigeFpgaIsp_Write(&state,GIGE_F_FPGA_CLAHE_CLIP,4,1)==GIGE_STATUS_BUSY);
    assert(!GigeFpgaIsp_Write(&state,GIGE_F_FPGA_ENABLE,1,0));
    assert(!GigeFpgaIsp_Write(&state,GIGE_F_FPGA_SCALER,1,0));
    assert(!GigeFpgaIsp_Write(&state,GIGE_F_FPGA_INTERPOLATION,1,0));
    assert(!GigeFpgaIsp_Write(&state,GIGE_F_FPGA_CLAHE,1,0));
    assert(!GigeFpgaIsp_Write(&state,GIGE_F_FPGA_BPC,1,0));
    assert(!GigeFpgaIsp_Write(&state,GIGE_F_FPGA_BLACK,31,0));
    assert(!GigeFpgaIsp_Write(&state,GIGE_F_FPGA_LSC,1,0));
    assert(!GigeFpgaIsp_Write(&state,GIGE_F_FPGA_RED,768,0));
    assert(!GigeFpgaIsp_Write(&state,GIGE_F_FPGA_GREEN,384,0));
    assert(!GigeFpgaIsp_Write(&state,GIGE_F_FPGA_BLUE,128,0));
    GigeFpgaGeometry g;
    assert(GigeFpgaIsp_Geometry(&state,160,202,&g));
    assert(GigeFpgaIsp_Geometry(&state,30,32,&g));
    assert(!GigeFpgaIsp_Geometry(&state,480,640,&g));
    assert(!GigeFpgaIsp_ApplyColor(&state,&g,1));
    assert(programs==1 && last_rows==972 && last_cols==1296 && last_oh==480 && last_ow==640 && gpio==14);
    assert(raw[0x10/4]==1944 && raw[0x18/4]==2592 && raw[0x20/4]==1 && raw[0x28/4]==31);
    assert(raw[0x30/4]==256 && raw[0x38/4]==256 && raw[0x40/4]==256 && raw[0]==0x81);
    assert(raw[XCAMERA_RAW_CORRECT_V5_CONTROL_ADDR_LSC_ENABLE_DATA/4]==1);
    assert(raw[XCAMERA_RAW_CORRECT_V5_CONTROL_ADDR_LSC_GAINS_BASE/4]==0x10001000);
    assert(C(BLUE_GAIN)==128 && C(GREEN_GAIN)==384 && C(RED_GAIN)==768);
    assert(C(ROWS)==1944 && C(COLS)==2592 && C(OUT_ROWS)==972 && C(OUT_COLS)==1296);
    assert(L(ROWS)==480 && L(COLS)==640 && L(ENABLE)==1 && L(CLIP)==4 && L(EPOCH)==1 && contrast[0]==0x81);
    assert(S(ROWS)==480 && S(COLS)==640);
    assert(!GigeFpgaIsp_ApplyColor(&state,&g,0) && L(EPOCH)==2);
    C(FRAME_SEQUENCE)=2;C(VALID_PIXELS)=100;C(RED_SUM)=30000;C(GREEN_SUM)=20000;C(BLUE_SUM)=10000;
    C(APPLIED_RED)=768;C(APPLIED_GREEN)=384;C(APPLIED_BLUE)=128;
    C(INTENSITY_SUM)=480*640*100;
    S(FRAME_SEQUENCE)=2;S(INTENSITY_SUM)=480*640*50;S(CLIPPED_PIXELS)=7;S(DARK_PIXELS)=9;
    GigeFpgaIsp_CaptureStatistics(&state);
    assert(state.latest.sequence==1 && state.latest.red_mean==300*65536 && state.latest.intensity_mean==50*65536);
    assert(state.latest.clipped==7 && state.latest.dark==9);
    S(FRAME_SEQUENCE)=4;C(FRAME_SEQUENCE)=6;GigeFpgaIsp_CaptureStatistics(&state);assert(state.latest.sequence==1);
    assert(!GigeFpgaIsp_Write(&state,GIGE_F_FPGA_CLAHE,0,0));
    assert(!GigeFpgaIsp_Geometry(&state,160,202,&g) && !GigeFpgaIsp_ApplyColor(&state,&g,0));
    assert(L(ENABLE)==0 && L(COLS)==202 && L(EPOCH)==3);
    signature=GIGE_FPGA_V8_SIGNATURE;raw_writes=contrast_writes=0;
    assert(!GigeFpgaIsp_Init(&state,&io,1));
    assert(GigeFpgaIsp_Write(&state,GIGE_F_FPGA_CLAHE,1,0)==GIGE_STATUS_NOT_IMPLEMENTED);
    assert(!GigeFpgaIsp_Read(&state,GIGE_F_FPGA_CLAHE_AVAILABLE,&value,0,0) && value==0);
    assert(!GigeFpgaIsp_Write(&state,GIGE_F_FPGA_ENABLE,1,0));
    assert(!GigeFpgaIsp_Geometry(&state,32,32,&g) && !GigeFpgaIsp_ApplyColor(&state,&g,0));
    assert(!raw_writes && !contrast_writes);
    assert(!state.independent_stream_led);
    signature=GIGE_FPGA_LED_SIGNATURE;
    assert(!GigeFpgaIsp_Init(&state,&io,1) && state.independent_stream_led);
    assert(state.capabilities==GIGE_FPGA_V9_CAPABILITIES);
    assert(!GigeFpgaIsp_Read(&state,GIGE_F_FPGA_VERSION,&value,0,0) && value==10);
    assert(!GigeFpgaIsp_Write(&state,GIGE_F_FPGA_ENABLE,1,0));
    assert(!GigeFpgaIsp_Geometry(&state,480,640,&g) && !GigeFpgaIsp_ApplyColor(&state,&g,0));
    signature=GIGE_FPGA_V9_SIGNATURE;
    assert(!GigeFpgaIsp_Init(&state,&io,1) && !state.independent_stream_led);
    puts("PASS v9 controls: shared unity pre-gains, exact color/contrast ABI, post-scaler bounds, history epochs, final statistics and absent-bank legacy gating");
}
