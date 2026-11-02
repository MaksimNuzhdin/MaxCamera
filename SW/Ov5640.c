/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "Ov5640.h"
#include <string.h>
#define TRY(x)                                                                                     \
    do                                                                                             \
    {                                                                                              \
        OV5640_RESULT result_ = (x);                                                               \
        if (result_ != OV5640_OK)                                                                  \
            return result_;                                                                        \
    } while (0)
static int bound(const OV5640 *s) { return s && s->io.read && s->io.write && s->io.delay_us; }
static int usable(const OV5640 *s)
{
    return bound(s) && s->state >= OV5640_READY && s->state <= OV5640_STREAMING;
}
static OV5640_RESULT transfer(OV5640 *s, uint16_t r, uint8_t *p, uint16_t n, int write)
{
    int e;
    if (!bound(s) || !p || !n || n > 256U || (uint32_t)r + n > 65536U)
        return OV5640_E_ARGUMENT;
    e = write ? s->io.write(s->io.user, s->address, r, p, n)
              : s->io.read(s->io.user, s->address, r, p, n);
    if (e)
    {
        s->transport_error = e;
        s->error_register = r;
        if (write)
            s->state = OV5640_FAULT;
        return OV5640_E_IO;
    }
    return OV5640_OK;
}
static OV5640_RESULT rd(OV5640 *s, uint16_t r, uint8_t *v) { return transfer(s, r, v, 1, 0); }
static OV5640_RESULT wr(OV5640 *s, uint16_t r, uint8_t v) { return transfer(s, r, &v, 1, 1); }
static OV5640_RESULT mod(OV5640 *s, uint16_t r, uint8_t m, uint8_t v)
{
    uint8_t old;
    TRY(rd(s, r, &old));
    return wr(s, r, (uint8_t)((old & ~m) | (v & m)));
}
static OV5640_RESULT word(OV5640 *s, uint16_t r, uint16_t v, uint8_t mask)
{
    TRY(mod(s, r, mask, (uint8_t)(v >> 8)));
    return wr(s, (uint16_t)(r + 1), (uint8_t)v);
}
static OV5640_RESULT readword(OV5640 *s, uint16_t r, uint16_t *v, uint8_t mask)
{
    uint8_t p[2];
    TRY(rd(s, r, &p[0]));
    TRY(rd(s, (uint16_t)(r + 1), &p[1]));
    *v = (uint16_t)(((p[0] & mask) << 8) | p[1]);
    return OV5640_OK;
}
static OV5640_RESULT failure(OV5640 *s, OV5640_RESULT e)
{ /* A partial multi-register change is not rolled back or retried. */
    if (e != OV5640_OK)
    {
        uint16_t r = s->error_register;
        int t = s->transport_error;
        (void)wr(s, 0x3008, 0x42);
        s->state = OV5640_FAULT;
        s->error_register = r;
        s->transport_error = t;
    }
    return e;
}
OV5640_RESULT Ov5640_Bind(OV5640 *s, const OV5640_IO *io, uint8_t a)
{
    if (!s || !io || !io->read || !io->write || !io->delay_us || a < 8 || a > 0x77)
        return OV5640_E_ARGUMENT;
    memset(s, 0, sizeof(*s));
    s->io = *io;
    s->address = a;
    s->state = OV5640_BOUND;
    return OV5640_OK;
}
OV5640_RESULT Ov5640_Read(OV5640 *s, uint16_t r, uint8_t *p, uint16_t n)
{
    return transfer(s, r, p, n, 0);
}
OV5640_RESULT Ov5640_Write(OV5640 *s, uint16_t r, const uint8_t *p, uint16_t n)
{
    OV5640_RESULT e;
    if (!bound(s))
        return OV5640_E_ARGUMENT;
    if (s->state == OV5640_STREAMING)
        return OV5640_E_STATE;
    e = transfer(s, r, (uint8_t *)p, n, 1);
    if (e == OV5640_OK)
        s->state = OV5640_BOUND;
    return e;
}
OV5640_RESULT Ov5640_Probe(OV5640 *s, uint16_t *id)
{
    if (!id)
        return OV5640_E_ARGUMENT;
    TRY(readword(s, 0x300a, id, 0xff));
    return *id == OV5640_CHIP_ID ? OV5640_OK : OV5640_E_ID;
}
/* Power transitions may complete before SCCB acknowledges the command.
 * Never retry. Accept a NACK ONLY when the requested state and chip ID can
 * subsequently be read back. All other register NACKs remain fatal. */
