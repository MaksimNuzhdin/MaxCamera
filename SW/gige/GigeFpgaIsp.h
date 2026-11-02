#ifndef GIGE_FPGA_ISP_H
#define GIGE_FPGA_ISP_H
#include <stdint.h>
#include "GigeFeatureRegisters.h"
#define GIGE_FPGA_ISP_BASE 0xA00E0000U
#define GIGE_FPGA_COLOR_BASE 0xA0100000U
#define GIGE_FPGA_COLOR_SIGNATURE 0x49535002U
#define GIGE_FPGA_COLOR_CAPABILITIES 63U
#define GIGE_FPGA_V11_SIGNATURE 0x4953500BU
#define GIGE_FPGA_V12_SIGNATURE 0x4953500CU
#define GIGE_FPGA_V13_SIGNATURE 0x4953500DU
#define GIGE_FPGA_WRITER_PPC4_CAPABILITY 262144U
#define GIGE_FPGA_V13_CAPABILITIES (GIGE_FPGA_V12_CAPABILITIES | GIGE_FPGA_WRITER_PPC4_CAPABILITY)
#define GIGE_FPGA_REMAP_CAPABILITY 131072U
#define GIGE_FPGA_V12_CAPABILITIES (GIGE_FPGA_V11_CAPABILITIES | GIGE_FPGA_REMAP_CAPABILITY)
#define GIGE_FPGA_ADAPTIVE_CAPABILITY 65536U
#define GIGE_FPGA_V11_CAPABILITIES (GIGE_FPGA_V9_CAPABILITIES | GIGE_FPGA_ADAPTIVE_CAPABILITY)
#define GIGE_FPGA_ADAPTIVE_BASE 0xA0180000U
#define GIGE_FPGA_LED_SIGNATURE 0x4953500AU
#define GIGE_FPGA_V9_SIGNATURE 0x49535009U
#define GIGE_FPGA_V9_CAPABILITIES 65535U
#define GIGE_FPGA_V8_SIGNATURE 0x49535008U
#define GIGE_FPGA_V8_CAPABILITIES 32767U
#define GIGE_FPGA_V7_SIGNATURE 0x49535007U
#define GIGE_FPGA_V7_CAPABILITIES 16383U
#define GIGE_FPGA_SCENE_BASE 0xA0110000U
#define GIGE_FPGA_V6_SIGNATURE 0x49535006U
#define GIGE_FPGA_V6_CAPABILITIES 8191U
#define GIGE_FPGA_V5_SIGNATURE 0x49535005U
#define GIGE_FPGA_V5_CAPABILITIES 4095U
#define GIGE_FPGA_V4_SIGNATURE 0x49535004U
#define GIGE_FPGA_V4_CAPABILITIES 2047U
#define GIGE_FPGA_V3_SIGNATURE 0x49535003U
#define GIGE_FPGA_V3_CAPABILITIES 511U
#define GIGE_FPGA_ISP_GPIO 0xA00F0000U
#define GIGE_FPGA_ISP_SIGNATURE 0x49535001U
#define GIGE_FPGA_ISP_CAP_RAW 7U
enum { GIGE_ISP_ENABLE, GIGE_ISP_BPC, GIGE_ISP_BLACK,
       GIGE_ISP_RED, GIGE_ISP_GREEN, GIGE_ISP_BLUE, GIGE_ISP_MEDIAN, GIGE_ISP_SCALER,
       GIGE_ISP_INPUT_WIDTH, GIGE_ISP_INPUT_HEIGHT, GIGE_ISP_ROI_X, GIGE_ISP_ROI_Y,
       GIGE_ISP_ROI_WIDTH, GIGE_ISP_ROI_HEIGHT, GIGE_ISP_ROI_ENABLE, GIGE_ISP_CCM, GIGE_ISP_GAMMA, GIGE_ISP_AWB, GIGE_ISP_BILATERAL, GIGE_ISP_SIGMA_COLOR, GIGE_ISP_SIGMA_SPACE, GIGE_ISP_SHARPEN, GIGE_ISP_LSC, GIGE_ISP_SCENE, GIGE_ISP_YUV_RANGE, GIGE_ISP_INTERPOLATION, GIGE_ISP_CLAHE, GIGE_ISP_CLAHE_CLIP, GIGE_ISP_ADAPTIVE_TONE, GIGE_ISP_TONE_TARGET, GIGE_ISP_COUNT };
