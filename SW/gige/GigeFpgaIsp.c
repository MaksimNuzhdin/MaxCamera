#include "GigeFpgaIsp.h"
#include "GigeVisionRegisters.h"
#include "GigeImage.h"
#include "GigeFpgaV3Registers.h"
#include "GigeFpgaV4Registers.h"
#include "GigeFpgaV5Registers.h"
#include "GigeFpgaRawV5Registers.h"
#include "GigeFpgaSceneV6Registers.h"
#include "GigeFpgaOutput.h"
#include "GigeFpgaV9Registers.h"
#include "GigeFpgaContrast.h"
#include "GigeFpgaAdaptiveRegisters.h"
#include "GigeFpgaPolyphase.h"
#include "GigeFpgaSceneV8Registers.h"
#define COLOR_REG(s,name) (((s)->capabilities&32768U)?XCAMERA_COLOR_PIPELINE_V9_CONTROL_##name:((s)->capabilities&2048U)?XCAMERA_COLOR_PIPELINE_V5_CONTROL_##name:((s)->capabilities&512U)?XCAMERA_COLOR_PIPELINE_V4_CONTROL_##name:XCAMERA_COLOR_PIPELINE_V3_CONTROL_##name)
#define DETAIL_REG(s,name) (((s)->capabilities&32768U)?XCAMERA_COLOR_PIPELINE_V9_CONTROL_##name:((s)->capabilities&2048U)?XCAMERA_COLOR_PIPELINE_V5_CONTROL_##name:XCAMERA_COLOR_PIPELINE_V4_CONTROL_##name)
#include <string.h>