static OV5640_RESULT power_state(OV5640 *s, uint8_t value)
{
    OV5640_STATE previous = s->state;
    OV5640_RESULT e = wr(s, 0x3008, value), check;
    uint8_t control = 0;
    uint16_t id = 0;
    int nack = e == OV5640_E_IO && s->transport_error == -5;
    if (e != OV5640_OK && !nack)
        return e;
    s->io.delay_us(s->io.user, 100000);
    check = rd(s, 0x3008, &control);
    if (check)
        return failure(s, check);
    check = Ov5640_Probe(s, &id);
    if (check)
        return failure(s, check);
    if ((value == 0x82 && (control & 0x80)) || (value != 0x82 && (control & 0xc2) != value))
        return failure(s, OV5640_E_IO);
    if (nack)
        ++s->verified_power_nacks;
    s->state = previous;
    s->transport_error = 0;
    s->error_register = 0;
    return OV5640_OK;
}
typedef struct
{
    uint16_t reg;
    uint8_t value;
    uint16_t delay_ms;
} TUNING;
static const TUNING tuning[] = {
#include "Ov5640Tuning.inc"
};
OV5640_RESULT Ov5640_InitWithClock(OV5640 *s, uint32_t xclk_hz)
{
    uint16_t id;
    size_t i;
    OV5640_RESULT e;
    if (!bound(s) || xclk_hz < 6000000U || xclk_hz > 27000000U)
        return OV5640_E_ARGUMENT;
    s->state = OV5640_BOUND;
    s->transport_error = 0;
    s->error_register = 0;
    TRY(Ov5640_Probe(s, &id));
    s->verified_power_nacks = 0;
    TRY(power_state(s, 0x82));
    TRY(power_state(s, 0x42));
    s->state = OV5640_BOUND;
    s->transport_error = 0;
    s->error_register = 0;
    /* Linux: powerup_sequence -> baseline table -> PLL/mode.
     * Gate frame output before the table wakes the sensor core. */
    TRY(wr(s, 0x4202, 0x0f));
    TRY(wr(s, 0x300e, 0x40));
    for (i = 0; i < sizeof(tuning) / sizeof(tuning[0]); ++i)
    {
        e = tuning[i].reg == 0x3008 ? power_state(s, tuning[i].value)
                                    : wr(s, tuning[i].reg, tuning[i].value);
        if (e)
            return failure(s, e);
        if (tuning[i].delay_ms)
            s->io.delay_us(s->io.user, (uint32_t)tuning[i].delay_ms * 1000U);
    }
    TRY(mod(s, 0x3108, 0x3f, 0x01));
    TRY(Ov5640_Probe(s, &id));
    s->config.clock.xclk_hz = xclk_hz;
    s->state = OV5640_READY;
    return OV5640_OK;
}
OV5640_RESULT Ov5640_Init(OV5640 *s) { return Ov5640_InitWithClock(s, 25000000U); }
static uint32_t sclk_root_div(OV5640_FORMAT format)
{

#if MAX_CAMERA_RGB_SLOW_CLOCK || MAX_CAMERA_RGB_FAST_FORMAT_CLOCK
    if(format==OV5640_RGB888)return 4U;
#else
    (void)format;
#endif
    return 2U;
}
static int ten_bit_mipi(OV5640_FORMAT format)
{
#if MAX_CAMERA_RGB10_EXPERIMENT
    if(format==OV5640_RGB888)return 1;
#endif
    return format==OV5640_RAW10;
}
static uint32_t clock_rate(const OV5640_CLOCK *c, uint32_t bit_div2x, uint32_t sclk_div)
{
    if (!c || !c->prediv || !c->sysdiv) return 0;
    return (uint32_t)((uint64_t)c->xclk_hz * c->multiplier /
        (c->prediv * c->sysdiv * bit_div2x * sclk_div));
}
uint32_t Ov5640_SystemClockHz(const OV5640_CLOCK *c)
{
    return c ? clock_rate(c,4U,2U) : 0;
}
uint32_t Ov5640_ConfigSystemClockHz(const OV5640_CONFIG *c)
{
    return c ? clock_rate(&c->clock,ten_bit_mipi(c->format) ? 5U : 4U,sclk_root_div(c->format)) : 0;
}
uint32_t Ov5640_FrameRateMilliHz(const OV5640_CONFIG *c)
{
    if (!c || !c->window.hts || !c->window.vts)
        return 0;
    return (uint32_t)((uint64_t)Ov5640_ConfigSystemClockHz(c) * 1000U /
                      ((uint64_t)c->window.hts * c->window.vts));
}
void Ov5640_DefaultConfig(OV5640_CONFIG *c)
{
    if (!c)
        return;
    memset(c, 0, sizeof(*c));
    c->interface = OV5640_MIPI;
    c->format = OV5640_YUYV;
    c->clock.xclk_hz = 25000000;
    c->clock.prediv = 3;
    c->clock.multiplier = 92;
    c->clock.sysdiv = 2;
    c->clock.mipi_div = 2;
    c->window.x_start = 14;
    c->window.y_start = 16;
    c->window.x_end = 2605;
    c->window.y_end = 1959;
    c->window.width = 640;
    c->window.height = 480;
    c->window.hts = 1600;
    c->window.vts = 998;
    c->window.x_offset = 2;
    c->window.y_offset = 4;
    c->window.x_increment = 0x31;
    c->window.y_increment = 0x31;
    c->lanes = 2;
    c->continuous_clock = 1;
}
void Ov5640_Raw8VgaConfig(OV5640_CONFIG *c)
{
    if (!c) return;
    Ov5640_DefaultConfig(c);
    c->format = OV5640_RAW8;
    c->clock.xclk_hz = 12000000;
    c->clock.multiplier = 200;
    c->clock.mipi_div = 1;
    c->window.x_start = c->window.y_start = 0;
    c->window.x_end = 671; c->window.y_end = 487;
    c->window.x_offset = 16; c->window.y_offset = 4;
    c->window.x_increment = c->window.y_increment = 0x11;
    c->window.hts = 2844; c->window.vts = 1968;
}
void Ov5640_Raw10VgaConfig(OV5640_CONFIG *c)
{
    if (!c) return;
    Ov5640_Raw8VgaConfig(c);
    c->format = OV5640_RAW10;
}
void Ov5640_Raw8FullConfig(OV5640_CONFIG *c)
{
    if(!c)return;
    Ov5640_Raw8VgaConfig(c);
    c->window.width=2592;c->window.height=1944;
    c->window.x_end=2623;c->window.y_end=1951;
}
void Ov5640_Raw10FullConfig(OV5640_CONFIG *c)
{
    if(!c)return;
    Ov5640_Raw8FullConfig(c);
    c->format=OV5640_RAW10;
}
static int increment_ok(uint8_t n) { return (n & 0x11) == 0x11; }
static int valid_config(const OV5640_CONFIG *c)
{
    uint64_t vco;
    const OV5640_WINDOW *w;
    if (!c)
        return 0;
    w = &c->window;
    if (c->interface > OV5640_DVP || (unsigned)c->format >= OV5640_FORMAT_COUNT || c->lanes != 2 ||
        c->virtual_channel > 3 || c->continuous_clock > 1 || c->mirror > 1 || c->flip > 1)
        return 0;
    if (c->clock.xclk_hz < 6000000 || c->clock.xclk_hz > 27000000 || c->clock.prediv < 1 ||
        c->clock.prediv > 8 || c->clock.prediv == 5 || c->clock.prediv == 7 ||
        c->clock.multiplier < 4 || c->clock.multiplier > 252 || c->clock.sysdiv < 1 ||
        c->clock.sysdiv > 16 || (c->clock.multiplier > 127 && (c->clock.multiplier & 1)) ||
        (c->clock.mipi_div != 1 && c->clock.mipi_div != 2))
        return 0;
    if (c->clock.xclk_hz / c->clock.prediv < 4000000U)
        return 0;
    vco = (uint64_t)c->clock.xclk_hz * c->clock.multiplier / c->clock.prediv;
    if (vco < 500000000 || vco > 1000000000)
        return 0;
    if (w->x_start >= w->x_end || w->y_start >= w->y_end || w->x_end > 2623 || w->y_end > 1959 ||
        !w->width || !w->height || w->width > 2592 || w->height > 1944 ||
        w->width > w->x_end - w->x_start + 1 || w->height > w->y_end - w->y_start + 1 ||
        w->hts <= w->width || w->hts > 8191 || w->vts < w->height + 4 || w->x_offset > 4095 ||
        w->y_offset > 2047 || !increment_ok(w->x_increment) || !increment_ok(w->y_increment))
        return 0;
    if (c->format <= OV5640_VYUY && (w->width & 1))
        return 0;
    if (c->format == OV5640_YUV420 &&
        (c->interface != OV5640_MIPI || (w->width & 1) || (w->height & 1)))
        return 0;
    if (c->format == OV5640_RAW8 || c->format == OV5640_RAW10) {
        /* DPC RAW tap is before ISP scaling. Its sampled sensor window minus
         * the two guard offsets must already equal the requested output.
         * Otherwise packets can complete with skewed Bayer rows/CRC errors. */
        uint32_t xs = (w->x_increment >> 4) + (w->x_increment & 15);
        uint32_t ys = (w->y_increment >> 4) + (w->y_increment & 15);
        uint32_t xspan = 2U * (w->x_end - w->x_start + 1U);
        uint32_t yspan = 2U * (w->y_end - w->y_start + 1U);
        if (xspan % xs || yspan % ys ||
            xspan / xs != w->width + 2U * w->x_offset ||
            yspan / ys != w->height + 2U * w->y_offset)
            return 0;
    }
    return 1;
}
static OV5640_RESULT configure(OV5640 *s, const OV5640_CONFIG *c)
{
    const OV5640_WINDOW *w = &c->window;
    uint8_t format = 0x30, mux = 0;
    /* 4837 describes the byte/sample clock before RAW bit-depth packing,
     * whereas frame rate/anti-banding use the format-dependent sensor SCLK. */
    uint32_t timing_div=sclk_root_div(c->format)/2U;
#if MAX_CAMERA_RGB_FAST_FORMAT_CLOCK
    if(c->format==OV5640_RGB888)timing_div=1;
#endif

    uint32_t timing_clock = clock_rate(&c->clock,4U,2U)/timing_div, period;
    uint16_t vals[10];
    const uint8_t masks[10] = {15, 15, 15, 7, 15, 7, 31, 255, 15, 7};
    unsigned int i;
    TRY(wr(s, 0x4202, 0x0f));
    if(c->interface == OV5640_MIPI) TRY(wr(s, 0x300e, 0x40));
#if MAX_CAMERA_RGB_STANDBY_EXPERIMENT
    if(c->format == OV5640_RGB888 || c->format == OV5640_RGB565)
        TRY(power_state(s,0x42));
    else
#endif
        /* Gate the running sensor core while replacing an existing mode.
         * MIPI gating alone leaves its readout/ISP state active across a
         * binning-to-full-array transition. Stream(1) wakes the new mode. */
        TRY(power_state(s, c->interface == OV5640_MIPI &&
                           s->state != OV5640_CONFIGURED ? 0x02 : 0x42));
    TRY(mod(s, 0x3503, 3, 3)); /* AE/AGC off during mode change. */
    const OV5640_CLOCK *old = &s->config.clock;
    if(s->state != OV5640_CONFIGURED || old->xclk_hz != c->clock.xclk_hz ||
       old->prediv != c->clock.prediv || old->multiplier != c->clock.multiplier ||
       old->sysdiv != c->clock.sysdiv || old->mipi_div != c->clock.mipi_div ||
       ten_bit_mipi(s->config.format) != ten_bit_mipi(c->format) ||
       sclk_root_div(s->config.format) != sclk_root_div(c->format)) {
        /* Slow the live clock tree BEFORE changing bit/root dividers. A zero
         * system-divider nibble means /16. Otherwise changing 10-bit to 8-bit
         * first transiently increases SCLK before the intended divider is set. */
        TRY(wr(s, 0x3035, c->clock.mipi_div));
        TRY(mod(s, 0x3034, 0x0f, ten_bit_mipi(c->format) ? 10 : 8));
        TRY(wr(s, 0x3036, c->clock.multiplier));
        TRY(mod(s, 0x3037, 0x1f, (uint8_t)(0x10 | c->clock.prediv)));
        uint8_t roots=sclk_root_div(c->format)==4 ? 0x06 : 0x01;
#if MAX_CAMERA_RGB_FAST_FORMAT_CLOCK
        if(c->format==OV5640_RGB888)roots=0x02;
#endif

        TRY(mod(s, 0x3108, 0x3f, roots));
        TRY(wr(s, 0x3035, (uint8_t)(((c->clock.sysdiv & 15) << 4) | c->clock.mipi_div)));
        period = (2000000000U + timing_clock) / (timing_clock * 2U);
        if (period > 255 || !period)
            return OV5640_E_RANGE;
        TRY(wr(s, 0x4837, (uint8_t)period));
    }
    vals[0] = w->x_start;
    vals[1] = w->y_start;
    vals[2] = w->x_end;
    vals[3] = w->y_end;
    vals[4] = w->width;
    vals[5] = w->height;
    vals[6] = w->hts;
    vals[7] = w->vts;
    vals[8] = w->x_offset;
    vals[9] = w->y_offset;
    for (i = 0; i < 10; ++i)
        TRY(word(s, (uint16_t)(0x3800 + 2 * i), vals[i], masks[i]));
    TRY(wr(s, 0x3814, w->x_increment));
    TRY(wr(s, 0x3815, w->y_increment));
    /* Analog timing differs between binned and full-array readout. */
    TRY(wr(s, 0x3618, w->x_increment == 0x11 ? 0x04 : 0x00));
    TRY(wr(s, 0x3612, 0x29));
    TRY(wr(s, 0x3708, w->x_increment == 0x11 ? 0x21 : 0x64));
    TRY(wr(s, 0x3709, w->x_increment == 0x11 ? 0x12 : 0x52));
    TRY(wr(s, 0x370c, w->x_increment == 0x11 ? 0x00 : 0x03));
    TRY(wr(s, 0x4004, w->y_increment == 0x11 ? 0x06 : 0x02));
    TRY(mod(s, 0x3820, 6, c->flip ? 6 : 0));
    TRY(mod(s, 0x3821, 0x27, (uint8_t)((c->mirror ? 6 : 0) | (w->x_increment != 0x11 ? 1 : 0) |
                                       (c->format == OV5640_JPEG ? 0x20 : 0))));
    if (c->format <= OV5640_VYUY)
        format = (uint8_t)(0x30 + c->format);
    else if (c->format == OV5640_RGB565)
    {
        format = 0x61;
        mux = 1;
    }
    else if (c->format == OV5640_RGB555 || c->format == OV5640_RGB444)
    {
        format = c->format == OV5640_RGB555 ? 0x81 : 0xa1;
        mux = 1;
    }
    else if (c->format == OV5640_RGB888)
    {
        format = 0x23;
        mux = 1;
    }
    else if (c->format == OV5640_RAW8 || c->format == OV5640_RAW10)
    {
        format = 0;
        mux = 3;
    }
    else if (c->format == OV5640_Y8)
        format = 0x10; /* Datasheet v2.03 table 6-5: native ISP Y8, YUV mux. */
    else if (c->format == OV5640_YUV444)
        format = 0x20;
    else if (c->format == OV5640_YUV420)
        format = 0x5f;
    TRY(wr(s, 0x4300, format));
    TRY(mod(s, 0x501f, 7, mux));
    TRY(mod(s, 0x5001, 0x20, 0x20)); /* automatic ISP scaling */
    TRY(mod(s, 0x501e, 0x40, 0));
    TRY(mod(s, 0x3002, 0x1c, c->format == OV5640_JPEG ? 0 : 0x1c));
    TRY(mod(s, 0x3006, 0x28, c->format == OV5640_JPEG ? 0x28 : 0));
    TRY(mod(s, 0x4600, 0x20, 0)); /* Clear any previous fixed-frame JPEG canvas. */
    TRY(mod(s, 0x4713, 7, 3)); /* JPEG mode3 default; advanced modes use feature API */
    if (c->interface == OV5640_MIPI)
    {
        TRY(wr(s, 0x300e, 0x44));
        TRY(wr(s, 0x3019, 0x70));
        TRY(wr(s, 0x3017, 0));
        TRY(wr(s, 0x3018, 0));
        TRY(mod(s, 0x4800, 0x24, 0x24)); /* LP11 while output is gated. */
        s->io.delay_us(s->io.user, 1000);
        TRY(mod(s, 0x4814, 0xc0, (uint8_t)(c->virtual_channel << 6)));
        TRY(wr(s, 0x471c, 0x50));
    }
    else
    {
        TRY(wr(s, 0x300e, 0x58));
        TRY(wr(s, 0x3017, 0xff));
        TRY(wr(s, 0x3018, 0xff));
        TRY(wr(s, 0x471c, 0x54));
        TRY(wr(s, 0x460b, 0x35));
        TRY(wr(s, 0x460c, 0x22));
    }
    TRY(mod(s, 0x3503, 3, 0));
    TRY(word(s, 0x3a02, (uint16_t)(w->vts - 4), 0xff));
    TRY(word(s, 0x3a14, (uint16_t)(w->vts - 4), 0xff));
    return OV5640_OK;
}
OV5640_RESULT Ov5640_Configure(OV5640 *s, const OV5640_CONFIG *c)
{
    OV5640_RESULT e;
    if (!valid_config(c))
        return OV5640_E_ARGUMENT;
    if (!usable(s) || s->state == OV5640_STREAMING)
        return OV5640_E_STATE;
    if (c->clock.xclk_hz != s->config.clock.xclk_hz)
        return OV5640_E_ARGUMENT;
    e = configure(s, c);
    if (e)
        return failure(s, e);
    s->config = *c;
    s->state = OV5640_CONFIGURED;
    e = Ov5640_SetBanding(s, 50);
    if (e)
        return failure(s, e);
    return OV5640_OK;
}
OV5640_RESULT Ov5640_Stream(OV5640 *s, int on)
{
    OV5640_RESULT e;
    if (!bound(s))
        return OV5640_E_ARGUMENT;
    if (on && (s->state != OV5640_CONFIGURED && s->state != OV5640_STREAMING))
        return OV5640_E_STATE;
    if (!on)
    {
        e = wr(s, 0x4202, 0x0f);
        if (e)
            return failure(s, e);
        if ((s->state == OV5640_CONFIGURED || s->state == OV5640_STREAMING) &&
            s->config.interface == OV5640_MIPI)
        {
            e = wr(s, 0x300e, 0x40);
            /* On AN5641, gating MIPI alone can leave RAW readout stalled on
             * the next enable. Standby guarantees a real wake transition,
             * as already used for successful format reconfiguration. */
            if(!e)e = power_state(s, 0x42);
        }
        else
            e = power_state(s, 0x42);
        if (e)
            return failure(s, e);
        if (s->state == OV5640_STREAMING)
            s->state = OV5640_CONFIGURED;
        return OV5640_OK;
    }
    e = power_state(s, 0x02);
    if (e)
        return failure(s, e);
    s->io.delay_us(s->io.user, 10000);
    if (s->config.interface == OV5640_MIPI)
    {
        e = mod(s, 0x4800, 0x20, s->config.continuous_clock ? 0 : 0x20);
        if (e)
            return failure(s, e);
        e = wr(s, 0x300e, 0x45);
        if (e)
            return failure(s, e);
    }
    e = wr(s, 0x4202, 0);
    if (e)
        return failure(s, e);
    s->state = OV5640_STREAMING;
    return OV5640_OK;
}
static const uint16_t ranges[][2] = {
    {0x3000, 0x3052}, {0x3a00, 0x3a25}, {0x3c00, 0x3c1e}, {0x3b00, 0x3b0c}, {0x3f00, 0x3f0d},
    {0x4000, 0x4033}, {0x4201, 0x4202}, {0x4300, 0x430d}, {0x4400, 0x4431}, {0x4600, 0x460d},
    {0x4700, 0x4745}, {0x4800, 0x4837}, {0x4901, 0x4902}, {0x5000, 0x5063}, {0x5180, 0x519e},
    {0x5300, 0x530c}, {0x5380, 0x538b}, {0x5480, 0x5490}, {0x5580, 0x558c}, {0x5600, 0x5606},
    {0x5680, 0x56a2}, {0x5800, 0x5849}, {0x6000, 0x603f}, {0x3602, 0x3606}};
