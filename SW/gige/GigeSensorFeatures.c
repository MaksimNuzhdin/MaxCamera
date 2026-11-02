#include "GigeSensorFeatures.h"
#include "GigeVisionDefaults.h"
#include <string.h>
/* Only the single main loop owns this state. Bus callbacks observe every
 * successful read/write, so GenApi polling normally requires no I2C transfer. */
static OV5640 *sensor;
static uint8_t cache[65536],gamma[16],lenc[62],otp[32];
static uint32_t staged[GIGE_FEATURE_COUNT];
static int last_error,otp_valid;
static unsigned refresh_index;
static unsigned index_of(uint32_t a) {return (a-GIGE_FEATURE_FIRST)/4U;}
#define S(k) staged[index_of(GIGE_F_##k)]
static uint32_t pair(uint16_t r,uint8_t mask) {return ((uint32_t)(cache[r]&mask)<<8)|cache[r+1];}
static int known_register(uint16_t r)
{
    for(unsigned i=0;i<GIGE_SENSOR_REGISTER_COUNT;++i)if(gige_sensor_register_catalogue[i]==r)return 1;
    return 0;
}
static uint16_t status(int e)
{
    if(!e)return 0;
    last_error=e;
    if(e==OV5640_E_STATE)return GIGE_STATUS_BUSY;
    if(e==OV5640_E_RANGE || e==OV5640_E_ARGUMENT)return GIGE_STATUS_INVALID_PARAM;
    return 0x8008U; /* local hardware/transport problem */
}
void GigeSensorFeatures_Bind(OV5640 *s)
{
    sensor=s;memset(cache,0,sizeof(cache));memset(staged,0,sizeof(staged));
    memset(otp,0,sizeof(otp));otp_valid=0;last_error=0;refresh_index=0;
}
void GigeSensorFeatures_Observe(uint16_t r,const uint8_t *data,uint16_t n)
{if((uint32_t)r+n<=sizeof(cache))memcpy(cache+r,data,n);}
int GigeSensorFeatures_Prepare(void)
{
    const uint16_t ranges[][2]={{0x3400,0x3406},{0x3500,0x350b},{0x380c,0x380f},{0x3820,0x3821},
        {0x5000,0x5003},{0x5028,0x502f},{0x5035,0x5036},{0x503d,0x503d},
        {0x5480,0x5490},{0x5580,0x558b},{0x5800,0x583d},{0x3602,0x3606},
        {0x3a00,0x3a00},{0x3a02,0x3a03},{0x3a14,0x3a15},{0x3a18,0x3a19},{0x3c00,0x3c01},
        {0x3a0f,0x3a11},{0x3a1b,0x3a1b},{0x3a1e,0x3a1f},{0x5302,0x5308},{0x56a1,0x56a1}};
    for(unsigned i=0;i<sizeof(ranges)/sizeof(ranges[0]);++i) {
        unsigned n=ranges[i][1]-ranges[i][0]+1;uint8_t p[64];
        int e=Ov5640_Read(sensor,ranges[i][0],p,(uint16_t)n);if(e)return e;
        GigeSensorFeatures_Observe(ranges[i][0],p,(uint16_t)n);
    }
    memcpy(gamma,cache+0x5481,sizeof(gamma));memcpy(lenc,cache+0x5800,sizeof(lenc));
    S(GAMMA_CONTROL)=cache[0x5480]&3;S(LENC_ENABLE)=(cache[0x5000]>>7)&1;
    const uint32_t fx[]={GIGE_F_FX_ENABLE,GIGE_F_FX_COS,GIGE_F_FX_SIN,GIGE_F_FX_U,GIGE_F_FX_V,GIGE_F_FX_Y,GIGE_F_FX_CONTRAST,GIGE_F_FX_BRIGHTNESS};
    for(unsigned i=0;i<8;++i)staged[index_of(fx[i])]=cache[0x5580+i];
    S(FX_SIGNS)=cache[0x5588]&63;S(FX_SATURATION)=(cache[0x5588]>>6)&1;
    S(FX_UV1)=cache[0x5589];S(FX_UV2)=pair(0x558a,1);
    S(DRAW_LEFT)=pair(0x5028,15);S(DRAW_RIGHT)=pair(0x502a,15);
    S(DRAW_TOP)=pair(0x502c,7);S(DRAW_BOTTOM)=pair(0x502e,7);
    S(DRAW_U)=cache[0x5035];S(DRAW_V)=cache[0x5036];S(DRAW_ENABLE)=(cache[0x5003]>>1)&1;
    S(VCM_TARGET)=((cache[0x3603]&63)<<4)|(cache[0x3602]>>4);
    S(VCM_SLEW)=cache[0x3602]&15;
    S(VCM_DIVIDER)=((cache[0x3605]&15)<<8)|cache[0x3604];
    S(VCM_CURRENT)=cache[0x3606]&7;S(VCM_RIH)=(cache[0x3605]>>4)&1;S(VCM_POWERDOWN)=(cache[0x3603]>>7)&1;
    S(REG_SELECTOR)=0x300a;S(PATCH_MASK)=255;return 0;
}
void GigeSensorFeatures_TelemetryError(uint16_t reg,int error)
{
    if(!sensor || !error)return;
    last_error=OV5640_E_IO;sensor->transport_error=error;sensor->error_register=reg;
    /* Match read-error semantics: retain last-good cache and sensor state. */
}
int GigeSensorFeatures_RefreshOne(void)
{
    static const uint16_t dynamic[]={0x3500,0x3501,0x3502,0x350a,0x350b,0x3503,0x3400,0x3401,0x3402,0x3403,0x3404,0x3405,0x3406,0x56a1};
    if(!sensor || sensor->state<OV5640_CONFIGURED || sensor->state==OV5640_FAULT)return 0;
    uint16_t r=dynamic[refresh_index++%(sizeof(dynamic)/sizeof(dynamic[0]))];uint8_t v;
    int e=Ov5640_Read(sensor,r,&v,1);if(!e)cache[r]=v;else last_error=e;return e;
}
uint16_t GigeSensorFeatures_Read(uint32_t a,uint32_t *v)
{
    if(!sensor)return GIGE_STATUS_BUSY;
    switch(a) {
    case GIGE_F_SENSOR_STATE:*v=sensor->state;return 0;
    case GIGE_F_OTP_VALID:*v=otp_valid;return 0;
    case GIGE_F_SENSOR_ERROR:*v=(uint32_t)last_error;return 0;
    case GIGE_F_I2C_ERROR:*v=(uint32_t)sensor->transport_error;return 0;
    case GIGE_F_ERROR_REGISTER:*v=sensor->error_register;return 0;
    case GIGE_F_SENSOR_ID:*v=OV5640_CHIP_ID;return 0;
    case GIGE_F_SENSOR_XCLK:*v=sensor->config.clock.xclk_hz;return 0;
    case GIGE_F_SENSOR_SCLK:*v=Ov5640_ConfigSystemClockHz(&sensor->config);return 0;
    case GIGE_F_SENSOR_HTS:*v=sensor->config.window.hts;return 0;
    case GIGE_F_SENSOR_VTS:*v=sensor->config.window.vts;return 0;
    case GIGE_F_EXPOSURE_MAX_LINES:*v=sensor->config.window.vts-4;return 0;
    case GIGE_F_SENSOR_FPS:*v=Ov5640_FrameRateMilliHz(&sensor->config);return 0;
    case GIGE_F_LINE_PERIOD: {
        uint32_t hz=Ov5640_ConfigSystemClockHz(&sensor->config);
        *v=hz?(uint32_t)((uint64_t)sensor->config.window.hts*1000000000ULL/hz):0;return 0;}
    default:break;
    }
    if(sensor->state<OV5640_CONFIGURED || sensor->state==OV5640_FAULT)return GIGE_STATUS_BUSY;
    switch(a) {
    case GIGE_F_AE_TARGET:*v=(cache[0x3a0f]+cache[0x3a10]+1U)/2U;return 0;
    case GIGE_F_SHARPNESS:*v=cache[0x5302];return 0;
    case GIGE_F_DENOISE:*v=cache[0x5306];return 0;
    case GIGE_F_SHARPNESS_AUTO:*v=(cache[0x5308]&0x40)?0:2;return 0;
    case GIGE_F_DENOISE_AUTO:*v=(cache[0x5308]&0x10)?0:2;return 0;
    case GIGE_F_LUMINANCE:*v=cache[0x56a1];return 0;
    case GIGE_F_EXPOSURE_AUTO:*v=(cache[0x3503]&1)?0:2;break;
    case GIGE_F_GAIN_AUTO:*v=(cache[0x3503]&2)?0:2;break;
    case GIGE_F_EXPOSURE_LINES:*v=((cache[0x3500]&15)<<12)|(cache[0x3501]<<4)|(cache[0x3502]>>4);break;
    case GIGE_F_GAIN:*v=pair(0x350a,3);break;
    case GIGE_F_MAX_EXPOSURE_50:*v=pair(0x3a14,255);break;
    case GIGE_F_MAX_EXPOSURE_60:*v=pair(0x3a02,255);break;
    case GIGE_F_MAX_GAIN:*v=pair(0x3a18,3);break;
    case GIGE_F_NIGHT_MODE:*v=(cache[0x3a00]>>2)&1;break;
    case GIGE_F_BANDING:*v=!(cache[0x3a00]&32)?0:(!(cache[0x3c01]&128)?1:((cache[0x3c00]&4)?50:60));break;
    case GIGE_F_AWB_AUTO:*v=(cache[0x3406]&1)?0:2;break;
    case GIGE_F_WB_RED:*v=pair(0x3400,15);break;
    case GIGE_F_WB_GREEN:*v=pair(0x3402,15);break;
    case GIGE_F_WB_BLUE:*v=pair(0x3404,15);break;
    case GIGE_F_MIRROR:*v=(cache[0x3821]&6)==6;break;
    case GIGE_F_FLIP:*v=(cache[0x3820]&6)==6;break;
    case GIGE_F_TEST_PATTERN:*v=(cache[0x503d]&128)?1+(cache[0x503d]&3):0;break;
    case GIGE_F_TEST_STYLE:*v=(cache[0x503d]>>2)&3;break;
    case GIGE_F_TEST_ROLL:*v=(cache[0x503d]>>6)&1;break;
    case GIGE_F_TEST_TRANSPARENT:*v=(cache[0x503d]>>5)&1;break;
    case GIGE_F_TEST_BW:*v=(cache[0x503d]>>4)&1;break;
    case GIGE_F_ISP0:*v=cache[0x5000]&0xa7;break;
    case GIGE_F_ISP1:*v=cache[0x5001]&0xa7;break;
    case GIGE_F_ISP3:*v=cache[0x5003]&7;break;
    case GIGE_F_GAMMA_VALUE:*v=gamma[S(GAMMA_INDEX)];break;
    case GIGE_F_LENC_VALUE:*v=lenc[S(LENC_INDEX)];break;
    case GIGE_F_REG_VALUE: {
        if(sensor->state==OV5640_STREAMING)return GIGE_STATUS_BUSY;
        uint8_t p;int e=Ov5640_Read(sensor,(uint16_t)S(REG_SELECTOR),&p,1);
        if(e)return status(e);
        cache[S(REG_SELECTOR)]=p;*v=p;break;}
    case GIGE_F_OTP_VALUE:if(!otp_valid)return GIGE_STATUS_BUSY;*v=otp[S(OTP_INDEX)];break;
    default:
        if(a<GIGE_FEATURE_FIRST || a>=GIGE_FEATURE_END)return GIGE_STATUS_INVALID_ADDRESS;
        *v=staged[index_of(a)];break;
    }
    return 0;
}
static uint16_t image_patch(uint16_t r,uint8_t mask,uint8_t value)
{
    /* No raw changes to the transport contract, reset, clocks, OTP or MCU.
     * The remaining chapter7 groups stay inspectable via the named selector. */
    OV5640_FEATURE f;
    if(r>=0x5180 && r<=0x519e)f=OV5640_FEATURE_AWB;
    else if(r>=0x5300 && r<=0x530c)f=OV5640_FEATURE_CIP;
    else if(r>=0x5380 && r<=0x538b)f=OV5640_FEATURE_CMX;
    else if(r>=0x5480 && r<=0x5490)f=OV5640_FEATURE_GAMMA;
    else if(r>=0x5580 && r<=0x558b)f=OV5640_FEATURE_SDE;
    else if(r>=0x5800 && r<=0x583d)f=OV5640_FEATURE_LENC;
    else if(r>=0x4000 && r<=0x4033)f=OV5640_FEATURE_BLC;
    else return GIGE_STATUS_WRITE_PROTECT;
    if(!mask || (value&~mask) || (r==0x5480 && (mask&~3U)) ||
       (r==0x5588 && (mask&128U)) || (r==0x558a && (mask&~1U)))return GIGE_STATUS_INVALID_PARAM;
    OV5640_PATCH p={r,mask,value};return status(Ov5640_ApplyFeature(sensor,f,&p,1));
}
uint16_t GigeSensorFeatures_Write(uint32_t a,uint32_t v)
{
    if(!sensor || a<GIGE_FEATURE_FIRST || a>=GIGE_FEATURE_END || (a&3))return GIGE_STATUS_INVALID_ADDRESS;
    const GigeFeatureDescriptor *d=&gige_feature_descriptors[index_of(a)];
    if(!d->writable)return GIGE_STATUS_WRITE_PROTECT;
    if((int64_t)v<d->minimum || (int64_t)v>d->maximum)return GIGE_STATUS_INVALID_PARAM;
    if(sensor->state<OV5640_CONFIGURED || sensor->state==OV5640_FAULT)return GIGE_STATUS_BUSY;
    if(d->stopped && sensor->state==OV5640_STREAMING)return GIGE_STATUS_BUSY;
    int e=0;
    switch(a) {
    case GIGE_F_AE_TARGET:e=Ov5640_SetAutoExposureTarget(sensor,(uint8_t)v);break;
    case GIGE_F_SHARPNESS:case GIGE_F_SHARPNESS_AUTO:
        if(a==GIGE_F_SHARPNESS_AUTO && v!=0 && v!=2)return GIGE_STATUS_INVALID_PARAM;
        e=Ov5640_SetCip(sensor,0,a==GIGE_F_SHARPNESS_AUTO?v!=0:!(cache[0x5308]&0x40),a==GIGE_F_SHARPNESS?(uint8_t)v:cache[0x5302]);break;
    case GIGE_F_DENOISE:case GIGE_F_DENOISE_AUTO:
        if(a==GIGE_F_DENOISE_AUTO && v!=0 && v!=2)return GIGE_STATUS_INVALID_PARAM;
        e=Ov5640_SetCip(sensor,1,a==GIGE_F_DENOISE_AUTO?v!=0:!(cache[0x5308]&0x10),a==GIGE_F_DENOISE?(uint8_t)v:cache[0x5306]);break;
    case GIGE_F_EXPOSURE_AUTO:
    case GIGE_F_GAIN_AUTO:
        if(v!=0 && v!=2)return GIGE_STATUS_INVALID_PARAM;
        e=Ov5640_SetAutoExposure(sensor,a==GIGE_F_EXPOSURE_AUTO?v!=0:!(cache[0x3503]&1),
                                a==GIGE_F_GAIN_AUTO?v!=0:!(cache[0x3503]&2));break;
    case GIGE_F_EXPOSURE_LINES: {
        if(!(cache[0x3503]&1))return GIGE_STATUS_ACCESS_DENIED;
        if(v>(uint32_t)sensor->config.window.vts-4)return GIGE_STATUS_INVALID_PARAM;
        OV5640_PATCH p[]={{0x3500,15,(uint8_t)(v>>12)},{0x3501,255,(uint8_t)(v>>4)},{0x3502,255,(uint8_t)(v<<4)}};
        e=Ov5640_GroupUpdate(sensor,0,p,3);break;}
    case GIGE_F_GAIN: {
        if(!(cache[0x3503]&2))return GIGE_STATUS_ACCESS_DENIED;
        OV5640_PATCH p[]={{0x350a,3,(uint8_t)(v>>8)},{0x350b,255,(uint8_t)v}};
        e=Ov5640_GroupUpdate(sensor,0,p,2);break;}
    case GIGE_F_MAX_EXPOSURE_50:
    case GIGE_F_MAX_EXPOSURE_60:
    case GIGE_F_MAX_GAIN:
    case GIGE_F_NIGHT_MODE:
        if((a==GIGE_F_MAX_EXPOSURE_50 || a==GIGE_F_MAX_EXPOSURE_60) &&
           v>(uint32_t)sensor->config.window.vts-4)return GIGE_STATUS_INVALID_PARAM;
        e=Ov5640_SetExposureLimits(sensor,a==GIGE_F_MAX_EXPOSURE_50?(uint16_t)v:(uint16_t)pair(0x3a14,255),
           a==GIGE_F_MAX_EXPOSURE_60?(uint16_t)v:(uint16_t)pair(0x3a02,255),
           a==GIGE_F_MAX_GAIN?(uint16_t)v:(uint16_t)pair(0x3a18,3),a==GIGE_F_NIGHT_MODE?(int)v:((cache[0x3a00]>>2)&1));break;
    case GIGE_F_BANDING:e=Ov5640_SetBanding(sensor,v);break;
    case GIGE_F_AWB_AUTO:
        if(v!=0 && v!=2)return GIGE_STATUS_INVALID_PARAM;
        if(v && (sensor->config.format==OV5640_RAW8 || sensor->config.format==OV5640_RAW10))return GIGE_STATUS_ACCESS_DENIED;
        /* Before the first frame the auto-WB registers may still be zero.
         * Seed unity only for those uninitialized channels when entering manual. */
        e=Ov5640_SetWhiteBalance(sensor,v!=0,(uint16_t)(pair(0x3400,15)?pair(0x3400,15):1024),
            (uint16_t)(pair(0x3402,15)?pair(0x3402,15):1024),(uint16_t)(pair(0x3404,15)?pair(0x3404,15):1024));break;
    case GIGE_F_WB_RED:case GIGE_F_WB_GREEN:case GIGE_F_WB_BLUE:
        if(!(cache[0x3406]&1))return GIGE_STATUS_ACCESS_DENIED;
        e=Ov5640_SetWhiteBalance(sensor,0,a==GIGE_F_WB_RED?(uint16_t)v:(uint16_t)pair(0x3400,15),
            a==GIGE_F_WB_GREEN?(uint16_t)v:(uint16_t)pair(0x3402,15),a==GIGE_F_WB_BLUE?(uint16_t)v:(uint16_t)pair(0x3404,15));break;
    case GIGE_F_MIRROR:case GIGE_F_FLIP:
        e=Ov5640_SetOrientation(sensor,a==GIGE_F_MIRROR?(int)v:sensor->config.mirror,a==GIGE_F_FLIP?(int)v:sensor->config.flip);break;
    case GIGE_F_TEST_PATTERN:case GIGE_F_TEST_STYLE:case GIGE_F_TEST_ROLL:case GIGE_F_TEST_TRANSPARENT:case GIGE_F_TEST_BW: {
        uint8_t old=cache[0x503d];
        e=Ov5640_SetTestPattern(sensor,a==GIGE_F_TEST_PATTERN?v!=0:(old>>7)&1,
            a==GIGE_F_TEST_PATTERN?(v?(uint8_t)(v-1):0):(old&3),a==GIGE_F_TEST_STYLE?(uint8_t)v:(old>>2)&3,
            a==GIGE_F_TEST_ROLL?(int)v:(old>>6)&1,a==GIGE_F_TEST_TRANSPARENT?(int)v:(old>>5)&1,a==GIGE_F_TEST_BW?(int)v:(old>>4)&1);break;}
    case GIGE_F_ISP0:case GIGE_F_ISP1:case GIGE_F_ISP3:
        e=Ov5640_SetIsp(sensor,a==GIGE_F_ISP0?(uint8_t)v:cache[0x5000]&0xa7,
            a==GIGE_F_ISP1?(uint8_t)v:cache[0x5001]&0xa7,a==GIGE_F_ISP3?(uint8_t)v:cache[0x5003]&7);break;
    case GIGE_F_FX_APPLY: {
        if(v!=1)return GIGE_STATUS_INVALID_PARAM;
        OV5640_EFFECT fx={(uint8_t)S(FX_ENABLE),(uint8_t)S(FX_COS),(uint8_t)S(FX_SIN),(uint8_t)S(FX_U),(uint8_t)S(FX_V),
            (uint8_t)S(FX_Y),(uint8_t)S(FX_CONTRAST),(uint8_t)S(FX_BRIGHTNESS),(uint8_t)S(FX_SIGNS),
            (uint8_t)S(FX_SATURATION),(uint8_t)S(FX_UV1),(uint16_t)S(FX_UV2)};
        e=Ov5640_SetEffect(sensor,&fx);break;}
    case GIGE_F_GAMMA_VALUE:gamma[S(GAMMA_INDEX)]=(uint8_t)v;break;
    case GIGE_F_LENC_VALUE:lenc[S(LENC_INDEX)]=(uint8_t)v;break;
    case GIGE_F_GAMMA_APPLY:if(v!=1)return GIGE_STATUS_INVALID_PARAM;e=Ov5640_SetGamma(sensor,gamma,(uint8_t)S(GAMMA_CONTROL));break;
    case GIGE_F_LENC_APPLY:if(v!=1)return GIGE_STATUS_INVALID_PARAM;e=Ov5640_SetLensCorrection(sensor,lenc,(int)S(LENC_ENABLE));break;
    case GIGE_F_DRAW_APPLY:if(v!=1)return GIGE_STATUS_INVALID_PARAM;
        e=Ov5640_SetDrawWindow(sensor,(uint16_t)S(DRAW_LEFT),(uint16_t)S(DRAW_TOP),(uint16_t)S(DRAW_RIGHT),(uint16_t)S(DRAW_BOTTOM),
                               (uint8_t)S(DRAW_U),(uint8_t)S(DRAW_V),(int)S(DRAW_ENABLE));break;
    case GIGE_F_VCM_TARGET:case GIGE_F_VCM_SLEW:case GIGE_F_VCM_DIVIDER:case GIGE_F_VCM_CURRENT:case GIGE_F_VCM_RIH:case GIGE_F_VCM_POWERDOWN:
        if(!GIGE_UNIT_VCM_AVAILABLE)return GIGE_STATUS_NOT_IMPLEMENTED;
        staged[index_of(a)]=v;break;
    case GIGE_F_VCM_APPLY:if(!GIGE_UNIT_VCM_AVAILABLE)return GIGE_STATUS_NOT_IMPLEMENTED;
        if(v!=1)return GIGE_STATUS_INVALID_PARAM;
        e=Ov5640_SetVcm(sensor,(uint16_t)S(VCM_TARGET),(uint8_t)S(VCM_SLEW),(uint16_t)S(VCM_DIVIDER),
                         (uint8_t)S(VCM_CURRENT),(int)S(VCM_RIH),(int)S(VCM_POWERDOWN));break;
    case GIGE_F_REG_SELECTOR:if(!known_register((uint16_t)v))return GIGE_STATUS_INVALID_PARAM;staged[index_of(a)]=v;break;
    case GIGE_F_PATCH_APPLY:if(v!=1)return GIGE_STATUS_INVALID_PARAM;return image_patch((uint16_t)S(REG_SELECTOR),(uint8_t)S(PATCH_MASK),(uint8_t)S(PATCH_VALUE));
    case GIGE_F_OTP_READ:if(v!=1)return GIGE_STATUS_INVALID_PARAM;
        e=Ov5640_ReadOtp(sensor,otp);if(!e)otp_valid=1;break;
    default:staged[index_of(a)]=v;break;
    }
    return status(e);
}