int GigeFpgaIsp_Init(GigeFpgaIsp *s,const GigeFpgaIO *io,int expected)
{
    memset(s,0,sizeof(*s));GigeFpgaConfig_Default(&s->config);
    if(io)s->io=*io;
    /* Legacy builds must never access an address absent from their FPGA. */
    if(!expected)return 0;
    if(!io || !io->read || !io->write)return -1;
    uint32_t signature=io->read(io->user,GIGE_FPGA_ISP_GPIO);
    if(signature!=GIGE_FPGA_ISP_SIGNATURE && signature!=GIGE_FPGA_COLOR_SIGNATURE && signature!=GIGE_FPGA_V3_SIGNATURE && signature!=GIGE_FPGA_V4_SIGNATURE && signature!=GIGE_FPGA_V5_SIGNATURE && signature!=GIGE_FPGA_V6_SIGNATURE && signature!=GIGE_FPGA_V7_SIGNATURE && signature!=GIGE_FPGA_V8_SIGNATURE && signature!=GIGE_FPGA_V9_SIGNATURE && signature!=GIGE_FPGA_LED_SIGNATURE && signature!=GIGE_FPGA_V11_SIGNATURE && signature!=GIGE_FPGA_V12_SIGNATURE && signature!=GIGE_FPGA_V13_SIGNATURE)return -1;
    s->independent_stream_led=signature==GIGE_FPGA_LED_SIGNATURE || signature==GIGE_FPGA_V11_SIGNATURE || signature==GIGE_FPGA_V12_SIGNATURE || signature==GIGE_FPGA_V13_SIGNATURE;
    s->capabilities=signature==GIGE_FPGA_V13_SIGNATURE?GIGE_FPGA_V13_CAPABILITIES:signature==GIGE_FPGA_V12_SIGNATURE?GIGE_FPGA_V12_CAPABILITIES:signature==GIGE_FPGA_V11_SIGNATURE?GIGE_FPGA_V11_CAPABILITIES:(signature==GIGE_FPGA_V9_SIGNATURE || s->independent_stream_led)?GIGE_FPGA_V9_CAPABILITIES:signature==GIGE_FPGA_V8_SIGNATURE?GIGE_FPGA_V8_CAPABILITIES:signature==GIGE_FPGA_V7_SIGNATURE?GIGE_FPGA_V7_CAPABILITIES:signature==GIGE_FPGA_V6_SIGNATURE?GIGE_FPGA_V6_CAPABILITIES:signature==GIGE_FPGA_V5_SIGNATURE?GIGE_FPGA_V5_CAPABILITIES:signature==GIGE_FPGA_V4_SIGNATURE?GIGE_FPGA_V4_CAPABILITIES:signature==GIGE_FPGA_V3_SIGNATURE?GIGE_FPGA_V3_CAPABILITIES:signature==GIGE_FPGA_COLOR_SIGNATURE?GIGE_FPGA_COLOR_CAPABILITIES:GIGE_FPGA_ISP_CAP_RAW;GigeFpgaIsp_Disable(s);return 0;
}
int GigeFpgaIsp_Address(uint32_t a)
{return !(a&3U) && ((a>=GIGE_F_FPGA_VERSION && a<=GIGE_F_FPGA_ROI_HEIGHT) || (a>=GIGE_F_FPGA_COLOR_AVAILABLE && a<=GIGE_F_FPGA_TONE_PREVIOUS_MEAN));}
int GigeFpgaIsp_BlackLiveAllowed(const GigeFpgaIsp *s,int healthy,int requested,
                               int streaming,int sensor_streaming,int sensor_raw10)
{
    return s && (s->capabilities==GIGE_FPGA_V9_CAPABILITIES || s->capabilities==GIGE_FPGA_V11_CAPABILITIES || s->capabilities==GIGE_FPGA_V12_CAPABILITIES || s->capabilities==GIGE_FPGA_V13_CAPABILITIES) &&
        s->config.value[GIGE_ISP_ENABLE] && healthy && requested && streaming &&
        sensor_streaming && sensor_raw10;
}
int GigeFpgaIsp_TransportWriteLocked(uint32_t a,int locked,int black_live)
{
    return locked && GigeFpgaIsp_Address(a) && a!=GIGE_F_FPGA_STATS_LATCH &&
        !(a==GIGE_F_FPGA_BLACK && black_live);
}
uint16_t GigeFpgaIsp_WriteBlackLive(GigeFpgaIsp *s,uint32_t v,int allowed)
{
    if(!GigeFpgaIsp_BlackLiveAllowed(s,allowed,allowed,allowed,allowed,allowed))return GIGE_STATUS_BUSY;
    if(v>1023)return GIGE_STATUS_INVALID_PARAM;
    /* RAW5 latches this AXI-Lite scalar once per HLS invocation. Do not restart
     * the core or use DMA completion as a frame boundary. This also covers RGB. */
    s->io.write(s->io.user,GIGE_FPGA_ISP_BASE+XCAMERA_RAW_CORRECT_V5_CONTROL_ADDR_BLACK_LEVEL_DATA,v);
    s->config.value[GIGE_ISP_BLACK]=v;
    return 0;
}
static int field(uint32_t a)
{
    switch(a) {
    case GIGE_F_FPGA_ADAPTIVE_TONE:return GIGE_ISP_ADAPTIVE_TONE;
    case GIGE_F_FPGA_TONE_TARGET:return GIGE_ISP_TONE_TARGET;
    case GIGE_F_FPGA_ENABLE:return GIGE_ISP_ENABLE;
    case GIGE_F_FPGA_BPC:return GIGE_ISP_BPC;
    case GIGE_F_FPGA_BLACK:return GIGE_ISP_BLACK;
    case GIGE_F_FPGA_RED:return GIGE_ISP_RED;
    case GIGE_F_FPGA_GREEN:return GIGE_ISP_GREEN;
    case GIGE_F_FPGA_BLUE:return GIGE_ISP_BLUE;
    case GIGE_F_FPGA_MEDIAN:return GIGE_ISP_MEDIAN;
    case GIGE_F_FPGA_SCALER:return GIGE_ISP_SCALER;
    case GIGE_F_FPGA_INPUT_WIDTH:return GIGE_ISP_INPUT_WIDTH;
    case GIGE_F_FPGA_INPUT_HEIGHT:return GIGE_ISP_INPUT_HEIGHT;
    case GIGE_F_FPGA_ROI_X:return GIGE_ISP_ROI_X;
    case GIGE_F_FPGA_ROI_Y:return GIGE_ISP_ROI_Y;
    case GIGE_F_FPGA_ROI_WIDTH:return GIGE_ISP_ROI_WIDTH;
    case GIGE_F_FPGA_ROI_HEIGHT:return GIGE_ISP_ROI_HEIGHT;
    case GIGE_F_FPGA_ROI_ENABLE:return GIGE_ISP_ROI_ENABLE;
    case GIGE_F_FPGA_CCM:return GIGE_ISP_CCM;
    case GIGE_F_FPGA_GAMMA:return GIGE_ISP_GAMMA;
    case GIGE_F_FPGA_AWB:return GIGE_ISP_AWB;
    case GIGE_F_FPGA_BILATERAL:return GIGE_ISP_BILATERAL;
    case GIGE_F_FPGA_SIGMA_COLOR:return GIGE_ISP_SIGMA_COLOR;
    case GIGE_F_FPGA_SIGMA_SPACE:return GIGE_ISP_SIGMA_SPACE;
    case GIGE_F_FPGA_SHARPEN:return GIGE_ISP_SHARPEN;
    case GIGE_F_FPGA_LSC:return GIGE_ISP_LSC;
    case GIGE_F_FPGA_SCENE:return GIGE_ISP_SCENE;
    case GIGE_F_FPGA_CLAHE:return GIGE_ISP_CLAHE;
    case GIGE_F_FPGA_CLAHE_CLIP:return GIGE_ISP_CLAHE_CLIP;
    case GIGE_F_FPGA_INTERPOLATION:return GIGE_ISP_INTERPOLATION;
    case GIGE_F_FPGA_YUV_RANGE:return GIGE_ISP_YUV_RANGE;
    default:return -1;
    }
}
uint16_t GigeFpgaIsp_Read(const GigeFpgaIsp *s,uint32_t a,uint32_t *v,int raw10,int running)
{
    int f=field(a);if(!v || !GigeFpgaIsp_Address(a))return GIGE_STATUS_INVALID_ADDRESS;
    if(f>=0){*v=s->config.value[f];return 0;}
    switch(a) {
    /* Camera layer supplies actual route/health state; fail closed otherwise. */
    case GIGE_F_FPGA_BLACK_LIVE_ACTIVE:*v=0;break;
    case GIGE_F_FPGA_UPSCALE_AVAILABLE:*v=(s->capabilities&16384U)!=0;break;
    case GIGE_F_FPGA_UPSCALE_MAXIMUM:*v=(s->capabilities&16384U)?GIGE_FPGA_POLYPHASE_MAX_UPSCALE:1U;break;
    case GIGE_F_FPGA_TONE_AVAILABLE:*v=(s->capabilities&GIGE_FPGA_ADAPTIVE_CAPABILITY)!=0;break;
    case GIGE_F_FPGA_TONE_APPLIED_GAIN:
    case GIGE_F_FPGA_TONE_PREVIOUS_MEAN:
        if(!(s->capabilities&GIGE_FPGA_ADAPTIVE_CAPABILITY))return GIGE_STATUS_NOT_IMPLEMENTED;
        *v=s->io.read(s->io.user,GIGE_FPGA_ADAPTIVE_BASE+(a==GIGE_F_FPGA_TONE_APPLIED_GAIN?
            XCAMERA_ADAPTIVE_TONE_CONTROL_ADDR_APPLIED_GAIN_Q8_DATA:XCAMERA_ADAPTIVE_TONE_CONTROL_ADDR_PREVIOUS_MEAN_DATA));
        /* Status is unlatched invocation telemetry, not a GVSP-frame association. */
        if(a==GIGE_F_FPGA_TONE_APPLIED_GAIN && (*v<128 || *v>512))*v=256;
        break;
    case GIGE_F_FPGA_CLAHE_AVAILABLE:*v=(s->capabilities&32768U)!=0;break;
    case GIGE_F_FPGA_POLYPHASE_AVAILABLE:*v=(s->capabilities&16384U)!=0;break;
    case GIGE_F_FPGA_OUTPUT_AVAILABLE:*v=(s->capabilities&8192U)!=0;break;
    case GIGE_F_FPGA_SCENE_AVAILABLE:*v=(s->capabilities&4096U)!=0;break;
    case GIGE_F_FPGA_FOCUS_MEAN:*v=s->snapshot.focus_mean;break;
    case GIGE_F_FPGA_MOTION_MEAN:*v=s->snapshot.motion_mean;break;
    case GIGE_F_FPGA_MOTION_VALID:*v=s->snapshot.motion_valid;break;
    case GIGE_F_FPGA_LSC_AVAILABLE:*v=(s->capabilities&2048U)!=0;break;
    case GIGE_F_FPGA_LSC_PHASE:*v=s->lsc_phase;break;
    case GIGE_F_FPGA_LSC_ROW:*v=s->lsc_row;break;
    case GIGE_F_FPGA_LSC_COLUMN:*v=s->lsc_column;break;
    case GIGE_F_FPGA_LSC_GAIN:*v=s->config.lsc[s->lsc_phase*63+s->lsc_row*9+s->lsc_column];break;
    case GIGE_F_FPGA_DETAIL_AVAILABLE:*v=(s->capabilities&512U)!=0;break;
    case GIGE_F_FPGA_COLOR_AVAILABLE:*v=(s->capabilities&64U)!=0;break;
    case GIGE_F_FPGA_MATRIX_INDEX:*v=s->matrix_index;break;
    case GIGE_F_FPGA_MATRIX_VALUE:*v=(uint32_t)s->config.matrix[s->matrix_index];break;
    case GIGE_F_FPGA_OFFSET_CHANNEL:*v=s->offset_channel;break;
    case GIGE_F_FPGA_OFFSET_VALUE:*v=(uint32_t)s->config.offsets[s->offset_channel];break;
    case GIGE_F_FPGA_GAMMA_CHANNEL:*v=s->gamma_channel;break;
    case GIGE_F_FPGA_GAMMA_INDEX:*v=s->gamma_index;break;
    case GIGE_F_FPGA_GAMMA_VALUE:*v=s->config.gamma[s->gamma_channel*256+s->gamma_index];break;
    case GIGE_F_FPGA_STATS_SEQUENCE:*v=s->snapshot.sequence;break;
    case GIGE_F_FPGA_STATS_VALID:*v=s->snapshot.valid_pixels;break;
    case GIGE_F_FPGA_MEAN_RED:*v=s->snapshot.red_mean;break;
    case GIGE_F_FPGA_MEAN_GREEN:*v=s->snapshot.green_mean;break;
    case GIGE_F_FPGA_MEAN_BLUE:*v=s->snapshot.blue_mean;break;
    case GIGE_F_FPGA_MEAN_INTENSITY:*v=s->snapshot.intensity_mean;break;
    case GIGE_F_FPGA_STATS_CLIPPED:*v=s->snapshot.clipped;break;
    case GIGE_F_FPGA_STATS_DARK:*v=s->snapshot.dark;break;
    case GIGE_F_FPGA_APPLIED_RED:*v=s->snapshot.applied_red;break;
    case GIGE_F_FPGA_APPLIED_GREEN:*v=s->snapshot.applied_green;break;
    case GIGE_F_FPGA_APPLIED_BLUE:*v=s->snapshot.applied_blue;break;
    case GIGE_F_FPGA_VERSION:*v=s->capabilities==GIGE_FPGA_V13_CAPABILITIES?13:s->capabilities==GIGE_FPGA_V12_CAPABILITIES?12:s->capabilities==GIGE_FPGA_V11_CAPABILITIES?11:s->independent_stream_led?10:s->capabilities==GIGE_FPGA_V9_CAPABILITIES?9:s->capabilities==GIGE_FPGA_V8_CAPABILITIES?8:s->capabilities==GIGE_FPGA_V7_CAPABILITIES?7:s->capabilities==GIGE_FPGA_V6_CAPABILITIES?6:s->capabilities==GIGE_FPGA_V5_CAPABILITIES?5:s->capabilities==GIGE_FPGA_V4_CAPABILITIES?4:s->capabilities==GIGE_FPGA_V3_CAPABILITIES?3:s->capabilities==GIGE_FPGA_COLOR_CAPABILITIES?2:s->capabilities?1:0;break;
    case GIGE_F_FPGA_CAPABILITIES:*v=s->capabilities;break;
    case GIGE_F_FPGA_AVAILABLE:*v=s->capabilities!=0;break;
    case GIGE_F_FPGA_ACTIVE:*v=s->capabilities && raw10 && running && s->config.value[GIGE_ISP_ENABLE];break;
    case GIGE_F_FPGA_MEDIAN_AVAILABLE:*v=(s->capabilities&16U)!=0;break;
    case GIGE_F_FPGA_SCALER_AVAILABLE:*v=(s->capabilities&32U)!=0;break;
    default:return GIGE_STATUS_INVALID_ADDRESS;
    }
    return 0;
}
uint16_t GigeFpgaIsp_Write(GigeFpgaIsp *s,uint32_t a,uint32_t v,int busy)
{
    if(!GigeFpgaIsp_Address(a))return GIGE_STATUS_INVALID_ADDRESS;
    if(a==GIGE_F_FPGA_STATS_LATCH) {
        if(!(s->capabilities&64U))return GIGE_STATUS_NOT_IMPLEMENTED;
        if(v!=1)return GIGE_STATUS_INVALID_PARAM;
        s->snapshot=s->latest;return 0;
    }
    int f=field(a);
    int lsc_table=a==GIGE_F_FPGA_LSC_PHASE || a==GIGE_F_FPGA_LSC_ROW || a==GIGE_F_FPGA_LSC_COLUMN || a==GIGE_F_FPGA_LSC_GAIN;
    int table=lsc_table || a==GIGE_F_FPGA_MATRIX_INDEX || a==GIGE_F_FPGA_MATRIX_VALUE ||
        a==GIGE_F_FPGA_OFFSET_CHANNEL || a==GIGE_F_FPGA_OFFSET_VALUE ||
        a==GIGE_F_FPGA_GAMMA_CHANNEL || a==GIGE_F_FPGA_GAMMA_INDEX || a==GIGE_F_FPGA_GAMMA_VALUE;
    if(f<0 && !table)return GIGE_STATUS_WRITE_PROTECT;
    if(!s->capabilities || (f>=GIGE_ISP_MEDIAN && !(s->capabilities&8U)) ||
       ((table || f>=GIGE_ISP_CCM) && !(s->capabilities&64U)))return GIGE_STATUS_NOT_IMPLEMENTED;
    if(f>=GIGE_ISP_BILATERAL && !(s->capabilities&512U))return GIGE_STATUS_NOT_IMPLEMENTED;
    if((lsc_table || f==GIGE_ISP_LSC) && !(s->capabilities&2048U))return GIGE_STATUS_NOT_IMPLEMENTED;
    if(f>=GIGE_ISP_ADAPTIVE_TONE && !(s->capabilities&GIGE_FPGA_ADAPTIVE_CAPABILITY))return GIGE_STATUS_NOT_IMPLEMENTED;
    if(f>=GIGE_ISP_CLAHE && !(s->capabilities&32768U))return GIGE_STATUS_NOT_IMPLEMENTED;
    if(f==GIGE_ISP_INTERPOLATION && !(s->capabilities&16384U))return GIGE_STATUS_NOT_IMPLEMENTED;
    if(f==GIGE_ISP_YUV_RANGE && !(s->capabilities&8192U))return GIGE_STATUS_NOT_IMPLEMENTED;
    if(f==GIGE_ISP_SCENE && !(s->capabilities&4096U))return GIGE_STATUS_NOT_IMPLEMENTED;
    if(busy)return GIGE_STATUS_BUSY;
    if(table) {
        switch(a) {
        case GIGE_F_FPGA_LSC_PHASE:if(v>3)return GIGE_STATUS_INVALID_PARAM;s->lsc_phase=v;break;
        case GIGE_F_FPGA_LSC_ROW:if(v>6)return GIGE_STATUS_INVALID_PARAM;s->lsc_row=v;break;
        case GIGE_F_FPGA_LSC_COLUMN:if(v>8)return GIGE_STATUS_INVALID_PARAM;s->lsc_column=v;break;
        case GIGE_F_FPGA_LSC_GAIN:if(v>16384)return GIGE_STATUS_INVALID_PARAM;s->config.lsc[s->lsc_phase*63+s->lsc_row*9+s->lsc_column]=(uint16_t)v;break;
        case GIGE_F_FPGA_MATRIX_INDEX:if(v>8)return GIGE_STATUS_INVALID_PARAM;s->matrix_index=v;break;
        case GIGE_F_FPGA_OFFSET_CHANNEL:if(v>2)return GIGE_STATUS_INVALID_PARAM;s->offset_channel=v;break;
        case GIGE_F_FPGA_GAMMA_CHANNEL:if(v>2)return GIGE_STATUS_INVALID_PARAM;s->gamma_channel=v;break;
        case GIGE_F_FPGA_GAMMA_INDEX:if(v>255)return GIGE_STATUS_INVALID_PARAM;s->gamma_index=v;break;
        case GIGE_F_FPGA_MATRIX_VALUE:case GIGE_F_FPGA_OFFSET_VALUE:
            if((int32_t)v<-8388608 || (int32_t)v>8388607)return GIGE_STATUS_INVALID_PARAM;
            if(a==GIGE_F_FPGA_MATRIX_VALUE)s->config.matrix[s->matrix_index]=(int32_t)v;
            else s->config.offsets[s->offset_channel]=(int32_t)v;
            break;
        case GIGE_F_FPGA_GAMMA_VALUE:if(v>255)return GIGE_STATUS_INVALID_PARAM;s->config.gamma[s->gamma_channel*256+s->gamma_index]=(uint8_t)v;break;
        }
        return 0;
    }
    GigeFpgaConfig next=s->config;next.value[f]=v;
    if(!GigeFpgaConfig_Valid(&next))return GIGE_STATUS_INVALID_PARAM;
    s->config=next;return 0;
}
int GigeFpgaIsp_Import(GigeFpgaIsp *s,const GigeFpgaConfig *p)
{
    if(!GigeFpgaConfig_Valid(p) || (!s->capabilities && p->value[GIGE_ISP_ENABLE]))return -1;
    if(!(s->capabilities&8U) && (p->value[GIGE_ISP_MEDIAN] || p->value[GIGE_ISP_SCALER] || p->value[GIGE_ISP_ROI_ENABLE]))return -1;
    if(!(s->capabilities&64U) && (p->value[GIGE_ISP_CCM] || p->value[GIGE_ISP_GAMMA] || p->value[GIGE_ISP_AWB]))return -1;
    if(!(s->capabilities&512U) && (p->value[GIGE_ISP_BILATERAL] || p->value[GIGE_ISP_SHARPEN]))return -1;
    if(!(s->capabilities&2048U) && p->value[GIGE_ISP_LSC])return -1;
    if(!(s->capabilities&32768U) && p->value[GIGE_ISP_CLAHE])return -1;
    if(!(s->capabilities&GIGE_FPGA_ADAPTIVE_CAPABILITY) && p->value[GIGE_ISP_ADAPTIVE_TONE])return -1;
    if(!(s->capabilities&16384U) && p->value[GIGE_ISP_INTERPOLATION])return -1;
    if(!(s->capabilities&8192U) && p->value[GIGE_ISP_YUV_RANGE])return -1;
    if(!(s->capabilities&4096U) && p->value[GIGE_ISP_SCENE])return -1;
    s->config=*p;return 0;
}
void GigeFpgaIsp_Disable(GigeFpgaIsp *s)
{s->color_running=0;memset(&s->latest,0,sizeof(s->latest));if(s->capabilities)s->io.write(s->io.user,GIGE_FPGA_ISP_GPIO+8,0);}
static void program_lsc(GigeFpgaIsp *s,uint32_t base,uint32_t flag,uint32_t table)
{
    s->io.write(s->io.user,base+flag,s->config.value[GIGE_ISP_LSC]);
    for(unsigned i=0;i<252;i+=2) {
        uint32_t word=s->config.lsc[i]|((uint32_t)s->config.lsc[i+1]<<16);
        s->io.write(s->io.user,base+table+i*2,word);
    }
}
/* V9 shares BLC/BPC/LSC before demosaic. RGB gains remain in the color core. */
static void program_shared_raw(GigeFpgaIsp *s,uint32_t rows,uint32_t cols)
{
    s->io.write(s->io.user,GIGE_FPGA_ISP_BASE+0x10,rows);
    s->io.write(s->io.user,GIGE_FPGA_ISP_BASE+0x18,cols);
    s->io.write(s->io.user,GIGE_FPGA_ISP_BASE+0x20,s->config.value[GIGE_ISP_BPC]);
    s->io.write(s->io.user,GIGE_FPGA_ISP_BASE+0x28,s->config.value[GIGE_ISP_BLACK]);
    for(unsigned offset=0x30;offset<=0x40;offset+=8)s->io.write(s->io.user,GIGE_FPGA_ISP_BASE+offset,256);
    program_lsc(s,GIGE_FPGA_ISP_BASE,XCAMERA_RAW_CORRECT_V5_CONTROL_ADDR_LSC_ENABLE_DATA,XCAMERA_RAW_CORRECT_V5_CONTROL_ADDR_LSC_GAINS_BASE);
    s->io.write(s->io.user,GIGE_FPGA_ISP_BASE,0x81);
}
int GigeFpgaIsp_Apply(GigeFpgaIsp *s,uint32_t rows,uint32_t cols,int raw10)
{
    if(!s->capabilities)return s->config.value[GIGE_ISP_ENABLE]?-1:0;
    GigeFpgaIsp_Disable(s);
    if(!s->config.value[GIGE_ISP_ENABLE] || !raw10)return 0;
    if(!GigeFpgaConfig_Valid(&s->config) || rows<8 || rows>1944 || (rows&1U) ||
       cols<16 || cols>2592 || (cols&15U))return -1;
    /* HLS ABI of camera_raw_correct; checked against its exported driver. */
    const uint32_t offsets[]={0x20,0x28,0x40,0x38,0x30};
    const unsigned fields[]={GIGE_ISP_BPC,GIGE_ISP_BLACK,GIGE_ISP_RED,GIGE_ISP_GREEN,GIGE_ISP_BLUE};
    s->io.write(s->io.user,GIGE_FPGA_ISP_BASE+0x10,rows);
    s->io.write(s->io.user,GIGE_FPGA_ISP_BASE+0x18,cols);
    for(unsigned i=0;i<5;++i)s->io.write(s->io.user,GIGE_FPGA_ISP_BASE+offsets[i],s->config.value[fields[i]]);
    if(s->capabilities&2048U)program_lsc(s,GIGE_FPGA_ISP_BASE,XCAMERA_RAW_CORRECT_V5_CONTROL_ADDR_LSC_ENABLE_DATA,XCAMERA_RAW_CORRECT_V5_CONTROL_ADDR_LSC_GAINS_BASE);
    s->io.write(s->io.user,GIGE_FPGA_ISP_BASE,0x81); /* start + frame auto-restart */
    s->io.write(s->io.user,GIGE_FPGA_ISP_GPIO+8,1);return 0;
}