typedef struct { uint32_t value[GIGE_ISP_COUNT]; int32_t matrix[9],offsets[3]; uint8_t gamma[768]; uint16_t lsc[252]; } GigeFpgaConfig;
static inline void GigeFpgaConfig_Default(GigeFpgaConfig *p)
{
    for(unsigned i=0;i<GIGE_ISP_COUNT;++i)p->value[i]=0;
    p->value[GIGE_ISP_CLAHE_CLIP]=4;p->value[GIGE_ISP_TONE_TARGET]=128;
    p->value[GIGE_ISP_SIGMA_COLOR]=24;p->value[GIGE_ISP_SIGMA_SPACE]=256;
    for(unsigned i=0;i<9;++i)p->matrix[i]=(i%4==0)?1048576:0;
    for(unsigned i=0;i<3;++i)p->offsets[i]=0;
    for(unsigned i=0;i<768;++i)p->gamma[i]=(uint8_t)i;
    for(unsigned i=0;i<252;++i)p->lsc[i]=4096;
    p->value[GIGE_ISP_INPUT_WIDTH]=p->value[GIGE_ISP_ROI_WIDTH]=2592;
    p->value[GIGE_ISP_INPUT_HEIGHT]=p->value[GIGE_ISP_ROI_HEIGHT]=1944;
    p->value[GIGE_ISP_RED]=p->value[GIGE_ISP_GREEN]=p->value[GIGE_ISP_BLUE]=256;
}
typedef struct {
    void *user;
    uint32_t (*read)(void*,uint32_t);
    void (*write)(void*,uint32_t,uint32_t);
} GigeFpgaIO;
typedef struct { uint32_t sequence,valid_pixels,red_mean,green_mean,blue_mean,
    applied_red,applied_green,applied_blue,intensity_mean,clipped,dark,
    focus_mean,motion_mean,motion_valid; } GigeFpgaStatistics;
typedef struct { GigeFpgaIO io; GigeFpgaConfig config; uint32_t capabilities;
    uint32_t matrix_index,offset_channel,gamma_channel,gamma_index,output_pixels,color_running;
    uint32_t lsc_phase,lsc_row,lsc_column;
    uint32_t contrast_epoch,independent_stream_led,tone_epoch;
    GigeFpgaStatistics latest,snapshot; } GigeFpgaIsp;
