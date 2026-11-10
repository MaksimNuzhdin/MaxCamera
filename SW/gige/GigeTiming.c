#include "GigeTiming.h"
int GigeTiming_DeferTelemetry(uint32_t payload,uint32_t rate,uint32_t software_pixels)
{
    /* The async SCCB deadline includes scheduling gaps. Large CPU image
     * conversions can strand a sensor read even below the wire-rate gate.
     * Keep the qualified VGA software path; direct DMA formats retain the
     * existing bandwidth policy. Sensor hardware AE/AGC stays enabled. */
    return software_pixels>640U*480U || (uint64_t)payload*rate>105000000000ULL;
}
int GigeTiming_CanServiceSensor(uint32_t rate,uint64_t elapsed)
{
    if(!rate)return 0;
    uint64_t period=1000000000ULL/rate;
    /* SCCB reads block for about 1ms. Leave margin before single-shot DMA
     * completion, where the next capture must be armed within blanking. */
    return period>4000U && elapsed<period-4000U;
}
uint32_t GigeTiming_MaxVts(uint32_t clock,uint32_t hts)
{
    if(!clock || !hts)return 0;
    uint32_t a=clock/hts/100,b=clock/hts/120;
    if(!a || !b || a>65535 || b>65535)return 0;
    /* Both AEC band-count registers are only eight bits, even if banding is
     * disabled. Match the sensor driver's validated register limits. */
    uint64_t maximum=(uint64_t)(a<b?a:b)*255U+4;
    /* Conservative qualified limit on AN5641: programming VTS62500 read
     * back correctly but did not yield the requested 0.5fps cadence. Do not
     * advertise that unqualified range merely because the register is16-bit. */
    return maximum>32767?32767:(uint32_t)maximum;
}
uint32_t GigeTiming_MinRate(uint32_t clock,uint32_t hts)
{
    uint32_t maximum=GigeTiming_MaxVts(clock,hts);if(!maximum)return 0;
    uint64_t denominator=(uint64_t)hts*maximum;
    return (uint32_t)(((uint64_t)clock*1000+denominator-1)/denominator);
}
uint32_t GigeTiming_Vts(uint32_t clock,uint32_t hts,uint32_t base,uint32_t rate)
{
    uint32_t maximum=GigeTiming_MaxVts(clock,hts);
    if(!maximum || !base || base>maximum || !rate)return 0;
    uint64_t denominator=(uint64_t)hts*rate;
    uint64_t lines=((uint64_t)clock*1000+denominator-1)/denominator;
    if(lines<base)lines=base;
    if(lines>maximum)lines=maximum;
    return (uint32_t)lines;
}