int GigeFpgaIsp_ColorSelected(const GigeFpgaIsp *s,uint32_t format)
{
    return (s->capabilities&8U) && s->config.value[GIGE_ISP_ENABLE] &&
           (format==GIGE_PIXEL_RGB8 || format==GIGE_PIXEL_BGR8 ||
            ((s->capabilities&8192U) && GigeFpgaOutput_Supported(format)));
}
int GigeFpgaIsp_Geometry(const GigeFpgaIsp *s,uint32_t rows,uint32_t cols,GigeFpgaGeometry *g)
{
    if(!g || !GigeFpgaConfig_Valid(&s->config) || !rows || !cols || rows>1944 || cols>2592)return -1;
    const uint32_t *v=s->config.value;
    if(v[GIGE_ISP_CLAHE] && !GigeFpgaContrast_Geometry(rows,cols))return -1;
    g->out_rows=rows;g->out_cols=cols;
    g->rows=v[GIGE_ISP_SCALER]?v[GIGE_ISP_INPUT_HEIGHT]:rows;
    g->cols=v[GIGE_ISP_SCALER]?v[GIGE_ISP_INPUT_WIDTH]:cols;
    if(g->rows<8 || (g->rows&1U) || g->cols<16 || (g->cols&15U))return -1;
    int roi=v[GIGE_ISP_SCALER] && v[GIGE_ISP_ROI_ENABLE];
    g->roi_x=roi?v[GIGE_ISP_ROI_X]:0;g->roi_y=roi?v[GIGE_ISP_ROI_Y]:0;
    g->roi_width=roi?v[GIGE_ISP_ROI_WIDTH]:g->cols;
    g->roi_height=roi?v[GIGE_ISP_ROI_HEIGHT]:g->rows;
    if(g->roi_x>=g->cols || g->roi_y>=g->rows ||
       g->roi_width>g->cols-g->roi_x || g->roi_height>g->rows-g->roi_y)return -1;
    GigeFpgaPolyphasePlan plan;
    if(GigeFpgaPolyphase_Plan(g->roi_height,g->roi_width,rows,cols,
       (s->capabilities&16384U) && v[GIGE_ISP_SCALER] && v[GIGE_ISP_INTERPOLATION],&plan))return -1;
    return 0;
}
static int apply_output(GigeFpgaIsp *s,const GigeFpgaGeometry *g,uint32_t format)
{
    GigeFpgaIsp_Disable(s);
    if(!(s->capabilities&8U) || !s->config.value[GIGE_ISP_ENABLE] || !g)return -1;
    GigeFpgaGeometry expected;
    if(GigeFpgaIsp_Geometry(s,g->out_rows,g->out_cols,&expected) || memcmp(g,&expected,sizeof(*g)))return -1;
    const uint32_t *v=s->config.value;
    if(GigeFpgaOutput_Program(&s->io,s->capabilities,g->out_rows,g->out_cols,format,(int)v[GIGE_ISP_YUV_RANGE]))return -1;
    GigeFpgaPolyphasePlan plan;
    if(GigeFpgaPolyphase_Plan(g->roi_height,g->roi_width,g->out_rows,g->out_cols,
       (s->capabilities&16384U) && v[GIGE_ISP_SCALER] && v[GIGE_ISP_INTERPOLATION],&plan))return -1;
    if(plan.active) {
#if defined(GIGE_POLYPHASE_SUPPORT) && GIGE_POLYPHASE_SUPPORT
        if(GigeFpgaPolyphase_Program(plan.rows,plan.cols,g->out_rows,g->out_cols))return -1;
#else
        return -1;
#endif
    }
    const uint32_t values[]={g->rows,g->cols,v[GIGE_ISP_BPC],v[GIGE_ISP_BLACK],
        v[GIGE_ISP_BLUE],v[GIGE_ISP_GREEN],v[GIGE_ISP_RED],v[GIGE_ISP_MEDIAN],
        g->roi_x,g->roi_y,g->roi_width,g->roi_height,plan.rows,plan.cols};
    /* Sequential 8-byte AXI-Lite slots; checked against the exported HLS ABI. */
    const uint32_t shared_offsets[]={XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_ROWS_DATA,
        XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_COLS_DATA,0,0,
        XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_BLUE_GAIN_DATA,XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_GREEN_GAIN_DATA,
        XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_RED_GAIN_DATA,XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_MEDIAN_ENABLE_DATA,
        XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_ROI_X_DATA,XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_ROI_Y_DATA,
        XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_ROI_WIDTH_DATA,XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_ROI_HEIGHT_DATA,
        XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_OUT_ROWS_DATA,XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_OUT_COLS_DATA};
    if(s->capabilities&32768U)program_shared_raw(s,g->rows,g->cols);
    if(GigeFpgaContrast_Program(&s->io,s->capabilities,g->out_rows,g->out_cols,
        (int)v[GIGE_ISP_CLAHE],v[GIGE_ISP_CLAHE_CLIP],++s->contrast_epoch))return -1;
    if(s->capabilities&GIGE_FPGA_ADAPTIVE_CAPABILITY) {
        const uint32_t base=GIGE_FPGA_ADAPTIVE_BASE;
        s->io.write(s->io.user,base+XCAMERA_ADAPTIVE_TONE_CONTROL_ADDR_ROWS_DATA,g->out_rows);
        s->io.write(s->io.user,base+XCAMERA_ADAPTIVE_TONE_CONTROL_ADDR_COLS_DATA,g->out_cols);
        s->io.write(s->io.user,base+XCAMERA_ADAPTIVE_TONE_CONTROL_ADDR_ENABLE_DATA,v[GIGE_ISP_ADAPTIVE_TONE]);
        s->io.write(s->io.user,base+XCAMERA_ADAPTIVE_TONE_CONTROL_ADDR_TARGET_DATA,v[GIGE_ISP_TONE_TARGET]);
        s->io.write(s->io.user,base+XCAMERA_ADAPTIVE_TONE_CONTROL_ADDR_EPOCH_DATA,++s->tone_epoch);
        s->io.write(s->io.user,base+XCAMERA_ADAPTIVE_TONE_CONTROL_ADDR_AP_CTRL,0x81);
    }
    for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);++i) {
        uint32_t offset=(s->capabilities&32768U)?shared_offsets[i]:0x10+i*8;
        if(offset)s->io.write(s->io.user,GIGE_FPGA_COLOR_BASE+offset,values[i]);
    }
    if(s->capabilities&64U) {
        s->io.write(s->io.user,GIGE_FPGA_COLOR_BASE+COLOR_REG(s,ADDR_CCM_ENABLE_DATA),v[GIGE_ISP_CCM]);
        s->io.write(s->io.user,GIGE_FPGA_COLOR_BASE+COLOR_REG(s,ADDR_GAMMA_ENABLE_DATA),v[GIGE_ISP_GAMMA]);
        s->io.write(s->io.user,GIGE_FPGA_COLOR_BASE+COLOR_REG(s,ADDR_AWB_ENABLE_DATA),v[GIGE_ISP_AWB]!=0);
        for(unsigned i=0;i<9;++i)s->io.write(s->io.user,GIGE_FPGA_COLOR_BASE+COLOR_REG(s,ADDR_MATRIX_BASE)+i*4,(uint32_t)s->config.matrix[i]);
        for(unsigned i=0;i<3;++i)s->io.write(s->io.user,GIGE_FPGA_COLOR_BASE+COLOR_REG(s,ADDR_OFFSETS_BASE)+i*4,(uint32_t)s->config.offsets[i]);
        for(unsigned i=0;i<768;i+=4) {
            uint32_t word=0;for(unsigned b=0;b<4;++b)word|=(uint32_t)s->config.gamma[i+b]<<(8*b);
            s->io.write(s->io.user,GIGE_FPGA_COLOR_BASE+COLOR_REG(s,ADDR_GAMMA_BASE)+i,word);
        }
    }
    if(s->capabilities&512U) {
        s->io.write(s->io.user,GIGE_FPGA_COLOR_BASE+DETAIL_REG(s,ADDR_BILATERAL_ENABLE_DATA),v[GIGE_ISP_BILATERAL]);
        s->io.write(s->io.user,GIGE_FPGA_COLOR_BASE+DETAIL_REG(s,ADDR_SIGMA_COLOR_DATA),v[GIGE_ISP_SIGMA_COLOR]);
        s->io.write(s->io.user,GIGE_FPGA_COLOR_BASE+DETAIL_REG(s,ADDR_SIGMA_SPACE_Q8_DATA),v[GIGE_ISP_SIGMA_SPACE]);
        s->io.write(s->io.user,GIGE_FPGA_COLOR_BASE+DETAIL_REG(s,ADDR_SHARPEN_STRENGTH_DATA),v[GIGE_ISP_SHARPEN]);
    }
    if((s->capabilities&2048U) && !(s->capabilities&32768U))program_lsc(s,GIGE_FPGA_COLOR_BASE,XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_LSC_ENABLE_DATA,XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_LSC_GAINS_BASE);
    s->output_pixels=g->out_rows*g->out_cols;
    memset(&s->latest,0,sizeof(s->latest));
    s->color_running=1;
    if(s->capabilities&4096U) {
        s->io.write(s->io.user,GIGE_FPGA_SCENE_BASE+XCAMERA_SCENE_METRICS_V6_CONTROL_ADDR_ROWS_DATA,g->out_rows);
        s->io.write(s->io.user,GIGE_FPGA_SCENE_BASE+XCAMERA_SCENE_METRICS_V6_CONTROL_ADDR_COLS_DATA,g->out_cols);
        s->io.write(s->io.user,GIGE_FPGA_SCENE_BASE+XCAMERA_SCENE_METRICS_V6_CONTROL_ADDR_ENABLE_DATA,v[GIGE_ISP_SCENE]);
        s->io.write(s->io.user,GIGE_FPGA_SCENE_BASE,0x81);
    }
    s->io.write(s->io.user,GIGE_FPGA_COLOR_BASE,0x81);
    s->io.write(s->io.user,GIGE_FPGA_ISP_GPIO+8,(format==GIGE_PIXEL_BGR8?6U:2U)|(plan.active?8U:0U));
    return 0;
}