OV5640_RESULT Ov5640_ApplyFeature(OV5640 *s, OV5640_FEATURE f, const OV5640_PATCH *p, size_t n)
{
    size_t i;
    OV5640_RESULT e;
    if ((unsigned)f >= sizeof(ranges) / sizeof(ranges[0]) || !p || !n || n > 256)
        return OV5640_E_ARGUMENT;
    if (!usable(s) || s->state == OV5640_STREAMING)
        return OV5640_E_STATE;
    for (i = 0; i < n; ++i)
        if (p[i].reg < ranges[f][0] || p[i].reg > ranges[f][1] || p[i].reg == 0x3008 ||
            (p[i].reg >= 0x3034 && p[i].reg <= 0x3037) || !p[i].mask || (p[i].value & ~p[i].mask))
            return OV5640_E_ARGUMENT;
    for (i = 0; i < n; ++i)
    {
        e = mod(s, p[i].reg, p[i].mask, p[i].value);
        if (e)
            return failure(s, e);
    }
    return OV5640_OK;
}
OV5640_RESULT Ov5640_SetAutoExposure(OV5640 *s, int ae, int ag)
{
    if (!usable(s))
        return OV5640_E_STATE;
    return mod(s, 0x3503, 3, (uint8_t)((ae ? 0 : 1) | (ag ? 0 : 2)));
}
OV5640_RESULT Ov5640_SetAutoExposureTarget(OV5640 *s,uint8_t target)
{
    if(target<8 || target>240)return OV5640_E_RANGE;
    uint8_t low=(uint8_t)(target*15U/16U),high=(uint8_t)(target*17U/16U);
    OV5640_PATCH p[]={{0x3a0f,255,high},{0x3a10,255,low},
        {0x3a1b,255,high},{0x3a1e,255,low},
        {0x3a11,255,(uint8_t)(high>127?255:high*2)}, {0x3a1f,255,(uint8_t)(low/2)}};
    return Ov5640_GroupUpdate(s,0,p,6);
}
OV5640_RESULT Ov5640_SetCip(OV5640 *s,int denoise,int automatic,uint8_t strength)
{
    if((denoise!=0 && denoise!=1) || (automatic!=0 && automatic!=1))return OV5640_E_ARGUMENT;
    uint8_t mask=denoise?0x10:0x40;
    OV5640_PATCH p[]={{(uint16_t)(denoise?0x5306:0x5302),255,strength},
        {0x5308,mask,(uint8_t)(automatic?0:mask)}};
    return Ov5640_GroupUpdate(s,0,p,2);
}
OV5640_RESULT Ov5640_SetExposure(OV5640 *s, uint32_t lines, uint16_t gain)
{
    OV5640_RESULT e;
    OV5640_PATCH p[6];
    if (!usable(s) || s->state < OV5640_CONFIGURED)
        return OV5640_E_STATE;
    if (!lines || lines > 65535 || gain < 16 || gain > 1023 || lines > s->config.window.vts - 4U)
        return OV5640_E_RANGE;
    p[0].reg = 0x3503;
    p[0].mask = 3;
    p[0].value = 3;
    p[1].reg = 0x3500;
    p[1].mask = 15;
    p[1].value = (uint8_t)(lines >> 12);
    p[2].reg = 0x3501;
    p[2].mask = 255;
    p[2].value = (uint8_t)(lines >> 4);
    p[3].reg = 0x3502;
    p[3].mask = 255;
    p[3].value = (uint8_t)(lines << 4);
    p[4].reg = 0x350a;
    p[4].mask = 3;
    p[4].value = (uint8_t)(gain >> 8);
    p[5].reg = 0x350b;
    p[5].mask = 255;
    p[5].value = (uint8_t)gain;
    e = Ov5640_GroupUpdate(s, 0, p, 6);
    return e;
}
OV5640_RESULT Ov5640_SetExposureLimits(OV5640 *s, uint16_t a, uint16_t b, uint16_t g, int night)
{
    OV5640_RESULT e;
    if (!usable(s))
        return OV5640_E_STATE;
    if (!a || !b || g < 16 || g > 1023)
        return OV5640_E_RANGE;
    e = word(s, 0x3a02, b, 255);
    if (e)
        return failure(s, e);
    e = word(s, 0x3a14, a, 255);
    if (e)
        return failure(s, e);
    e = word(s, 0x3a18, g, 3);
    if (e)
        return failure(s, e);
    return failure(s, mod(s, 0x3a00, 4, night ? 4 : 0));
}
OV5640_RESULT Ov5640_SetBanding(OV5640 *s, unsigned int hz)
{
    uint32_t step50, step60, clk;
    uint16_t max;
    OV5640_RESULT e;
    if (!usable(s) || s->state < OV5640_CONFIGURED)
        return OV5640_E_STATE;
    if (hz != 0 && hz != 1 && hz != 50 && hz != 60)
        return OV5640_E_ARGUMENT;
    clk = Ov5640_ConfigSystemClockHz(&s->config);
    max = (uint16_t)(s->config.window.vts - 4);
    step50 = clk / s->config.window.hts / 100;
    step60 = clk / s->config.window.hts / 120;
    if (!step50 || !step60 || step50 > 65535 || step60 > 65535 || max / step50 > 255 ||
        max / step60 > 255)
        return OV5640_E_RANGE;
    e = word(s, 0x3a08, (uint16_t)step50, 255);
    if (e)
        return failure(s, e);
    e = word(s, 0x3a0a, (uint16_t)step60, 255);
    if (e)
        return failure(s, e);
    e = wr(s, 0x3a0e, (uint8_t)(max / step50));
    if (e)
        return failure(s, e);
    e = wr(s, 0x3a0d, (uint8_t)(max / step60));
    if (e)
        return failure(s, e);
    e = mod(s, 0x3c01, 0x80, hz == 1 ? 0 : 0x80);
    if (e)
        return failure(s, e);
    e = mod(s, 0x3c00, 4, hz == 50 ? 4 : 0);
    if (e)
        return failure(s, e);
    return failure(s, mod(s, 0x3a00, 0x20, hz ? 0x20 : 0));
}
OV5640_RESULT Ov5640_SetWhiteBalance(OV5640 *s, int automatic, uint16_t r, uint16_t g, uint16_t b)
{
    OV5640_PATCH p[7];
    unsigned int i;
    uint16_t v[3];
    if (!usable(s))
        return OV5640_E_STATE;
    if (automatic)
        return mod(s, 0x3406, 1, 0);
    if (r > 4095 || g > 4095 || b > 4095 || !r || !g || !b)
        return OV5640_E_RANGE;
    v[0] = r;
    v[1] = g;
    v[2] = b;
    for (i = 0; i < 3; ++i)
    {
        p[2 * i].reg = (uint16_t)(0x3400 + 2 * i);
        p[2 * i].mask = 15;
        p[2 * i].value = (uint8_t)(v[i] >> 8);
        p[2 * i + 1].reg = (uint16_t)(0x3401 + 2 * i);
        p[2 * i + 1].mask = 255;
        p[2 * i + 1].value = (uint8_t)v[i];
    }
    p[6].reg = 0x3406;
    p[6].mask = 1;
    p[6].value = 1;
    return Ov5640_GroupUpdate(s, 0, p, 7);
}
OV5640_RESULT Ov5640_SetOrientation(OV5640 *s, int mirror, int flip)
{
    OV5640_PATCH p[2] = {{0x3820, 6, 0}, {0x3821, 6, 0}};
    OV5640_RESULT e;
    if (!usable(s))
        return OV5640_E_STATE;
    p[0].value = flip ? 6 : 0;
    p[1].value = mirror ? 6 : 0;
    e = Ov5640_GroupUpdate(s, 0, p, 2);
    if (!e)
    {
        s->config.mirror = mirror ? 1 : 0;
        s->config.flip = flip ? 1 : 0;
    }
    return e;
}
OV5640_RESULT Ov5640_SetIsp(OV5640 *s, uint8_t a, uint8_t b, uint8_t c)
{
    OV5640_PATCH p[3] = {{0x5000, 0xa7, 0}, {0x5001, 0xa7, 0}, {0x5003, 7, 0}};
    if ((a & ~0xa7) || (b & ~0xa7) || (c & ~7))
        return OV5640_E_ARGUMENT;
    p[0].value = a;
    p[1].value = b;
    p[2].value = c;
    return Ov5640_GroupUpdate(s, 0, p, 3);
}
OV5640_RESULT Ov5640_SetEffect(OV5640 *s, const OV5640_EFFECT *v)
{
    OV5640_PATCH p[14];
    unsigned int i;
    uint8_t values[12];
    if (!v || v->signs > 63 || v->manual_saturation > 1 || v->uv_threshold2 > 511)
        return OV5640_E_ARGUMENT;
    values[0] = v->enable_bits;
    values[1] = v->hue_cos;
    values[2] = v->hue_sin;
    values[3] = v->u;
    values[4] = v->v;
    values[5] = v->y_offset;
    values[6] = v->contrast;
    values[7] = v->brightness;
    values[8] = (uint8_t)(v->signs | (v->manual_saturation << 6));
    values[9] = v->uv_threshold1;
    values[10] = (uint8_t)(v->uv_threshold2 >> 8);
    values[11] = (uint8_t)v->uv_threshold2;
    for (i = 0; i < 12; ++i)
    {
        p[i].reg = (uint16_t)(0x5580 + i);
        p[i].mask = 255;
        p[i].value = values[i];
    }
    p[8].mask = 0x7f;
    p[10].mask = 1;
    p[12].reg = 0x5001;
    p[12].mask = 0x80;
    p[12].value = 0x80;
    return Ov5640_GroupUpdate(s, 0, p, 13);
}
OV5640_RESULT Ov5640_SetTestPattern(OV5640 *s, int en, uint8_t p, uint8_t style, int rolling,
                                    int trans, int bw)
{
    if (!usable(s))
        return OV5640_E_STATE;
    if (p > 3 || style > 3)
        return OV5640_E_ARGUMENT;
    return wr(s, 0x503d, (uint8_t)((en ? 128 : 0) | (rolling ? 64 : 0) | (trans ? 32 : 0) |
                                   (bw ? 16 : 0) | (style << 2) | p));
}
OV5640_RESULT Ov5640_SetGamma(OV5640 *s, const uint8_t curve[16], uint8_t control)
{
    unsigned int i;
    OV5640_RESULT e;
    if (!usable(s) || s->state == OV5640_STREAMING)
        return OV5640_E_STATE;
    if (!curve || control > 3)
        return OV5640_E_ARGUMENT;
    for (i = 1; i < 15; ++i)
        if (curve[i] < curve[i - 1])
            return OV5640_E_RANGE; /* last is slope */
    e = transfer(s, 0x5481, (uint8_t *)curve, 16, 1);
    if (e)
        return failure(s, e);
    return failure(s, mod(s, 0x5480, 3, control));
}
OV5640_RESULT Ov5640_SetLensCorrection(OV5640 *s, const uint8_t table[62], int en)
{
    OV5640_RESULT e;
    if (!usable(s) || s->state == OV5640_STREAMING)
        return OV5640_E_STATE;
    if (!table)
        return OV5640_E_ARGUMENT;
    e = transfer(s, 0x5800, (uint8_t *)table, 62, 1);
    if (e)
        return failure(s, e);
    return failure(s, mod(s, 0x5000, 128, en ? 128 : 0));
}
OV5640_RESULT Ov5640_SetDrawWindow(OV5640 *s, uint16_t l, uint16_t t, uint16_t r, uint16_t b,
                                   uint8_t u, uint8_t v, int en)
{
    OV5640_RESULT e;
    if (!usable(s) || s->state == OV5640_STREAMING)
        return OV5640_E_STATE;
    if (l > r || t > b || r > 4095 || b > 2047)
        return OV5640_E_RANGE;
    e = word(s, 0x5028, l, 15);
    if (e)
        return failure(s, e);
    e = word(s, 0x502a, r, 15);
    if (e)
        return failure(s, e);
    e = word(s, 0x502c, t, 7);
    if (e)
        return failure(s, e);
    e = word(s, 0x502e, b, 7);
    if (e)
        return failure(s, e);
    e = wr(s, 0x5035, u);
    if (e)
        return failure(s, e);
    e = wr(s, 0x5036, v);
    if (e)
        return failure(s, e);
    return failure(s, mod(s, 0x5003, 2, en ? 2 : 0));
}
OV5640_RESULT Ov5640_SetJpegFixedFrame(OV5640 *s,uint16_t width,uint16_t lines,uint8_t padding)
{
    if(!width || !lines)return OV5640_E_ARGUMENT;
    if(!usable(s) || s->state!=OV5640_CONFIGURED || s->config.format!=OV5640_JPEG)
        return OV5640_E_STATE;
    TRY(word(s,0x4602,width,0xff));
    TRY(word(s,0x4604,lines,0xff));
    TRY(wr(s,0x460d,padding));
    TRY(mod(s,0x4600,0x20,0x20));
    return mod(s,0x4713,7,2);
}
OV5640_RESULT Ov5640_SetJpegQuality(OV5640 *s, uint8_t q)
{
    if (!usable(s))
        return OV5640_E_STATE;
    if (q > 63)
        return OV5640_E_RANGE;
    return mod(s, 0x4407, 63, q);
}
OV5640_RESULT Ov5640_SetJpegQuantTable(OV5640 *s, uint8_t start, const uint8_t *p, uint16_t n)
{
    uint16_t i;
    OV5640_RESULT e;
    if (!usable(s) || s->state == OV5640_STREAMING)
        return OV5640_E_STATE;
    if (!p || !n || (unsigned)start + n > 256)
        return OV5640_E_ARGUMENT;
    for (i = 0; i < n; ++i)
    {
        e = wr(s, 0x4411, (uint8_t)(start + i));
        if (e)
            return failure(s, e);
        e = wr(s, 0x4410, p[i]);
        if (e)
            return failure(s, e);
    }
    return failure(s, mod(s, 0x4403, 128, 128));
}
OV5640_RESULT Ov5640_SetJpegComment(OV5640 *s, const uint8_t *p, uint8_t n, uint8_t marker)
{
    OV5640_RESULT e;
    if (!usable(s) || s->state == OV5640_STREAMING)
        return OV5640_E_STATE;
    if (n > 16 || (n & 1) || (!p && n))
        return OV5640_E_ARGUMENT;
    if (n)
    {
        e = transfer(s, 0x4420, (uint8_t *)p, n, 1);
        if (e)
            return failure(s, e);
    }
    e = wr(s, 0x4431, marker);
    if (e)
        return failure(s, e);
    return wr(s, 0x4430, n);
}
OV5640_RESULT Ov5640_GetJpegStatus(OV5640 *s, uint32_t *length, uint8_t *overflow)
{
    uint8_t p[3], v;
    if (!length || !overflow)
        return OV5640_E_ARGUMENT;
    TRY(transfer(s, 0x4414, p, 3, 0));
    TRY(rd(s, 0x4417, &v));
    *length = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
    *overflow = v & 1;
    return OV5640_OK;
}
OV5640_RESULT Ov5640_SetVcm(OV5640 *s, uint16_t target, uint8_t slew, uint16_t divider,
                            uint8_t current, int rih, int down)
{
    OV5640_RESULT e;
    if (!usable(s))
        return OV5640_E_STATE;
    if (target > 1023 || slew > 15 || divider > 4095 || current > 7)
        return OV5640_E_RANGE;
    e = wr(s, 0x3602, (uint8_t)((target << 4) | slew));
    if (e)
        return failure(s, e);
    e = mod(s, 0x3603, 0xbf, (uint8_t)((target >> 4) | (down ? 128 : 0)));
    if (e)
        return failure(s, e);
    e = wr(s, 0x3604, (uint8_t)divider);
    if (e)
        return failure(s, e);
    e = mod(s, 0x3605, 31, (uint8_t)((divider >> 8) | (rih ? 16 : 0)));
    if (e)
        return failure(s, e);
    return failure(s, mod(s, 0x3606, 7, current));
}
OV5640_RESULT Ov5640_SetFrexExposure(OV5640 *s, uint32_t lines, uint32_t width, uint8_t delay)
{
    OV5640_RESULT e;
    if (!usable(s))
        return OV5640_E_STATE;
    if (lines > 0xffffff || !lines || width > 0xfffff || delay > 15)
        return OV5640_E_RANGE;
    e = wr(s, 0x3b01, (uint8_t)(lines >> 16));
    if (e)
        return failure(s, e);
    e = word(s, 0x3b04, (uint16_t)lines, 255);
    if (e)
        return failure(s, e);
    e = wr(s, 0x3b06, (uint8_t)((delay << 4) | (width & 15)));
    if (e)
        return failure(s, e);
    return failure(s, word(s, 0x3b0b, (uint16_t)(width >> 4), 255));
}
OV5640_RESULT Ov5640_RequestFrex(OV5640 *s)
{
    if (!usable(s))
        return OV5640_E_STATE;
    return wr(s, 0x3b08, 1);
}
OV5640_RESULT Ov5640_GetStatus(OV5640 *s, OV5640_STATUS *v)
{
    OV5640_STATUS t;
    uint8_t p[3], a;
    if (!v || !usable(s))
        return OV5640_E_ARGUMENT;
    memset(&t, 0, sizeof(t));
    TRY(transfer(s, 0x3500, p, 3, 0));
    t.exposure_lines = (((uint32_t)p[0] & 15) << 12) | ((uint32_t)p[1] << 4) | (p[2] >> 4);
    TRY(readword(s, 0x350a, &t.gain16, 3));
    TRY(readword(s, 0x3400, &t.r_gain, 15));
    TRY(readword(s, 0x3402, &t.g_gain, 15));
    TRY(readword(s, 0x3404, &t.b_gain, 15));
    TRY(readword(s, 0x380c, &t.hts, 31));
    TRY(readword(s, 0x380e, &t.vts, 255));
    TRY(rd(s, 0x3503, &a));
    t.manual_exposure = a & 1;
    t.manual_gain = (a >> 1) & 1;
    TRY(rd(s, 0x3406, &a));
    t.manual_awb = a & 1;
    TRY(rd(s, 0x503d, &t.test_pattern));
    *v = t;
    return OV5640_OK;
}
static int group_register(uint16_t r)
{ /* Restrict atomic API to image controls; no resets/OTP/clock/MCU commands. */
    return (r >= 0x3400 && r <= 0x3406) || (r >= 0x3500 && r <= 0x350d) || r == 0x3820 ||
           r == 0x3821 || (r >= 0x3a00 && r <= 0x3a25) || (r >= 0x5000 && r <= 0x5005) ||
           (r >= 0x5300 && r <= 0x558b);
}
OV5640_RESULT Ov5640_GroupUpdate(OV5640 *s, uint8_t group, const OV5640_PATCH *p, size_t n)
{
    uint8_t values[13], a;
    size_t i, j;
    OV5640_RESULT e;
    if (!usable(s))
        return OV5640_E_STATE;
    if (!p || !n || n > 13 || group > 2)
        return OV5640_E_ARGUMENT; /* ID3 reserved for embedded SOF */
    for (i = 0; i < n; ++i)
    {
        if (!group_register(p[i].reg) || !p[i].mask || (p[i].value & ~p[i].mask))
            return OV5640_E_ARGUMENT;
        for (j = 0; j < i; ++j)
            if (p[j].reg == p[i].reg)
                return OV5640_E_ARGUMENT;
    }
    for (i = 0; i < n; ++i)
    {
        TRY(rd(s, p[i].reg, &a));
        values[i] = (uint8_t)((a & ~p[i].mask) | p[i].value);
    }
    if (s->state != OV5640_STREAMING)
    {
        for (i = 0; i < n; ++i)
        {
            e = wr(s, p[i].reg, values[i]);
            if (e)
                return failure(s, e);
        }
        return OV5640_OK;
    }
    TRY(rd(s, 0x3213, &a));
    if (a & 0x20)
        return OV5640_E_STATE;
    e = wr(s, 0x3212, group);
    if (e)
        return failure(s, e);
    for (i = 0; i < n; ++i)
    {
        e = wr(s, p[i].reg, values[i]);
        if (e)
            return failure(s, e);
    }
    e = wr(s, 0x3212, (uint8_t)(0x10 | group));
    if (e)
        return failure(s, e);
    e = wr(s, 0x3212, (uint8_t)(0xa0 | group));
    if (e)
        return failure(s, e);
    return OV5640_OK;
}
static OV5640_RESULT wait_clear(OV5640 *s, uint16_t reg, uint8_t mask, uint32_t timeout)
{
    uint8_t v;
    uint32_t elapsed = 0, step;
    for (;;)
    {
        TRY(rd(s, reg, &v));
        if (!(v & mask))
            return OV5640_OK;
        if (elapsed >= timeout)
            return OV5640_E_TIMEOUT;
        step = timeout - elapsed;
        if (step > 1000)
            step = 1000;
        s->io.delay_us(s->io.user, step);
        elapsed += step;
    }
}
OV5640_RESULT Ov5640_ReadOtp(OV5640 *s, uint8_t p[32])
{
    OV5640_RESULT e;
    if (!usable(s) || s->state == OV5640_STREAMING)
        return OV5640_E_STATE;
    if (!p)
        return OV5640_E_ARGUMENT;
    TRY(wr(s, 0x3d21, 1));
    s->io.delay_us(s->io.user, 5000);
    e = wait_clear(s, 0x3d21, 0x80, 100000);
    if (e)
    {
        (void)wr(s, 0x3d21, 0);
        return e;
    }
    e = transfer(s, 0x3d00, p, 32, 0);
    if (e)
    {
        (void)wr(s, 0x3d21, 0);
        return e;
    }
    return wr(s, 0x3d21, 0);
}
OV5640_RESULT Ov5640_LoadMcu(OV5640 *s, const uint8_t *p, uint16_t n)
{
    uint16_t offset, chunk;
    OV5640_RESULT e;
    if (!usable(s) || s->state == OV5640_STREAMING)
        return OV5640_E_STATE;
    if (!p || !n || n > 4096)
        return OV5640_E_ARGUMENT;
    e = mod(s, 0x3000, 0x20, 0x20);
    if (e)
        return failure(s, e);
    for (offset = 0; offset < n; offset = (uint16_t)(offset + chunk))
    {
        chunk = (uint16_t)(n - offset);
        if (chunk > 256)
            chunk = 256;
        e = transfer(s, (uint16_t)(0x8000 + offset), (uint8_t *)(p + offset), chunk, 1);
        if (e)
            return failure(s, e);
    }
    return failure(s, mod(s, 0x3000, 0x20, 0));
}
OV5640_RESULT Ov5640_McuCommand(OV5640 *s, uint8_t cmd, const uint8_t p[5], uint32_t timeout)
{
    OV5640_RESULT e;
    if (!usable(s))
        return OV5640_E_STATE;
    if (!p || !timeout || timeout > 10000000U)
        return OV5640_E_ARGUMENT;
    /* Common OV AF firmware mailbox ABI, not specified by sensor datasheet.
     * Caller must ensure the supplied firmware implements this protocol. */
    TRY(wait_clear(s, 0x3023, 0xff, timeout));
    e = transfer(s, 0x3024, (uint8_t *)p, 5, 1);
    if (e)
        return failure(s, e);
    e = wr(s, 0x3023, 1);
    if (e)
        return failure(s, e);
    e = wr(s, 0x3022, cmd);
    if (e)
        return failure(s, e);
    return wait_clear(s, 0x3023, 0xff, timeout);
}
OV5640_RESULT Ov5640_SetAddress(OV5640 *s, uint8_t address)
{
    uint16_t id;
    OV5640_RESULT e;
    if (!usable(s) || s->state == OV5640_STREAMING)
        return OV5640_E_STATE;
    if (address < 8 || address > 0x77)
        return OV5640_E_ARGUMENT;
    TRY(wr(s, 0x3100, (uint8_t)(address << 1)));
    s->address = address;
    e = Ov5640_Probe(s, &id);
    if (e)
        s->state = OV5640_FAULT; /* keep new address for explicit recovery */
    return e;
}
/* Manufacturing operation. Intentionally excluded from ordinary builds. */
OV5640_RESULT Ov5640_ProgramOtp(OV5640 *s, const uint8_t desired[27], uint32_t avdd_mv,
                                uint32_t confirmation)
{
#if defined(OV5640_ENABLE_OTP_PROGRAM) && OV5640_ENABLE_OTP_PROGRAM
    uint8_t old[32], shadow[32];
    unsigned int i;
    OV5640_RESULT e;
    if (!usable(s) || s->state == OV5640_STREAMING)
        return OV5640_E_STATE;
    if (!desired || avdd_mv < 1995 || avdd_mv > 2205 || confirmation != OV5640_OTP_CONFIRM)
        return OV5640_E_ARGUMENT;
    TRY(Ov5640_ReadOtp(s, old));
    memset(shadow, 0, sizeof(shadow));
    for (i = 0; i < 27; ++i)
    {
        if (old[i + 5] & (uint8_t)~desired[i])
            return OV5640_E_RANGE; /* no 1->0 */
        shadow[i + 5] = (uint8_t)(desired[i] & ~old[i + 5]);
    }
    TRY(transfer(s, 0x3d00, shadow, 32, 1));
    TRY(wr(s, 0x3d20, 3)); /* slow programming */
    s->io.delay_us(s->io.user, 10000);
    e = wait_clear(s, 0x3d20, 128, 1000000);
    if (e)
    {
        (void)wr(s, 0x3d20, 0);
        return failure(s, e);
    }
    TRY(wr(s, 0x3d20, 0));
    TRY(Ov5640_ReadOtp(s, old));
    if (memcmp(old + 5, desired, 27))
        return failure(s, OV5640_E_IO);
    return OV5640_OK;
#else
    (void)s;
    (void)desired;
    (void)avdd_mv;
    (void)confirmation;
    return OV5640_E_STATE;
#endif
}
