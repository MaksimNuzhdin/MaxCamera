#include "GigeFpgaIsp.h"
#include "GigeFpgaAdaptiveRegisters.h"
#include "GigeFpgaRawV5Registers.h"
#include "GigeVisionRegisters.h"
#include <assert.h>
#include <stdio.h>
#ifndef TEST_SIGNATURE
#define TEST_SIGNATURE GIGE_FPGA_V11_SIGNATURE
#endif
static uint32_t signature=TEST_SIGNATURE;
static uint32_t tone[64];static unsigned tone_reads,tone_writes,all_writes;
static uint32_t raw_black,route;
int GigeFpgaPolyphase_Program(uint32_t h,uint32_t w,uint32_t oh,uint32_t ow)
{(void)h;(void)w;(void)oh;(void)ow;return 0;}
static uint32_t rd(void *u,uint32_t a) {
    (void)u;if(a==GIGE_FPGA_ISP_GPIO)return signature;
    if(a>=GIGE_FPGA_ADAPTIVE_BASE && a<GIGE_FPGA_ADAPTIVE_BASE+sizeof(tone)) {
        assert(signature==GIGE_FPGA_V11_SIGNATURE || signature==GIGE_FPGA_V12_SIGNATURE || signature==GIGE_FPGA_V13_SIGNATURE);++tone_reads;return tone[(a-GIGE_FPGA_ADAPTIVE_BASE)/4];
    }
    return 0;
}
static void wr(void *u,uint32_t a,uint32_t v) {
    (void)u;++all_writes;
    if(a==GIGE_FPGA_ISP_GPIO+8)route=v;
    if(a==GIGE_FPGA_ISP_BASE+XCAMERA_RAW_CORRECT_V5_CONTROL_ADDR_BLACK_LEVEL_DATA)raw_black=v;
    if(a>=GIGE_FPGA_ADAPTIVE_BASE && a<GIGE_FPGA_ADAPTIVE_BASE+sizeof(tone)) {
        assert(signature==GIGE_FPGA_V11_SIGNATURE || signature==GIGE_FPGA_V12_SIGNATURE || signature==GIGE_FPGA_V13_SIGNATURE);++tone_writes;tone[(a-GIGE_FPGA_ADAPTIVE_BASE)/4]=v;
    }
}
#define T(k) tone[XCAMERA_ADAPTIVE_TONE_CONTROL_ADDR_##k##_DATA/4]
int main(void) {
    GigeFpgaIO io={0,rd,wr};GigeFpgaIsp s;GigeFpgaGeometry g;uint32_t v;
    assert(!GigeFpgaIsp_Init(&s,&io,1));assert(s.capabilities==(TEST_SIGNATURE==GIGE_FPGA_V13_SIGNATURE?524287U:TEST_SIGNATURE==GIGE_FPGA_V12_SIGNATURE?262143U:131071U) && s.independent_stream_led);
    assert(!GigeFpgaIsp_Read(&s,GIGE_F_FPGA_VERSION,&v,0,0) && v==(TEST_SIGNATURE==GIGE_FPGA_V13_SIGNATURE?13U:TEST_SIGNATURE==GIGE_FPGA_V12_SIGNATURE?12U:11U));
    assert(!GigeFpgaIsp_Read(&s,GIGE_F_FPGA_TONE_AVAILABLE,&v,0,0) && v==1);
    assert(s.config.value[GIGE_ISP_ADAPTIVE_TONE]==0 && s.config.value[GIGE_ISP_TONE_TARGET]==128);
    assert(GigeFpgaIsp_Write(&s,GIGE_F_FPGA_ADAPTIVE_TONE,2,0)==GIGE_STATUS_INVALID_PARAM);
    assert(GigeFpgaIsp_Write(&s,GIGE_F_FPGA_TONE_TARGET,15,0)==GIGE_STATUS_INVALID_PARAM);
    assert(GigeFpgaIsp_Write(&s,GIGE_F_FPGA_TONE_TARGET,225,0)==GIGE_STATUS_INVALID_PARAM);
    assert(GigeFpgaIsp_Write(&s,GIGE_F_FPGA_TONE_TARGET,128,1)==GIGE_STATUS_BUSY);
    assert(GigeFpgaIsp_Write(&s,GIGE_F_FPGA_ADAPTIVE_TONE,0,1)==GIGE_STATUS_BUSY);
    assert(!tone_writes && !tone_reads);
    assert(!GigeFpgaIsp_Write(&s,GIGE_F_FPGA_ENABLE,1,0));
    assert(!GigeFpgaIsp_Write(&s,GIGE_F_FPGA_ADAPTIVE_TONE,1,0));
    assert(!GigeFpgaIsp_Write(&s,GIGE_F_FPGA_TONE_TARGET,176,0));
    assert(!GigeFpgaIsp_Geometry(&s,480,640,&g));assert(!GigeFpgaIsp_ApplyColor(&s,&g,0));
    assert(tone_writes==6 && tone[0]==0x81 && T(ROWS)==480 && T(COLS)==640 && T(ENABLE)==1 && T(TARGET)==176 && T(EPOCH)==1);
    assert(route==2);
    assert(GigeFpgaIsp_BlackLiveAllowed(&s,1,1,1,1,1));
    unsigned previous=all_writes;assert(!GigeFpgaIsp_WriteBlackLive(&s,64,1));
    assert(all_writes==previous+1 && raw_black==64 && tone_writes==6);
    assert(GigeFpgaIsp_TransportWriteLocked(GIGE_F_FPGA_ADAPTIVE_TONE,1,1));
    assert(GigeFpgaIsp_TransportWriteLocked(GIGE_F_FPGA_TONE_TARGET,1,1));
    T(APPLIED_GAIN_Q8)=384;T(PREVIOUS_MEAN)=80;
    assert(!GigeFpgaIsp_Read(&s,GIGE_F_FPGA_TONE_APPLIED_GAIN,&v,0,0) && v==384);
    assert(!GigeFpgaIsp_Read(&s,GIGE_F_FPGA_TONE_PREVIOUS_MEAN,&v,0,0) && v==80);
    assert(GigeFpgaIsp_Write(&s,GIGE_F_FPGA_TONE_APPLIED_GAIN,256,0)==GIGE_STATUS_WRITE_PROTECT);
    T(APPLIED_GAIN_Q8)=0;assert(!GigeFpgaIsp_Read(&s,GIGE_F_FPGA_TONE_APPLIED_GAIN,&v,0,0) && v==256);
    assert(!GigeFpgaIsp_Write(&s,GIGE_F_FPGA_ADAPTIVE_TONE,0,0));
    assert(!GigeFpgaIsp_Write(&s,GIGE_F_FPGA_TONE_TARGET,16,0));
    assert(!GigeFpgaIsp_Geometry(&s,240,320,&g));assert(!GigeFpgaIsp_ApplyColor(&s,&g,0));
    assert(tone_writes==12 && T(ROWS)==240 && T(COLS)==320 && T(ENABLE)==0 && T(TARGET)==16 && T(EPOCH)==2);
    const uint32_t legacy[]={GIGE_FPGA_V9_SIGNATURE,GIGE_FPGA_LED_SIGNATURE,GIGE_FPGA_V8_SIGNATURE};
    for(unsigned i=0;i<3;++i) {
        signature=legacy[i];tone_writes=tone_reads=0;
        assert(!GigeFpgaIsp_Init(&s,&io,1));assert(s.independent_stream_led==(i==1));
        assert(!GigeFpgaIsp_Read(&s,GIGE_F_FPGA_TONE_AVAILABLE,&v,0,0) && v==0);
        assert(GigeFpgaIsp_Read(&s,GIGE_F_FPGA_TONE_APPLIED_GAIN,&v,0,0)==GIGE_STATUS_NOT_IMPLEMENTED);
        assert(GigeFpgaIsp_Read(&s,GIGE_F_FPGA_TONE_PREVIOUS_MEAN,&v,0,0)==GIGE_STATUS_NOT_IMPLEMENTED);
        assert(GigeFpgaIsp_Write(&s,GIGE_F_FPGA_ADAPTIVE_TONE,1,0)==GIGE_STATUS_NOT_IMPLEMENTED);
        GigeFpgaConfig p=s.config;p.value[GIGE_ISP_ADAPTIVE_TONE]=1;assert(GigeFpgaIsp_Import(&s,&p));
        p.value[GIGE_ISP_ADAPTIVE_TONE]=0;assert(!GigeFpgaIsp_Import(&s,&p));
        assert(!GigeFpgaIsp_Write(&s,GIGE_F_FPGA_ENABLE,1,0));
        assert(!GigeFpgaIsp_Geometry(&s,480,640,&g));assert(!GigeFpgaIsp_ApplyColor(&s,&g,0));
        assert(!tone_writes && !tone_reads);
        assert(GigeFpgaIsp_BlackLiveAllowed(&s,1,1,1,1,1)==(i!=2));
    }
    puts("PASS v11/v12/v13 adaptive MMIO, epoch/geometry/reset state, guards, LED identity/live black and zero absent-bank access on v8/v9/v10");
}
