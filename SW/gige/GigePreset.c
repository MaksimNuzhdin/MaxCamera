#include "GigePreset.h"
#include "GigeImage.h"
#include <string.h>
const uint32_t gige_preset_addresses[GIGE_PRESET_COUNT]={
#define X(k) GIGE_F_##k,
#include "GigePresetFeatures.inc"
#undef X
};
#define V(k) p->values[GIGE_PRESET_##k]
int GigePreset_Validate(const GigePreset *p)
{
    if(!p || !GigeImage_Valid(p->width,p->height,p->pixel_format) || p->frame_rate_enable>1 || p->frame_rate<200 || p->frame_rate>30000)return -1;
    if(p->line_period_ns>1000000000U || !GigeFpgaConfig_Valid(&p->fpga))return -1;
    if(p->compression>1 || p->jpeg_quality>63 || (p->compression && (p->width!=640 || p->height!=480 || p->pixel_format!=GIGE_PIXEL_YUYV8)))return -1;
    if(GigeImage_Raw(p->pixel_format) && (V(MIRROR) || V(FLIP)))return -1;
    for(unsigned i=0;i<GIGE_PRESET_COUNT;++i) {
        const GigeFeatureDescriptor *d=&gige_feature_descriptors[(gige_preset_addresses[i]-GIGE_FEATURE_FIRST)/4];
        if((int64_t)p->values[i]<d->minimum || (int64_t)p->values[i]>d->maximum)return -1;
    }
    const unsigned autos[]={GIGE_PRESET_EXPOSURE_AUTO,GIGE_PRESET_GAIN_AUTO,GIGE_PRESET_AWB_AUTO,GIGE_PRESET_SHARPNESS_AUTO,GIGE_PRESET_DENOISE_AUTO};
    for(unsigned i=0;i<sizeof(autos)/sizeof(autos[0]);++i)if(p->values[autos[i]]!=0 && p->values[autos[i]]!=2)return -1;
    if(V(BANDING)!=0 && V(BANDING)!=1 && V(BANDING)!=50 && V(BANDING)!=60)return -1;
    if((V(ISP0)&~0xa7U) || (V(ISP1)&~0xa7U) || (V(ISP3)&~7U) || V(DRAW_LEFT)>V(DRAW_RIGHT) || V(DRAW_TOP)>V(DRAW_BOTTOM))return -1;
    for(unsigned i=1;i<15;++i)if(p->gamma[i]<p->gamma[i-1])return -1;
    if(p->ae_thresholds[0]<=p->ae_thresholds[1] || p->ae_thresholds[3]<=p->ae_thresholds[4])return -1;
    return 0;
}
static void put(uint8_t *p,uint32_t v) {for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(i*8));}
static uint32_t get(const uint8_t *p) {return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
#define OLD_BYTES (32U+GIGE_PRESET_COUNT*4U+84U)
#define JPEG_BYTES (OLD_BYTES+8U)
#define TIMING_BYTES (JPEG_BYTES+4U)
#define ISP1_BYTES (TIMING_BYTES+8U+6U*4U)
#define ISP2_BYTES (TIMING_BYTES+8U+15U*4U)
#define ISP3_BYTES (TIMING_BYTES+8U+18U*4U+816U)
#define ISP4_BYTES (TIMING_BYTES+8U+22U*4U+816U)
#define ISP5_BYTES (TIMING_BYTES+8U+23U*4U+816U+504U)
#define ISP6_BYTES (TIMING_BYTES+8U+24U*4U+816U+504U)
#define ISP7_BYTES (TIMING_BYTES+8U+25U*4U+816U+504U)
#define ISP8_BYTES (TIMING_BYTES+8U+26U*4U+816U+504U)
#define ISP9_BYTES (TIMING_BYTES+8U+28U*4U+816U+504U)
#define SETTINGS_END (TIMING_BYTES+8U+GIGE_ISP_COUNT*4U)
#define COLOR_END (SETTINGS_END+12U*4U+768U)
#define BYTES (COLOR_END+504U)
uint32_t GigePreset_Encode(const GigePreset *p,uint8_t *data,uint32_t capacity)
{
    if(!data || capacity<BYTES || GigePreset_Validate(p))return 0;
    put(data,0x31505355U);put(data+4,GIGE_PRESET_COUNT);put(data+8,p->width);put(data+12,p->height);
    put(data+16,p->pixel_format);put(data+20,p->frame_rate_enable);put(data+24,p->frame_rate);put(data+28,BYTES);
    for(unsigned i=0;i<GIGE_PRESET_COUNT;++i)put(data+32+i*4,p->values[i]);
    memcpy(data+32+GIGE_PRESET_COUNT*4,p->gamma,16);memcpy(data+48+GIGE_PRESET_COUNT*4,p->lenc,62);
    memcpy(data+110+GIGE_PRESET_COUNT*4,p->ae_thresholds,6);
    put(data+OLD_BYTES,p->compression);put(data+OLD_BYTES+4,p->jpeg_quality);
    put(data+JPEG_BYTES,p->line_period_ns);
    put(data+TIMING_BYTES,0x41505349U);put(data+TIMING_BYTES+4,GIGE_ISP_COUNT);
    for(unsigned i=0;i<GIGE_ISP_COUNT;++i)put(data+TIMING_BYTES+8+i*4,p->fpga.value[i]);
    for(unsigned i=0;i<9;++i)put(data+SETTINGS_END+i*4,(uint32_t)p->fpga.matrix[i]);
    for(unsigned i=0;i<3;++i)put(data+SETTINGS_END+36+i*4,(uint32_t)p->fpga.offsets[i]);
    memcpy(data+SETTINGS_END+48,p->fpga.gamma,768);
    for(unsigned i=0;i<252;++i) {data[COLOR_END+i*2]=(uint8_t)p->fpga.lsc[i];data[COLOR_END+i*2+1]=(uint8_t)(p->fpga.lsc[i]>>8);}
    return BYTES;
}
int GigePreset_Decode(GigePreset *p,const uint8_t *data,uint32_t bytes)
{
    if(!p || !data || (bytes!=BYTES && bytes!=ISP1_BYTES && bytes!=ISP2_BYTES && bytes!=ISP3_BYTES && bytes!=ISP4_BYTES && bytes!=ISP5_BYTES && bytes!=ISP6_BYTES && bytes!=ISP7_BYTES && bytes!=ISP8_BYTES && bytes!=ISP9_BYTES && bytes!=TIMING_BYTES && bytes!=JPEG_BYTES && bytes!=OLD_BYTES) || get(data)!=0x31505355U || get(data+4)!=GIGE_PRESET_COUNT || get(data+28)!=bytes)return -1;
    GigePreset result;memset(&result,0,sizeof(result));result.width=get(data+8);result.height=get(data+12);
    result.pixel_format=get(data+16);result.frame_rate_enable=get(data+20);result.frame_rate=get(data+24);
    for(unsigned i=0;i<GIGE_PRESET_COUNT;++i)result.values[i]=get(data+32+i*4);
    memcpy(result.gamma,data+32+GIGE_PRESET_COUNT*4,16);memcpy(result.lenc,data+48+GIGE_PRESET_COUNT*4,62);
    memcpy(result.ae_thresholds,data+110+GIGE_PRESET_COUNT*4,6);
    result.compression=bytes>=JPEG_BYTES?get(data+OLD_BYTES):0;
    result.jpeg_quality=bytes>=JPEG_BYTES?get(data+OLD_BYTES+4):4;
    if(bytes>=TIMING_BYTES)result.line_period_ns=get(data+JPEG_BYTES);
    else {
        // Previous production profiles used40MHz RAW10 and fixed HTS2844.
        uint32_t hts=GigeImage_Raw(result.pixel_format)?2844:
                     (result.width<=1280 && result.height<=960?1600:2844);
        uint32_t clock=(result.pixel_format==GIGE_PIXEL_BG10 || result.pixel_format==GIGE_PIXEL_BG10P)?40000000:50000000;
        result.line_period_ns=(uint32_t)((uint64_t)hts*1000000000U/clock);
    }
    GigeFpgaConfig_Default(&result.fpga);
    if(bytes==BYTES || bytes==ISP1_BYTES || bytes==ISP2_BYTES || bytes==ISP3_BYTES || bytes==ISP4_BYTES || bytes==ISP5_BYTES || bytes==ISP6_BYTES || bytes==ISP7_BYTES || bytes==ISP8_BYTES || bytes==ISP9_BYTES) {
        unsigned count=bytes==ISP1_BYTES?6:bytes==ISP2_BYTES?15:bytes==ISP3_BYTES?18:bytes==ISP4_BYTES?22:bytes==ISP5_BYTES?23:bytes==ISP6_BYTES?24:bytes==ISP7_BYTES?25:bytes==ISP8_BYTES?26:bytes==ISP9_BYTES?28:GIGE_ISP_COUNT;
        uint32_t magic=bytes==ISP1_BYTES?0x31505349U:bytes==ISP2_BYTES?0x32505349U:bytes==ISP3_BYTES?0x33505349U:bytes==ISP4_BYTES?0x34505349U:bytes==ISP5_BYTES?0x35505349U:bytes==ISP6_BYTES?0x36505349U:bytes==ISP7_BYTES?0x37505349U:bytes==ISP8_BYTES?0x38505349U:bytes==ISP9_BYTES?0x39505349U:0x41505349U;
        if(get(data+TIMING_BYTES)!=magic || get(data+TIMING_BYTES+4)!=count)return -1;
        for(unsigned i=0;i<count;++i)result.fpga.value[i]=get(data+TIMING_BYTES+8+i*4);
    }
    if(bytes==BYTES || bytes==ISP3_BYTES || bytes==ISP4_BYTES || bytes==ISP5_BYTES || bytes==ISP6_BYTES || bytes==ISP7_BYTES || bytes==ISP8_BYTES || bytes==ISP9_BYTES) {
        unsigned tables=bytes==ISP3_BYTES?TIMING_BYTES+8U+18U*4U:bytes==ISP4_BYTES?TIMING_BYTES+8U+22U*4U:bytes==ISP5_BYTES?TIMING_BYTES+8U+23U*4U:bytes==ISP6_BYTES?TIMING_BYTES+8U+24U*4U:bytes==ISP7_BYTES?TIMING_BYTES+8U+25U*4U:bytes==ISP8_BYTES?TIMING_BYTES+8U+26U*4U:bytes==ISP9_BYTES?TIMING_BYTES+8U+28U*4U:SETTINGS_END;
        for(unsigned i=0;i<9;++i)result.fpga.matrix[i]=(int32_t)get(data+tables+i*4);
        for(unsigned i=0;i<3;++i)result.fpga.offsets[i]=(int32_t)get(data+tables+36+i*4);
        memcpy(result.fpga.gamma,data+tables+48,768);
    }
    if(bytes==BYTES || bytes==ISP5_BYTES || bytes==ISP6_BYTES || bytes==ISP7_BYTES || bytes==ISP8_BYTES || bytes==ISP9_BYTES) {
        unsigned grid=bytes==ISP5_BYTES?TIMING_BYTES+8U+23U*4U+816U:bytes==ISP6_BYTES?TIMING_BYTES+8U+24U*4U+816U:bytes==ISP7_BYTES?TIMING_BYTES+8U+25U*4U+816U:bytes==ISP8_BYTES?TIMING_BYTES+8U+26U*4U+816U:bytes==ISP9_BYTES?TIMING_BYTES+8U+28U*4U+816U:COLOR_END;
        for(unsigned i=0;i<252;++i)result.fpga.lsc[i]=(uint16_t)(data[grid+i*2]|((uint16_t)data[grid+i*2+1]<<8));
    }
    // ISP1 enabled correction only for RAW10. Preserve its bypass behavior
    // for saved RGB/YUV/RAW8 profiles when upgrading to the color pipeline.
    if(bytes==ISP1_BYTES && result.pixel_format!=GIGE_PIXEL_BG10 && result.pixel_format!=GIGE_PIXEL_BG10P)
        result.fpga.value[GIGE_ISP_ENABLE]=0;
    // Older presets could enable the master while native YUV bypassed the FPGA.
    // Preserve that capture path when adding opt-in hardware YUV processing.
    if(bytes!=BYTES && bytes!=ISP7_BYTES && bytes!=ISP8_BYTES && bytes!=ISP9_BYTES && (result.pixel_format==GIGE_PIXEL_YUYV8 || result.pixel_format==GIGE_PIXEL_UYVY8 ||
       result.pixel_format==GIGE_PIXEL_YVYU8 || result.pixel_format==GIGE_PIXEL_VYUY8 ||
       result.pixel_format==GIGE_PIXEL_YUV444 || result.pixel_format==GIGE_PIXEL_YUV420))
        result.fpga.value[GIGE_ISP_ENABLE]=0;
    if(GigePreset_Validate(&result))return -1;
    *p=result;return 0;
}

void GigePreset_AdaptTiming(GigePreset *p,uint32_t period,uint32_t max_lines)
{
    if(!p || !period || !max_lines)return;
    if(p->line_period_ns && p->line_period_ns!=period) {
        const unsigned fields[]={GIGE_PRESET_EXPOSURE_LINES,GIGE_PRESET_MAX_EXPOSURE_50,GIGE_PRESET_MAX_EXPOSURE_60};
        for(unsigned i=0;i<3;++i) {
            uint64_t lines=((uint64_t)p->values[fields[i]]*p->line_period_ns+period/2U)/period;
            if(!lines)lines=1;
            if(lines>max_lines)lines=max_lines;
            p->values[fields[i]]=(uint32_t)lines;
        }
    }
    p->line_period_ns=period;
}