int GigeFpgaIsp_ApplyColor(GigeFpgaIsp *s,const GigeFpgaGeometry *g,int bgr)
{return apply_output(s,g,bgr?GIGE_PIXEL_BGR8:GIGE_PIXEL_RGB8);}
int GigeFpgaIsp_ApplyOutput(GigeFpgaIsp *s,const GigeFpgaGeometry *g,uint32_t format)
{return apply_output(s,g,format);}

static uint32_t color_read(const GigeFpgaIsp *s,uint32_t offset)
{return s->io.read(s->io.user,GIGE_FPGA_COLOR_BASE+offset);}
static uint64_t color_read64(const GigeFpgaIsp *s,uint32_t offset)
{return color_read(s,offset)|((uint64_t)color_read(s,offset+4)<<32);}
static uint32_t scene_read(const GigeFpgaIsp *s,uint32_t offset)
{return s->io.read(s->io.user,GIGE_FPGA_SCENE_BASE+offset);}
void GigeFpgaIsp_CaptureStatistics(GigeFpgaIsp *s)
{
    if(!(s->capabilities&64U) || !s->color_running)return;
    GigeFpgaStatistics next={0};
    uint32_t seq=color_read(s,COLOR_REG(s,ADDR_FRAME_SEQUENCE_DATA));
    if(!seq || (seq&1U) || seq/2==s->latest.sequence)return;
    if((s->capabilities&4096U) && scene_read(s,XCAMERA_SCENE_METRICS_V6_CONTROL_ADDR_FRAME_SEQUENCE_DATA)!=seq)return;
    next.valid_pixels=color_read(s,COLOR_REG(s,ADDR_VALID_PIXELS_DATA));
    if(next.valid_pixels) {
        next.red_mean=(uint32_t)((color_read64(s,COLOR_REG(s,ADDR_RED_SUM_DATA))<<16)/next.valid_pixels);
        next.green_mean=(uint32_t)((color_read64(s,COLOR_REG(s,ADDR_GREEN_SUM_DATA))<<16)/next.valid_pixels);
        next.blue_mean=(uint32_t)((color_read64(s,COLOR_REG(s,ADDR_BLUE_SUM_DATA))<<16)/next.valid_pixels);
    }
    next.applied_red=color_read(s,COLOR_REG(s,ADDR_APPLIED_RED_DATA));
    next.applied_green=color_read(s,COLOR_REG(s,ADDR_APPLIED_GREEN_DATA));
    next.applied_blue=color_read(s,COLOR_REG(s,ADDR_APPLIED_BLUE_DATA));
    if(s->output_pixels)next.intensity_mean=(uint32_t)((color_read64(s,COLOR_REG(s,ADDR_INTENSITY_SUM_DATA))<<16)/s->output_pixels);
    next.clipped=color_read(s,COLOR_REG(s,ADDR_CLIPPED_PIXELS_DATA));
    next.dark=color_read(s,COLOR_REG(s,ADDR_DARK_PIXELS_DATA));
    if(s->capabilities&4096U) {
        next.focus_mean=scene_read(s,XCAMERA_SCENE_METRICS_V6_CONTROL_ADDR_FOCUS_Q16_DATA);
        next.motion_mean=scene_read(s,XCAMERA_SCENE_METRICS_V6_CONTROL_ADDR_MOTION_Q16_DATA);
        next.motion_valid=scene_read(s,XCAMERA_SCENE_METRICS_V6_CONTROL_ADDR_MOTION_VALID_DATA);
    }
    if(s->capabilities&16384U) {
        if(s->output_pixels)next.intensity_mean=(uint32_t)(((uint64_t)scene_read(s,XCAMERA_SCENE_METRICS_V8_CONTROL_ADDR_INTENSITY_SUM_DATA)<<16)/s->output_pixels);
        next.clipped=scene_read(s,XCAMERA_SCENE_METRICS_V8_CONTROL_ADDR_CLIPPED_PIXELS_DATA);
        next.dark=scene_read(s,XCAMERA_SCENE_METRICS_V8_CONTROL_ADDR_DARK_PIXELS_DATA);
    }
    if(seq!=color_read(s,COLOR_REG(s,ADDR_FRAME_SEQUENCE_DATA)) ||
       ((s->capabilities&4096U) && scene_read(s,XCAMERA_SCENE_METRICS_V6_CONTROL_ADDR_FRAME_SEQUENCE_DATA)!=seq))return;
    next.sequence=seq/2;s->latest=next;
}