int GigeSensorFeatures_Export(GigePreset *p)
{
    if(!sensor || !p || sensor->state!=OV5640_CONFIGURED)return -1;
    uint32_t previous[GIGE_FEATURE_COUNT];uint8_t old_gamma[16],old_lenc[62];
    memcpy(previous,staged,sizeof(staged));memcpy(old_gamma,gamma,16);memcpy(old_lenc,lenc,62);
    int result=GigeSensorFeatures_Prepare();
    if(!result) {
        for(unsigned i=0;i<GIGE_PRESET_COUNT;++i)if(GigeSensorFeatures_Read(gige_preset_addresses[i],&p->values[i]))result=-1;
        memcpy(p->gamma,gamma,16);memcpy(p->lenc,lenc,62);
        const uint16_t regs[]={0x3a0f,0x3a10,0x3a11,0x3a1b,0x3a1e,0x3a1f};
        for(unsigned i=0;i<6;++i)p->ae_thresholds[i]=cache[regs[i]];
        /* Automatic channels can be uninitialized before the first frame. */
        if(p->values[GIGE_PRESET_EXPOSURE_LINES]<1)p->values[GIGE_PRESET_EXPOSURE_LINES]=1;
        if(p->values[GIGE_PRESET_EXPOSURE_LINES]>(uint32_t)sensor->config.window.vts-4)p->values[GIGE_PRESET_EXPOSURE_LINES]=sensor->config.window.vts-4;
        if(p->values[GIGE_PRESET_GAIN]<16)p->values[GIGE_PRESET_GAIN]=16;
        for(unsigned i=GIGE_PRESET_WB_RED;i<=GIGE_PRESET_WB_BLUE;++i)if(!p->values[i])p->values[i]=1024;
    }
    memcpy(staged,previous,sizeof(staged));memcpy(gamma,old_gamma,16);memcpy(lenc,old_lenc,62);
    return result;
}
uint16_t GigeSensorFeatures_Import(const GigePreset *p)
{
    if(!sensor || sensor->state!=OV5640_CONFIGURED)return GIGE_STATUS_BUSY;
    if(GigePreset_Validate(p) || p->values[GIGE_PRESET_EXPOSURE_LINES]>(uint32_t)sensor->config.window.vts-4 ||
       p->values[GIGE_PRESET_MAX_EXPOSURE_50]>(uint32_t)sensor->config.window.vts-4 ||
       p->values[GIGE_PRESET_MAX_EXPOSURE_60]>(uint32_t)sensor->config.window.vts-4)return GIGE_STATUS_INVALID_PARAM;
    uint16_t e=GigeSensorFeatures_Write(GIGE_F_EXPOSURE_AUTO,0);
    if(!e)e=GigeSensorFeatures_Write(GIGE_F_GAIN_AUTO,0);
    if(!e)e=GigeSensorFeatures_Write(GIGE_F_AWB_AUTO,0);
    for(unsigned i=0;!e && i<GIGE_PRESET_COUNT;++i) {
        uint32_t a=gige_preset_addresses[i];
        if(a==GIGE_F_EXPOSURE_AUTO || a==GIGE_F_GAIN_AUTO || a==GIGE_F_AWB_AUTO || a==GIGE_F_ISP0 || a==GIGE_F_ISP1 || a==GIGE_F_ISP3)continue;
        e=GigeSensorFeatures_Write(a,p->values[i]);
    }
    if(!e)e=GigeSensorFeatures_Write(GIGE_F_FX_APPLY,1);
    if(!e)e=GigeSensorFeatures_Write(GIGE_F_DRAW_APPLY,1);
    for(unsigned i=0;!e && i<16;++i){e=GigeSensorFeatures_Write(GIGE_F_GAMMA_INDEX,i);if(!e)e=GigeSensorFeatures_Write(GIGE_F_GAMMA_VALUE,p->gamma[i]);}
    if(!e)e=GigeSensorFeatures_Write(GIGE_F_GAMMA_APPLY,1);
    for(unsigned i=0;!e && i<62;++i){e=GigeSensorFeatures_Write(GIGE_F_LENC_INDEX,i);if(!e)e=GigeSensorFeatures_Write(GIGE_F_LENC_VALUE,p->lenc[i]);}
    if(!e)e=GigeSensorFeatures_Write(GIGE_F_LENC_APPLY,1);
    if(!e)e=GigeSensorFeatures_Write(GIGE_F_ISP0,p->values[GIGE_PRESET_ISP0]);
    if(!e)e=GigeSensorFeatures_Write(GIGE_F_ISP1,p->values[GIGE_PRESET_ISP1]);
    if(!e)e=GigeSensorFeatures_Write(GIGE_F_ISP3,p->values[GIGE_PRESET_ISP3]);
    const uint16_t regs[]={0x3a0f,0x3a10,0x3a11,0x3a1b,0x3a1e,0x3a1f};OV5640_PATCH patches[6];
    for(unsigned i=0;i<6;++i){patches[i].reg=regs[i];patches[i].mask=255;patches[i].value=p->ae_thresholds[i];}
    if(!e)e=status(Ov5640_GroupUpdate(sensor,0,patches,6));
    /* Older RAW presets may have saved Continuous AWB. The RAW tap does not
     * supply valid AWB statistics; retain saved manual gains instead. */
    if(!e)e=GigeSensorFeatures_Write(GIGE_F_AWB_AUTO,
        (sensor->config.format==OV5640_RAW8 || sensor->config.format==OV5640_RAW10)?0:p->values[GIGE_PRESET_AWB_AUTO]);
    if(!e)e=GigeSensorFeatures_Write(GIGE_F_GAIN_AUTO,p->values[GIGE_PRESET_GAIN_AUTO]);
    if(!e)e=GigeSensorFeatures_Write(GIGE_F_EXPOSURE_AUTO,p->values[GIGE_PRESET_EXPOSURE_AUTO]);
    return e;
}
