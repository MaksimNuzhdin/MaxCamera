#include "GigeSensorFeatures.h"
#include "GigeVisionDefaults.h"
#include "GigeImage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t registers[65536];
static unsigned reads,writes;
static int fail;
static OV5640 sensor;
static int read_bus(void *u,uint8_t a,uint16_t r,uint8_t *p,uint16_t n)
{
    (void)u;assert(a==0x3c);++reads;if(fail)return -5;
    memcpy(p,registers+r,n);GigeSensorFeatures_Observe(r,p,n);return 0;
}
static int write_bus(void *u,uint8_t a,uint16_t r,const uint8_t *p,uint16_t n)
{
    (void)u;assert(a==0x3c);++writes;if(fail)return -5;
    memcpy(registers+r,p,n);
    if(r==0x3008 && p[0]==0x82)registers[r]=2;
    GigeSensorFeatures_Observe(r,p,n);return 0;
}
static void delay(void *u,uint32_t us) {(void)u;(void)us;}
static uint32_t get(uint32_t a) {uint32_t v;assert(!GigeSensorFeatures_Read(a,&v));return v;}
static void set(uint32_t a,uint32_t v) {uint16_t e=GigeSensorFeatures_Write(a,v);if(e)fprintf(stderr,"write %08x=%u -> %04x state=%d\n",a,v,e,sensor.state);assert(!e);}
int main(void)
{
    memset(registers,0,sizeof(registers));registers[0x300a]=0x56;registers[0x300b]=0x40;
    OV5640_IO io={0,read_bus,write_bus,delay};assert(!Ov5640_Bind(&sensor,&io,0x3c));
    GigeSensorFeatures_Bind(&sensor);assert(!Ov5640_InitWithClock(&sensor,12000000));
    OV5640_CONFIG config;Ov5640_DefaultConfig(&config);config.clock.xclk_hz=12000000;config.clock.multiplier=200;
    config.clock.mipi_div=1;config.window.vts=1042;assert(!Ov5640_Configure(&sensor,&config));
    assert(!Ov5640_SetExposureLimits(&sensor,1038,1038,512,0));
    assert(!GigeSensorFeatures_Prepare());
    GigePreset saved;memset(&saved,0,sizeof(saved));saved.width=640;saved.height=480;saved.pixel_format=GIGE_PIXEL_YUYV8;saved.frame_rate=29990;
    GigeFpgaConfig_Default(&saved.fpga);saved.fpga.value[GIGE_ISP_BLACK]=31;
    saved.fpga.value[GIGE_ISP_BPC]=1;saved.fpga.value[GIGE_ISP_RED]=512;
    saved.fpga.value[GIGE_ISP_MEDIAN]=1;saved.fpga.value[GIGE_ISP_SCALER]=1;
    saved.fpga.value[GIGE_ISP_ROI_ENABLE]=1;saved.fpga.value[GIGE_ISP_INPUT_WIDTH]=320;
    saved.fpga.value[GIGE_ISP_INPUT_HEIGHT]=240;saved.fpga.value[GIGE_ISP_ROI_X]=3;
    saved.fpga.value[GIGE_ISP_ROI_Y]=5;saved.fpga.value[GIGE_ISP_ROI_WIDTH]=301;
    saved.fpga.value[GIGE_ISP_ROI_HEIGHT]=229;
    saved.fpga.value[GIGE_ISP_CCM]=1;saved.fpga.value[GIGE_ISP_GAMMA]=1;saved.fpga.value[GIGE_ISP_AWB]=2;
    saved.fpga.value[GIGE_ISP_BILATERAL]=1;saved.fpga.value[GIGE_ISP_SIGMA_COLOR]=37;
    saved.fpga.value[GIGE_ISP_SIGMA_SPACE]=384;saved.fpga.value[GIGE_ISP_SHARPEN]=64;
    saved.fpga.value[GIGE_ISP_SCENE]=1;
    saved.fpga.value[GIGE_ISP_YUV_RANGE]=1;
    saved.fpga.value[GIGE_ISP_INTERPOLATION]=1;
    saved.fpga.value[GIGE_ISP_CLAHE]=1;saved.fpga.value[GIGE_ISP_CLAHE_CLIP]=6;
    saved.fpga.value[GIGE_ISP_LSC]=1;
    for(unsigned i=0;i<252;++i)saved.fpga.lsc[i]=4096+i*16;
    saved.fpga.matrix[1]=-262144;saved.fpga.offsets[0]=1048576;
    for(unsigned i=0;i<768;++i)saved.fpga.gamma[i]=(uint8_t)(255-i);
    assert(!GigeSensorFeatures_Export(&saved) && !GigePreset_Validate(&saved));
    uint8_t encoded[2048];uint32_t encoded_size=GigePreset_Encode(&saved,encoded,sizeof(encoded));assert(encoded_size);
    GigePreset decoded;assert(!GigePreset_Decode(&decoded,encoded,encoded_size) && !memcmp(&saved,&decoded,sizeof(saved)));
    encoded[4]^=1;assert(GigePreset_Decode(&decoded,encoded,encoded_size));encoded[4]^=1;
    encoded[encoded_size-(8+GIGE_ISP_COUNT*4+816+504)]^=1;assert(GigePreset_Decode(&decoded,encoded,encoded_size));encoded[encoded_size-(8+GIGE_ISP_COUNT*4+816+504)]^=1;
    /* All previous Flash schemas decode, with processing disabled. */
    uint8_t old[2048];memcpy(old,encoded,encoded_size);
    const unsigned extension=8+GIGE_ISP_COUNT*4+816+504;
    unsigned timing_size=encoded_size-extension;
    // ISP3 keeps its complete calibration tables while new detail controls default.
    unsigned isp3_size=timing_size+8+18*4+816;
    memmove(old+timing_size+8+18*4,encoded+timing_size+8+GIGE_ISP_COUNT*4,816);
    old[timing_size+3]=0x33;old[timing_size+4]=18;
    old[28]=(uint8_t)isp3_size;old[29]=(uint8_t)(isp3_size>>8);old[30]=old[31]=0;
    assert(!GigePreset_Decode(&decoded,old,isp3_size));
    assert(decoded.fpga.value[GIGE_ISP_AWB]==2 && !decoded.fpga.value[GIGE_ISP_BILATERAL] && !decoded.fpga.value[GIGE_ISP_SHARPEN]);
    assert(decoded.fpga.value[GIGE_ISP_SIGMA_COLOR]==24 && decoded.fpga.value[GIGE_ISP_SIGMA_SPACE]==256);
    assert(decoded.fpga.matrix[1]==-262144 && decoded.fpga.offsets[0]==1048576 && decoded.fpga.gamma[767]==0);
    memcpy(old,encoded,encoded_size);
    // ISP4 retains bilateral/sharpening and color tables; new LSC is neutral.
    unsigned isp4_size=timing_size+8+22*4+816;
    memmove(old+timing_size+8+22*4,encoded+timing_size+8+GIGE_ISP_COUNT*4,816);
    old[timing_size+3]=0x34;old[timing_size+4]=22;
    old[28]=(uint8_t)isp4_size;old[29]=(uint8_t)(isp4_size>>8);old[30]=old[31]=0;
    assert(!GigePreset_Decode(&decoded,old,isp4_size));
    assert(decoded.fpga.value[GIGE_ISP_BILATERAL]==1 && decoded.fpga.value[GIGE_ISP_SHARPEN]==64 && !decoded.fpga.value[GIGE_ISP_LSC]);
    assert(decoded.fpga.matrix[1]==-262144 && decoded.fpga.gamma[767]==0);
    for(unsigned i=0;i<252;++i)assert(decoded.fpga.lsc[i]==4096);
    memcpy(old,encoded,encoded_size);
    // ISP5 keeps the complete LSC grid and defaults new scene diagnostics off.
    unsigned isp5_size=timing_size+8+23*4+816+504;
    memmove(old+timing_size+8+23*4,encoded+timing_size+8+GIGE_ISP_COUNT*4,816+504);
    old[timing_size+3]=0x35;old[timing_size+4]=23;
    old[28]=(uint8_t)isp5_size;old[29]=(uint8_t)(isp5_size>>8);old[30]=old[31]=0;
    assert(!GigePreset_Decode(&decoded,old,isp5_size));
    assert(decoded.fpga.value[GIGE_ISP_LSC]==1 && !decoded.fpga.value[GIGE_ISP_SCENE]);
    for(unsigned i=0;i<252;++i)assert(decoded.fpga.lsc[i]==4096+i*16);
    memcpy(old,encoded,encoded_size);
    // ISP6 preserves color tables, LSC and scene diagnostics; range defaults limited.
    unsigned isp6_size=timing_size+8+24*4+816+504;
    memmove(old+timing_size+8+24*4,encoded+timing_size+8+GIGE_ISP_COUNT*4,816+504);
    old[timing_size+3]=0x36;old[timing_size+4]=24;
    old[28]=(uint8_t)isp6_size;old[29]=(uint8_t)(isp6_size>>8);old[30]=old[31]=0;
    assert(!GigePreset_Decode(&decoded,old,isp6_size));
    assert(decoded.fpga.value[GIGE_ISP_LSC]==1 && !decoded.fpga.value[GIGE_ISP_YUV_RANGE]);
    for(unsigned i=0;i<252;++i)assert(decoded.fpga.lsc[i]==4096+i*16);
    // Legacy native YUV master enable must not silently select the new RAW path.
    old[16]=(uint8_t)GIGE_PIXEL_YUYV8;old[17]=(uint8_t)(GIGE_PIXEL_YUYV8>>8);
    old[18]=(uint8_t)(GIGE_PIXEL_YUYV8>>16);old[19]=(uint8_t)(GIGE_PIXEL_YUYV8>>24);
    old[timing_size+8]=1;
    assert(!GigePreset_Decode(&decoded,old,isp6_size));
    assert(!decoded.fpga.value[GIGE_ISP_ENABLE]);
    memcpy(old,encoded,encoded_size);
    // ISP7 preserves hardware YUV selection/range and defaults interpolation to Area.
    unsigned isp7_size=timing_size+8+25*4+816+504;
    memmove(old+timing_size+8+25*4,encoded+timing_size+8+GIGE_ISP_COUNT*4,816+504);
    old[timing_size+3]=0x37;old[timing_size+4]=25;
    old[28]=(uint8_t)isp7_size;old[29]=(uint8_t)(isp7_size>>8);old[30]=old[31]=0;
    old[timing_size+8]=1;
    assert(!GigePreset_Decode(&decoded,old,isp7_size));
    assert(decoded.fpga.value[GIGE_ISP_ENABLE]==1 && decoded.fpga.value[GIGE_ISP_YUV_RANGE]==1);
    assert(!decoded.fpga.value[GIGE_ISP_INTERPOLATION]);
    for(unsigned i=0;i<252;++i)assert(decoded.fpga.lsc[i]==4096+i*16);
    assert(decoded.fpga.matrix[1]==-262144 && decoded.fpga.gamma[767]==0);
    memcpy(old,encoded,encoded_size);
    // ISP8 keeps hardware YUV and interpolation while local contrast defaults off.
    unsigned isp8_size=timing_size+8+26*4+816+504;
    memmove(old+timing_size+8+26*4,encoded+timing_size+8+GIGE_ISP_COUNT*4,816+504);
    old[timing_size+3]=0x38;old[timing_size+4]=26;
    old[28]=(uint8_t)isp8_size;old[29]=(uint8_t)(isp8_size>>8);old[30]=old[31]=0;
    old[timing_size+8]=1;
    assert(!GigePreset_Decode(&decoded,old,isp8_size));
    assert(decoded.fpga.value[GIGE_ISP_ENABLE]==1 && decoded.fpga.value[GIGE_ISP_YUV_RANGE]==1);
    assert(decoded.fpga.value[GIGE_ISP_INTERPOLATION]==1 && !decoded.fpga.value[GIGE_ISP_CLAHE]);
    assert(decoded.fpga.value[GIGE_ISP_CLAHE_CLIP]==4);
    for(unsigned i=0;i<252;++i)assert(decoded.fpga.lsc[i]==4096+i*16);
    assert(decoded.fpga.matrix[1]==-262144 && decoded.fpga.gamma[767]==0);
    memcpy(old,encoded,encoded_size);
    // Previously deployed ISP1 presets retain the six original controls.
    unsigned isp1_size=timing_size+32;
    old[timing_size]=0x49;old[timing_size+1]=0x53;old[timing_size+2]=0x50;old[timing_size+3]=0x31;
    old[timing_size+4]=6;old[timing_size+5]=old[timing_size+6]=old[timing_size+7]=0;
    old[28]=(uint8_t)isp1_size;old[29]=(uint8_t)(isp1_size>>8);old[30]=old[31]=0;
    old[timing_size+8]=1;
    assert(!GigePreset_Decode(&decoded,old,isp1_size));
    assert(!decoded.fpga.value[GIGE_ISP_ENABLE]);
    assert(decoded.fpga.value[GIGE_ISP_BLACK]==31 && decoded.fpga.value[GIGE_ISP_RED]==512);
    assert(decoded.fpga.value[GIGE_ISP_INPUT_WIDTH]==2592 && !decoded.fpga.value[GIGE_ISP_SCALER]);
    memcpy(old,encoded,encoded_size);
    unsigned isp2_size=timing_size+8+15*4;
    old[timing_size+3]=0x32;old[timing_size+4]=15;
    old[28]=(uint8_t)isp2_size;old[29]=(uint8_t)(isp2_size>>8);old[30]=old[31]=0;
    assert(!GigePreset_Decode(&decoded,old,isp2_size));
    assert(decoded.fpga.value[GIGE_ISP_MEDIAN]==1 && decoded.fpga.value[GIGE_ISP_SCALER]==1);
    assert(!decoded.fpga.value[GIGE_ISP_CCM] && !decoded.fpga.value[GIGE_ISP_GAMMA] && !decoded.fpga.value[GIGE_ISP_AWB]);
    assert(decoded.fpga.matrix[0]==1048576 && decoded.fpga.matrix[1]==0 && decoded.fpga.gamma[256+37]==37);
    old[28]=(uint8_t)timing_size;old[29]=(uint8_t)(timing_size>>8);old[30]=old[31]=0;
    assert(!GigePreset_Decode(&decoded,old,timing_size));
    assert(!decoded.fpga.value[GIGE_ISP_ENABLE] && decoded.fpga.value[GIGE_ISP_RED]==256);
    for(unsigned trim=4;trim<=12;trim+=8) {
        unsigned size=timing_size-trim;old[28]=(uint8_t)size;old[29]=(uint8_t)(size>>8);old[30]=old[31]=0;
        assert(!GigePreset_Decode(&decoded,old,size));
        assert(decoded.line_period_ns==32000 && decoded.compression==0);
        if(trim==12)assert(decoded.jpeg_quality==4);
    }
    // Old RAW10 profiles preserve absolute integration across40->80MHz.
    memcpy(old,encoded,encoded_size);old[16]=0x52;old[17]=0;old[18]=0x0a;old[19]=1;
    memset(old+32+GIGE_PRESET_MIRROR*4,0,4);memset(old+32+GIGE_PRESET_FLIP*4,0,4);
    old[28]=(uint8_t)(timing_size-4);old[29]=(uint8_t)((timing_size-4)>>8);old[30]=old[31]=0;
    assert(!GigePreset_Decode(&decoded,old,timing_size-4) && decoded.line_period_ns==71100);
    decoded.values[GIGE_PRESET_EXPOSURE_LINES]=100;decoded.values[GIGE_PRESET_MAX_EXPOSURE_50]=1000;decoded.values[GIGE_PRESET_MAX_EXPOSURE_60]=1800;
    GigePreset_AdaptTiming(&decoded,35550,1964);
    assert(decoded.values[GIGE_PRESET_EXPOSURE_LINES]==200 && decoded.values[GIGE_PRESET_MAX_EXPOSURE_50]==1964 && decoded.values[GIGE_PRESET_MAX_EXPOSURE_60]==1964);
    assert(decoded.line_period_ns==35550);
    decoded=saved;decoded.compression=1;decoded.jpeg_quality=16;
    assert(!GigePreset_Validate(&decoded));decoded.width=320;assert(GigePreset_Validate(&decoded));
    decoded=saved;decoded.pixel_format=GIGE_PIXEL_BG10P;decoded.values[GIGE_PRESET_MIRROR]=1;
    assert(GigePreset_Validate(&decoded));
    unsigned initial_writes=writes;decoded=saved;decoded.values[GIGE_PRESET_EXPOSURE_AUTO]=1;
    assert(GigeSensorFeatures_Import(&decoded)==GIGE_STATUS_INVALID_PARAM && writes==initial_writes);
    set(GIGE_F_AE_TARGET,96);assert(get(GIGE_F_AE_TARGET)==96 && registers[0x3a0f]>registers[0x3a10]);
    set(GIGE_F_SHARPNESS_AUTO,0);set(GIGE_F_SHARPNESS,63);assert(registers[0x5302]==63 && (registers[0x5308]&0x40));
    set(GIGE_F_DENOISE_AUTO,0);set(GIGE_F_DENOISE,32);assert(registers[0x5306]==32 && (registers[0x5308]&0x10));
    assert(!GigeSensorFeatures_Import(&saved));assert(get(GIGE_F_EXPOSURE_AUTO)==2 && get(GIGE_F_GAIN_AUTO)==2);
    assert(registers[0x3a0f]==saved.ae_thresholds[0] && registers[0x3a10]==saved.ae_thresholds[1]);

    assert(get(GIGE_F_SENSOR_FPS)>29900 && get(GIGE_F_LINE_PERIOD)==32000);
    assert(get(GIGE_F_EXPOSURE_AUTO)==2 && get(GIGE_F_GAIN_AUTO)==2);
    assert(GigeSensorFeatures_Write(GIGE_F_EXPOSURE_LINES,400)==GIGE_STATUS_ACCESS_DENIED);
    set(GIGE_F_EXPOSURE_AUTO,0);set(GIGE_F_EXPOSURE_LINES,400);
    assert(get(GIGE_F_EXPOSURE_LINES)==400 && get(GIGE_F_GAIN_AUTO)==2);
    set(GIGE_F_GAIN_AUTO,0);set(GIGE_F_GAIN,64);assert(get(GIGE_F_GAIN)==64);
    assert(GigeSensorFeatures_Write(GIGE_F_EXPOSURE_LINES,1039)==GIGE_STATUS_INVALID_PARAM);
    set(GIGE_F_MIRROR,1);set(GIGE_F_FLIP,1);assert((registers[0x3820]&6)==6 && (registers[0x3821]&6)==6);
    set(GIGE_F_TEST_PATTERN,1);assert(registers[0x503d]&128);set(GIGE_F_TEST_PATTERN,0);
    set(GIGE_F_AWB_AUTO,0);set(GIGE_F_WB_RED,1500);assert(get(GIGE_F_WB_RED)==1500);
    unsigned before=reads;get(GIGE_F_GAIN);get(GIGE_F_EXPOSURE_LINES);assert(reads==before); /* no polling I2C */
    assert(!Ov5640_Stream(&sensor,1));before=writes;
    assert(GigeSensorFeatures_Write(GIGE_F_MIRROR,0)==GIGE_STATUS_BUSY && writes==before);
    set(GIGE_F_GAMMA_INDEX,3); /* staging remains available during acquisition */
    assert(!Ov5640_Stream(&sensor,0));
    set(GIGE_F_GAMMA_INDEX,0);set(GIGE_F_GAMMA_VALUE,255);before=writes;
    assert(GigeSensorFeatures_Write(GIGE_F_GAMMA_APPLY,1)==GIGE_STATUS_INVALID_PARAM && writes==before);
    set(GIGE_F_REG_SELECTOR,0x3036);set(GIGE_F_PATCH_MASK,255);set(GIGE_F_PATCH_VALUE,1);before=writes;
    assert(GigeSensorFeatures_Write(GIGE_F_PATCH_APPLY,1)==GIGE_STATUS_WRITE_PROTECT && writes==before);
    set(GIGE_F_REG_SELECTOR,0x5381);set(GIGE_F_PATCH_VALUE,30);set(GIGE_F_PATCH_APPLY,1);assert(registers[0x5381]==30);
    assert(GigeSensorFeatures_Write(GIGE_F_REG_SELECTOR,0xdead)==GIGE_STATUS_INVALID_PARAM);
    assert(GigeSensorFeatures_Write(GIGE_F_VCM_APPLY,1)==GIGE_STATUS_NOT_IMPLEMENTED);
    Ov5640_Raw10VgaConfig(&config);assert(!Ov5640_Configure(&sensor,&config));
    assert(get(GIGE_F_SENSOR_SCLK)==40000000 && get(GIGE_F_LINE_PERIOD)==71100);
    before=writes;assert(GigeSensorFeatures_Write(GIGE_F_AWB_AUTO,2)==GIGE_STATUS_ACCESS_DENIED && writes==before);
    decoded=saved;decoded.pixel_format=GIGE_PIXEL_BG10P;
    decoded.values[GIGE_PRESET_MIRROR]=decoded.values[GIGE_PRESET_FLIP]=0;
    assert(!GigeSensorFeatures_Import(&decoded) && get(GIGE_F_AWB_AUTO)==0);
    uint32_t old_gain=get(GIGE_F_GAIN);unsigned old_reads=reads,old_writes=writes;
    OV5640_STATE old_state=sensor.state;
    GigeSensorFeatures_TelemetryError(0x350a,-4);
    assert(sensor.state==old_state && reads==old_reads && writes==old_writes);
    assert(get(GIGE_F_GAIN)==old_gain && get(GIGE_F_I2C_ERROR)==(uint32_t)-4);
    assert(get(GIGE_F_ERROR_REGISTER)==0x350a && get(GIGE_F_SENSOR_ERROR)==(uint32_t)OV5640_E_IO);
    fail=1;uint32_t v;assert(GigeSensorFeatures_Read(GIGE_F_REG_VALUE,&v)==0x8008);
    assert(get(GIGE_F_SENSOR_ERROR)==(uint32_t)OV5640_E_IO);
    puts("PASS: sensor feature cache, real API mapping, manual/auto isolation, streaming locks, range and fault handling");
}