static inline int GigeFpgaConfig_Valid(const GigeFpgaConfig *p)
{
    if(!p)return 0;
    for(unsigned i=0;i<252;++i)if(p->lsc[i]>16384)return 0;
    for(unsigned i=0;i<9;++i)if(p->matrix[i]<-8388608 || p->matrix[i]>8388607)return 0;
    for(unsigned i=0;i<3;++i)if(p->offsets[i]<-8388608 || p->offsets[i]>8388607)return 0;
    return p->value[GIGE_ISP_ADAPTIVE_TONE]<=1 && p->value[GIGE_ISP_TONE_TARGET]>=16 &&
        p->value[GIGE_ISP_TONE_TARGET]<=224 && p->value[GIGE_ISP_ENABLE]<=1 && p->value[GIGE_ISP_BPC]<=1 &&
        p->value[GIGE_ISP_BLACK]<=1023 && p->value[GIGE_ISP_RED]<=4096 &&
        p->value[GIGE_ISP_GREEN]<=4096 && p->value[GIGE_ISP_BLUE]<=4096 &&
        p->value[GIGE_ISP_MEDIAN]<=1 && p->value[GIGE_ISP_SCALER]<=1 &&
        p->value[GIGE_ISP_ROI_ENABLE]<=1 && p->value[GIGE_ISP_CCM]<=1 && p->value[GIGE_ISP_GAMMA]<=1 &&
        p->value[GIGE_ISP_CLAHE]<=1 && p->value[GIGE_ISP_CLAHE_CLIP]>=1 && p->value[GIGE_ISP_CLAHE_CLIP]<=8 && p->value[GIGE_ISP_INTERPOLATION]<=1 && p->value[GIGE_ISP_YUV_RANGE]<=1 && p->value[GIGE_ISP_SCENE]<=1 && p->value[GIGE_ISP_LSC]<=1 && p->value[GIGE_ISP_BILATERAL]<=1 && p->value[GIGE_ISP_SHARPEN]<=256 &&
        p->value[GIGE_ISP_SIGMA_COLOR]>=1 && p->value[GIGE_ISP_SIGMA_COLOR]<=255 &&
        p->value[GIGE_ISP_SIGMA_SPACE]>=64 && p->value[GIGE_ISP_SIGMA_SPACE]<=1024 &&
        (p->value[GIGE_ISP_AWB]==0 || p->value[GIGE_ISP_AWB]==2) &&
        p->value[GIGE_ISP_INPUT_WIDTH]>=16 && p->value[GIGE_ISP_INPUT_WIDTH]<=2592 &&
        !(p->value[GIGE_ISP_INPUT_WIDTH]&15U) &&
        p->value[GIGE_ISP_INPUT_HEIGHT]>=8 && p->value[GIGE_ISP_INPUT_HEIGHT]<=1944 &&
        !(p->value[GIGE_ISP_INPUT_HEIGHT]&1U) &&
        p->value[GIGE_ISP_ROI_X]<2592 && p->value[GIGE_ISP_ROI_Y]<1944 &&
        p->value[GIGE_ISP_ROI_WIDTH]>=1 && p->value[GIGE_ISP_ROI_WIDTH]<=2592 &&
        p->value[GIGE_ISP_ROI_HEIGHT]>=1 && p->value[GIGE_ISP_ROI_HEIGHT]<=1944;
}
#ifdef __cplusplus
extern "C" {
#endif
int GigeFpgaIsp_Init(GigeFpgaIsp*,const GigeFpgaIO*,int expected_hardware);
int GigeFpgaIsp_Address(uint32_t);
uint16_t GigeFpgaIsp_Read(const GigeFpgaIsp*,uint32_t,uint32_t*,int raw10,int running);
uint16_t GigeFpgaIsp_Write(GigeFpgaIsp*,uint32_t,uint32_t,int busy);
/* Only this scalar is audited for live writes; callers supply actual route state. */
int GigeFpgaIsp_BlackLiveAllowed(const GigeFpgaIsp*,int healthy,int requested,
                               int streaming,int sensor_streaming,int sensor_raw10);
uint16_t GigeFpgaIsp_WriteBlackLive(GigeFpgaIsp*,uint32_t,int allowed);
int GigeFpgaIsp_TransportWriteLocked(uint32_t address,int locked,int black_live);
int GigeFpgaIsp_Import(GigeFpgaIsp*,const GigeFpgaConfig*);
void GigeFpgaIsp_Disable(GigeFpgaIsp*);
void GigeFpgaIsp_CaptureStatistics(GigeFpgaIsp*);
typedef struct { uint32_t rows,cols,roi_x,roi_y,roi_width,roi_height,out_rows,out_cols; } GigeFpgaGeometry;
int GigeFpgaIsp_ColorSelected(const GigeFpgaIsp*,uint32_t pixel_format);
int GigeFpgaIsp_Geometry(const GigeFpgaIsp*,uint32_t rows,uint32_t cols,GigeFpgaGeometry*);
int GigeFpgaIsp_ApplyOutput(GigeFpgaIsp*,const GigeFpgaGeometry*,uint32_t pixel_format);
int GigeFpgaIsp_ApplyColor(GigeFpgaIsp*,const GigeFpgaGeometry*,int bgr);
int GigeFpgaIsp_Apply(GigeFpgaIsp*,uint32_t rows,uint32_t cols,int raw10);
#ifdef __cplusplus
}
#endif
#endif
