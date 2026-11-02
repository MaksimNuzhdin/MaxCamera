#include "Ov5640.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct
{
    uint8_t reg[65536];
    int calls, fail_at, writes,failed_write;
    uint16_t failed_reg;
    uint64_t delayed;
    int otp_busy, mailbox_busy;
} MOCK;
static MOCK m;
static int read_bus(void *u, uint8_t a, uint16_t r, uint8_t *p, uint16_t n)
{
    MOCK *b = (MOCK *)u;
    assert(a == 0x3c);
    if (++b->calls == b->fail_at){b->failed_reg=r;b->failed_write=0;return -5;}
    if (r == 0x3d21 && b->otp_busy)
    {
        *p = 128;
        return 0;
    }
    if (r == 0x3023 && b->mailbox_busy)
    {
        *p = 1;
        return 0;
    }
    memcpy(p, b->reg + r, n);
    return 0;
}
static int write_bus(void *u, uint8_t a, uint16_t r, const uint8_t *p, uint16_t n)
{
    MOCK *b = (MOCK *)u;
    assert(a == 0x3c);
    assert(n <= 256);
    if (++b->calls == b->fail_at){b->failed_reg=r;b->failed_write=1;return -5;}
    memcpy(b->reg + r, p, n);
    b->writes++;
    if (r == 0x3008 && p[0] == 0x82)
        b->reg[r] = 0x02;
    if (r == 0x3022)
        b->reg[0x3023] = 0;
    return 0;
}
static void delay_bus(void *u, uint32_t us) { ((MOCK *)u)->delayed += us; }
static void setup(OV5640 *s)
{
    OV5640_IO io = {&m, read_bus, write_bus, delay_bus};
    memset(&m, 0, sizeof(m));
    m.reg[0x300a] = 0x56;
    m.reg[0x300b] = 0x40;
    assert(Ov5640_Bind(s, &io, 0x3c) == 0);
}
static void configured(OV5640 *s)
{
    OV5640_CONFIG c;
    setup(s);
    assert(Ov5640_Init(s) == 0);
    Ov5640_DefaultConfig(&c);
    assert(Ov5640_Configure(s, &c) == 0);
}
static unsigned word(unsigned r) { return (m.reg[r] << 8) | m.reg[r + 1]; }
static void raw8_profile(void)
{
    OV5640 s;
    OV5640_CONFIG c;
    setup(&s);
    assert(Ov5640_InitWithClock(&s,12000000)==0);
    Ov5640_Raw8VgaConfig(&c);
    assert(c.format==OV5640_RAW8 && Ov5640_FrameRateMilliHz(&c)==8933);
    assert(Ov5640_Configure(&s,&c)==0);
    assert(word(0x3804)==671 && word(0x3806)==487);
    assert(word(0x3808)==640 && word(0x380a)==480);
    assert(word(0x380c)==2844 && word(0x380e)==1968);
    assert(word(0x3810)==16 && word(0x3812)==4);
    assert(m.reg[0x3814]==0x11 && m.reg[0x3815]==0x11);
    assert(m.reg[0x4300]==0 && (m.reg[0x501f]&7)==3);
    /* Same PLL settings, different bit divider: reprogram on both transitions. */
    Ov5640_Raw10VgaConfig(&c);
    assert(c.format==OV5640_RAW10 && c.window.width==640 && c.window.hts==2844);
    assert(Ov5640_ConfigSystemClockHz(&c)==40000000);
    assert(Ov5640_FrameRateMilliHz(&c)==7146);
    assert(Ov5640_Configure(&s,&c)==0);
    assert((m.reg[0x3034]&15)==10 && m.reg[0x4300]==0 && (m.reg[0x501f]&7)==3);
    assert(m.reg[0x4837]==20); /* MIPI byte timing is independent of RAW depth. */
    assert(Ov5640_SetBanding(&s,50)==0 && word(0x3a08)==140);
    c.format=OV5640_RAW8;
    assert(Ov5640_Configure(&s,&c)==0 && (m.reg[0x3034]&15)==8 && m.reg[0x4837]==20);
    int writes=m.writes;
    c.window.width=638; /* RAW DPC cannot scale a 640-pixel sampled window. */
    assert(Ov5640_Configure(&s,&c)==OV5640_E_ARGUMENT && m.writes==writes);
    Ov5640_Raw8VgaConfig(&c);c.window.x_offset=15;
    assert(Ov5640_Configure(&s,&c)==OV5640_E_ARGUMENT && m.writes==writes);
    Ov5640_Raw8VgaConfig(&c);c.window.y_offset=3;
    assert(Ov5640_Configure(&s,&c)==OV5640_E_ARGUMENT && m.writes==writes);
    Ov5640_Raw8VgaConfig(&c);c.window.x_increment=0x31;
    assert(Ov5640_Configure(&s,&c)==OV5640_E_ARGUMENT && m.writes==writes);
    Ov5640_DefaultConfig(&c);c.clock.xclk_hz=12000000;c.clock.multiplier=200;
    c.format=OV5640_RAW8; /* Former scaled full-array VGA profile is rejected. */
    assert(Ov5640_Configure(&s,&c)==OV5640_E_ARGUMENT && m.writes==writes);
    c.format=OV5640_Y8;
    assert(Ov5640_Configure(&s,&c)==0 && m.reg[0x4300]==0x10);
    assert(m.reg[0x5001]&0x20); /* ISP-scaled formats remain supported. */
    Ov5640_Raw10FullConfig(&c);
    assert(c.window.width==2592 && c.window.height==1944 && c.window.x_end==2623 && c.window.y_end==1951);
    assert(Ov5640_FrameRateMilliHz(&c)==7146 && Ov5640_Configure(&s,&c)==0);
    assert(word(0x3808)==2592 && word(0x380a)==1944 && (m.reg[0x3034]&15)==10);
    Ov5640_Raw8FullConfig(&c);
    assert(Ov5640_FrameRateMilliHz(&c)==8933 && Ov5640_Configure(&s,&c)==0);
    assert(word(0x3808)==2592 && word(0x380a)==1944 && (m.reg[0x3034]&15)==8);
}
static void jpeg_fixed_frame(void)
{
    OV5640 s;OV5640_CONFIG c;
    configured(&s);int before=m.writes;
    assert(Ov5640_SetJpegFixedFrame(&s,1280,480,0)==OV5640_E_STATE && m.writes==before);
    c=s.config;c.format=OV5640_JPEG;
    assert(Ov5640_Configure(&s,&c)==0);
    m.reg[0x4600]=0x81;m.reg[0x4713]=0xf8;
    assert(Ov5640_SetJpegFixedFrame(&s,1280,480,0)==0);
    assert(word(0x4602)==1280 && word(0x4604)==480 && m.reg[0x460d]==0);
    assert(m.reg[0x4600]==0xa1 && m.reg[0x4713]==0xfa);
    before=m.writes;
    assert(Ov5640_SetJpegFixedFrame(&s,0,480,0)==OV5640_E_ARGUMENT && m.writes==before);
    assert(Ov5640_Stream(&s,1)==0);before=m.writes;
    assert(Ov5640_SetJpegFixedFrame(&s,1280,480,0)==OV5640_E_STATE && m.writes==before);
    assert(Ov5640_Stream(&s,0)==0);
    before=m.writes;m.fail_at=m.calls+2;
    assert(Ov5640_SetJpegFixedFrame(&s,1280,480,0)==OV5640_E_IO);
    assert(s.state==OV5640_FAULT && m.failed_reg==0x4602 && m.writes==before);
}
static void mode_transition(void)
{
    OV5640 s;OV5640_CONFIG c;
    configured(&s);c=s.config;c.format=OV5640_JPEG;
    assert(Ov5640_Configure(&s,&c)==0);
    assert(Ov5640_SetJpegFixedFrame(&s,1280,480,0)==0);
    c.format=OV5640_UYVY;
    assert(Ov5640_Configure(&s,&c)==0);
    assert(s.state==OV5640_CONFIGURED && m.reg[0x3008]==0x42);
    assert(!(m.reg[0x4600]&0x20) && (m.reg[0x4713]&7)==3);
    assert(Ov5640_Stream(&s,1)==0 && m.reg[0x3008]==0x02 && m.reg[0x4202]==0);
}
int main(void)
{
    OV5640 s;
    OV5640_CONFIG c;
    OV5640_STATUS status;
    uint16_t id;
    uint8_t data[32];
    int before, i, total;
    OV5640_PATCH p = {0x5580, 0x40, 0x40};
    OV5640_EFFECT effect;
    raw8_profile();
    jpeg_fixed_frame();
    mode_transition();
    configured(&s);Ov5640_DefaultConfig(&c);
    c.format=OV5640_RGB555;assert(Ov5640_Configure(&s,&c)==0 && m.reg[0x4300]==0x81 && (m.reg[0x501f]&7)==1);
    c.format=OV5640_RGB444;assert(Ov5640_Configure(&s,&c)==0 && m.reg[0x4300]==0xa1 && (m.reg[0x501f]&7)==1);
    c.format=OV5640_YUV444;assert(Ov5640_Configure(&s,&c)==0 && m.reg[0x4300]==0x20 && (m.reg[0x501f]&7)==0);
    c.format=OV5640_YUV420;assert(Ov5640_Configure(&s,&c)==0 && m.reg[0x4300]==0x5f && (m.reg[0x501f]&7)==0);
    before=m.writes;c.interface=OV5640_DVP;
    assert(Ov5640_Configure(&s,&c)==OV5640_E_ARGUMENT && m.writes==before);
    c.interface=OV5640_MIPI;c.window.height=479;
    assert(Ov5640_Configure(&s,&c)==OV5640_E_ARGUMENT && m.writes==before);
    setup(&s);
    assert(Ov5640_Stream(&s, 1) == OV5640_E_STATE);
    m.reg[0x300b] = 0x41;
    assert(Ov5640_Init(&s) == OV5640_E_ID);
    assert(m.writes == 0);
    m.reg[0x300b] = 0x40;
    assert(Ov5640_Probe(&s, &id) == 0 && id == 0x5640);
    assert(Ov5640_Init(&s) == 0 && s.state == OV5640_READY && m.reg[0x3008] == 0x02 &&
           m.reg[0x4202] == 0x0f);
    Ov5640_DefaultConfig(&c);
    assert(Ov5640_Configure(&s, &c) == 0);
    assert(s.state == OV5640_CONFIGURED);
    assert(word(0x3808) == 640 && word(0x380a) == 480 && word(0x380c) == 1600 &&
           word(0x380e) == 998);
    assert(m.reg[0x4300] == 0x30 && m.reg[0x3036] == 92 && m.reg[0x4837] == 21);
    assert(Ov5640_SystemClockHz(&c.clock) == 47916666);
    assert(Ov5640_FrameRateMilliHz(&c) == 30007);
    assert(word(0x3a08) == 299 && word(0x3a0a) == 249);
    c.window.width=2592;c.window.height=1944;c.window.hts=2844;c.window.vts=3375;
    c.window.x_increment=c.window.y_increment=0x11;
    assert(Ov5640_Configure(&s,&c)==0);
    assert(word(0x3808)==2592 && word(0x380a)==1944 && word(0x380c)==2844 && word(0x380e)==3375);
    assert(m.reg[0x3814]==0x11 && m.reg[0x3618]==4 && m.reg[0x3708]==0x21 && m.reg[0x4004]==6);
    Ov5640_DefaultConfig(&c);assert(Ov5640_Configure(&s,&c)==0);
    assert(m.reg[0x3814]==0x31 && m.reg[0x3618]==0 && m.reg[0x3708]==0x64 && m.reg[0x4004]==2);
    /* Native luminance must select the ISP, not Bayer bypass. Switching back
     * must restore the formatter and preserve unrelated mux register bits. */
    Ov5640_Raw8VgaConfig(&c);c.clock=s.config.clock;
    assert(Ov5640_Configure(&s,&c)==0);
    m.reg[0x501f]|=0xa0;
    c.format=OV5640_Y8;assert(Ov5640_Configure(&s,&c)==0);
    assert(m.reg[0x4300]==0x10 && m.reg[0x501f]==0xa0);
    assert(s.config.format==OV5640_Y8 && s.state==OV5640_CONFIGURED);
    assert(m.reg[0x4202]==0x0f); /* Configuration never starts frame output. */
    c.format=OV5640_UYVY;assert(Ov5640_Configure(&s,&c)==0);
    assert(m.reg[0x4300]==0x32 && (m.reg[0x501f]&7)==0);
    c.format=(OV5640_FORMAT)OV5640_FORMAT_COUNT;before=m.writes;
    assert(Ov5640_Configure(&s,&c)==OV5640_E_ARGUMENT && m.writes==before);
    Ov5640_DefaultConfig(&c);assert(Ov5640_Configure(&s,&c)==0);
    before = m.writes;
    c.clock.xclk_hz = 24000000;
    assert(Ov5640_Configure(&s, &c) == OV5640_E_ARGUMENT && m.writes == before);
    Ov5640_DefaultConfig(&c);
    before = m.writes;
    c.window.width = 641;
    assert(Ov5640_Configure(&s, &c) == OV5640_E_ARGUMENT);
    assert(m.writes == before);
    Ov5640_DefaultConfig(&c);
    c.clock.prediv = 0;
    assert(Ov5640_Configure(&s, &c) == OV5640_E_ARGUMENT);
    assert(Ov5640_SetExposure(&s, 400, 0x123) == 0);
    assert(m.reg[0x3500] == 0 && m.reg[0x3501] == 25 && m.reg[0x3502] == 0);
    assert(word(0x350a) == 0x123 && m.reg[0x3503] == 3);
    before = m.writes;
    assert(Ov5640_SetExposure(&s, 995, 16) == OV5640_E_RANGE && m.writes == before);
    assert(Ov5640_SetWhiteBalance(&s, 0, 0x400, 0x500, 0x600) == 0 && word(0x3404) == 0x600);
    assert(Ov5640_GetStatus(&s, &status) == 0 && status.exposure_lines == 400 &&
           status.gain16 == 0x123 && status.manual_awb == 1);
    m.reg[0x3820] = 0x41;
    assert(Ov5640_SetOrientation(&s, 1, 1) == 0 && m.reg[0x3820] == 0x47);
    memset(&effect, 0, sizeof(effect));
    effect.enable_bits = 0x42;
    effect.u = 64;
    effect.v = 64;
    effect.manual_saturation = 1;
    assert(Ov5640_SetEffect(&s, &effect) == 0 && m.reg[0x5580] == 0x42 && m.reg[0x5588] == 0x40);
    assert(Ov5640_SetTestPattern(&s, 1, 2, 3, 1, 0, 1) == 0 && m.reg[0x503d] == 0xde);
    assert(Ov5640_SetJpegQuality(&s, 64) == OV5640_E_RANGE);
    m.reg[0x4407] = 128;
    assert(Ov5640_SetJpegQuality(&s, 10) == 0 && m.reg[0x4407] == 138);
    assert(Ov5640_SetVcm(&s, 0x2ab, 5, 0x456, 3, 1, 0) == 0 && m.reg[0x3602] == 0xb5 &&
           (m.reg[0x3603] & 0xbf) == 0x2a);
    assert(Ov5640_SetFrexExposure(&s, 0x123456, 0xabcde, 3) == 0 && m.reg[0x3b01] == 0x12 &&
           word(0x3b04) == 0x3456 && m.reg[0x3b06] == 0x3e && word(0x3b0b) == 0xabcd);
    assert(Ov5640_Stream(&s, 1) == 0 && s.state == OV5640_STREAMING && m.reg[0x300e] == 0x45);
    assert(Ov5640_Configure(&s, &c) == OV5640_E_ARGUMENT);
    Ov5640_DefaultConfig(&c);
    assert(Ov5640_Configure(&s, &c) == OV5640_E_STATE);
    assert(Ov5640_GroupUpdate(&s, 0, &p, 1) == 0 && m.reg[0x3212] == 0xa0);
    assert(Ov5640_GroupUpdate(&s, 3, &p, 1) == OV5640_E_ARGUMENT);
    m.reg[0x3213] = 0x20;
    assert(Ov5640_GroupUpdate(&s, 0, &p, 1) == OV5640_E_STATE);
    m.reg[0x3213] = 0;
    assert(Ov5640_Stream(&s, 0) == 0 && s.state == OV5640_CONFIGURED && m.reg[0x3008] == 0x42 &&
           m.reg[0x300e] == 0x40);
    assert(Ov5640_ReadOtp(&s, data) == 0 && m.reg[0x3d20] == 0);
    m.otp_busy = 1;
    before = (int)m.delayed;
    assert(Ov5640_ReadOtp(&s, data) == OV5640_E_TIMEOUT);
    assert(m.delayed - (unsigned)before == 105000);
    m.otp_busy = 0;
    assert(Ov5640_ApplyFeature(&s, OV5640_FEATURE_SDE, &p, 1) == 0);
    p.reg = 0x3d20;
    assert(Ov5640_ApplyFeature(&s, OV5640_FEATURE_SDE, &p, 1) == OV5640_E_ARGUMENT);
    {
        uint8_t fw[513];
        memset(fw, 0x69, sizeof(fw));
        assert(Ov5640_LoadMcu(&s, fw, 513) == 0 && m.reg[0x8200] == 0x69 && !(m.reg[0x3000] & 32));
    }
    memset(data, 0, 5);
    assert(Ov5640_McuCommand(&s, 3, data, 5000) == 0);
    m.mailbox_busy = 1;
    before = m.writes;
    assert(Ov5640_McuCommand(&s, 3, data, 5000) == OV5640_E_TIMEOUT && before == m.writes);
    m.mailbox_busy = 0;
    /* Inject an error at EVERY transaction in configuration. A partial config
     * must never be streamable, even if the failure occurred on a read. */
    setup(&s);
    assert(Ov5640_Init(&s) == 0);
    before = m.calls;
    assert(Ov5640_Configure(&s, &c) == 0);
    total = m.calls - before;
    for (i = 1; i <= total; ++i)
    {
        setup(&s);
        assert(Ov5640_Init(&s) == 0);
        m.fail_at = m.calls + i;
        unsigned verified_before=s.verified_power_nacks;
        OV5640_RESULT result=Ov5640_Configure(&s,&c);
        if(result==OV5640_OK) {
            assert(m.failed_reg==0x3008 && m.failed_write);
            assert(s.verified_power_nacks==verified_before+1);
        } else {
            assert(result==OV5640_E_IO && s.state==OV5640_FAULT);
            assert(s.transport_error==-5 && Ov5640_Stream(&s,1)==OV5640_E_STATE);
        }
    }
    configured(&s);
    before = m.calls;
    assert(Ov5640_SetExposure(&s, 100, 16) == 0);
    total = m.calls - before;
    for (i = 7; i <= total; ++i)
    {
        configured(&s);
        m.fail_at = m.calls + i;
        assert(Ov5640_SetExposure(&s, 100, 16) == OV5640_E_IO);
        assert(s.state == OV5640_FAULT);
    }
    setup(&s);
    m.fail_at = 3; /* two separately addressed ID bytes, then reset NACK */
    assert(Ov5640_Init(&s) == 0 && s.state == OV5640_READY);
    assert(Ov5640_ProgramOtp(&s, data, 2100, OV5640_OTP_CONFIRM) == OV5640_E_STATE);
    configured(&s);
    m.fail_at = m.calls + 2; /* power readback must still be mandatory */
    assert(Ov5640_Stream(&s, 1) == OV5640_E_IO && s.state == OV5640_FAULT);
    assert(Ov5640_Init(&s) == 0 && s.state == OV5640_READY);
    {
        const uint32_t clocks[] = {6000000, 12000000, 24000000, 25000000, 27000000};
        unsigned k;
        for (k = 0; k < sizeof(clocks) / sizeof(clocks[0]); ++k)
        {
            setup(&s);
            assert(Ov5640_InitWithClock(&s, clocks[k]) == 0);
            /* PLL programming belongs to Configure, not Init. */
            assert(m.reg[0x3036] == 0);
            assert(m.reg[0x3103] == 3 && m.reg[0x3008] == 2);
        }
    }
    setup(&s);
    assert(Ov5640_Init(&s) == 0);
    total = m.calls;
    for (i = 1; i <= total; ++i)
    {
        setup(&s);
        m.fail_at = i;
        if (i == 3)
            assert(Ov5640_Init(&s) == 0); /* reset-only NACK exception */
        else
        {
            assert(Ov5640_Init(&s) == OV5640_E_IO);
            assert(Ov5640_Stream(&s, 1) == OV5640_E_STATE);
        }
    }
    puts("OV5640: configuration, controls, groups, bounds, timeout and fault injection passed");
    return 0;
}
