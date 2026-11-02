#include "bracket_session.h"
#include <limits.h>
#include <string.h>

static uint64_t token_for(const HdrSession *s) {return s->generation*2+s->phase;}
static void free_leases(HdrSession *s)
{for(unsigned i=0;i<2;++i)s->leases[i].state=HDR_FREE;}
void HdrSession_Init(HdrSession *s) {if(s)memset(s,0,sizeof(*s));}
static int same_request(const HdrFrame *a,const HdrFrame *b)
{
    return a->width==b->width && a->height==b->height &&
        a->geometry_generation==b->geometry_generation &&
        a->configuration_generation==b->configuration_generation &&
        a->bayer_phase==b->bayer_phase && a->line_period_ns==b->line_period_ns &&
        a->exposure_lines==b->exposure_lines && a->gain16==b->gain16 &&
        a->black==b->black && a->white==b->white;
}
int HdrSession_Begin(HdrSession *s,const HdrFrame req[2],uint16_t *a,uint16_t *b,
                     size_t count,uint64_t now,uint64_t timeout,uint64_t gap)
{
    if(!s || !req || s->state!=HDR_IDLE || !a || !b || !count ||
       !timeout || timeout>INT64_MAX || !gap || gap>timeout ||
       s->generation>=UINT64_MAX/2 || count>SIZE_MAX/sizeof(*a))return -1;
    HdrFrame requested[2]={req[0],req[1]};
    for(unsigned i=0;i<2;++i) {
        requested[i].effective_exposure_verified=1; /* Validate request shape only. */
        requested[i].frame_id=i;requested[i].timestamp_us=i;
    }
    if(HdrBracket_Validate(&requested[0],&requested[1],gap) ||
       count!=(size_t)requested[0].width*requested[0].height)return -1;
    uintptr_t aa=(uintptr_t)a,bb=(uintptr_t)b;size_t bytes=count*sizeof(*a);
    if(aa>UINTPTR_MAX-bytes || bb>UINTPTR_MAX-bytes ||
       (aa<bb+bytes && bb<aa+bytes) || aa%_Alignof(uint16_t) || bb%_Alignof(uint16_t))return -1;
    ++s->generation;s->phase=0;s->started_us=now;s->timeout_us=timeout;
    s->maximum_gap_us=gap;s->configuration_pending=0;
    for(unsigned i=0;i<2;++i) {
        s->requested[i]=requested[i];s->requested[i].effective_exposure_verified=0;
        s->requested[i].frame_id=0;s->requested[i].timestamp_us=0;
        memset(&s->frames[i],0,sizeof(s->frames[i]));
        s->leases[i]=(HdrLease){i?b:a,count,HDR_RESERVED};
    }
    s->state=HDR_CONFIGURE;return 0;
}
int HdrSession_Configure(HdrSession *s,uint64_t now_us,uint64_t *token,HdrFrame *request)
{
    HdrSession_Poll(s,now_us);
    if(!s || !token || !request || s->state!=HDR_CONFIGURE || s->configuration_pending)return -1;
    *token=token_for(s);*request=s->requested[s->phase];s->configuration_pending=1;return 0;
}
int HdrSession_Configured(HdrSession *s,uint64_t token,uint64_t now_us,int success)
{
    HdrSession_Poll(s,now_us);
    if(!s || s->state!=HDR_CONFIGURE || !s->configuration_pending || token!=token_for(s))return -1;
    s->configuration_pending=0;
    if(!success){HdrSession_Cancel(s);return -1;}
    s->state=HDR_CAPTURE;return 0;
}
int HdrSession_Arm(HdrSession *s,uint64_t token,uint64_t now_us,uint16_t **buffer)
{
    HdrSession_Poll(s,now_us);
    if(!s || !buffer || s->state!=HDR_CAPTURE || token!=token_for(s) ||
       s->leases[s->phase].state!=HDR_RESERVED)return -1;
    s->leases[s->phase].state=HDR_DMA;*buffer=s->leases[s->phase].data;return 0;
}
int HdrSession_Complete(HdrSession *s,uint64_t token,uint64_t now_us,const HdrFrame *f)
{
    HdrSession_Poll(s,now_us);
    if(!s || s->state!=HDR_CAPTURE || token!=token_for(s) ||
       s->leases[s->phase].state!=HDR_DMA)return -1;
    /* Completion acknowledges DMA quiescence even when frame metadata is invalid. */
    s->leases[s->phase].state=HDR_HELD;
    if(HdrFrame_Validate(f) || !same_request(f,&s->requested[s->phase]) ||
       (s->phase && HdrBracket_Validate(&s->frames[0],f,s->maximum_gap_us))) {
        HdrSession_Cancel(s);return -1;
    }
    s->frames[s->phase]=*f;
    if(s->phase)s->state=HDR_READY;
    else {s->phase=1;s->state=HDR_CONFIGURE;}
    return 0;
}
void HdrSession_Cancel(HdrSession *s)
{
    if(!s || s->state==HDR_IDLE || s->state==HDR_FAILED || s->state==HDR_DRAINING)return;
    if(s->configuration_pending || s->leases[0].state==HDR_DMA || s->leases[1].state==HDR_DMA)
        s->state=HDR_DRAINING;
    else s->state=HDR_FAILED;
}
void HdrSession_Poll(HdrSession *s,uint64_t now)
{
    if(s && (s->state==HDR_CONFIGURE || s->state==HDR_CAPTURE) &&
       now-s->started_us>=s->timeout_us)HdrSession_Cancel(s);
}
int HdrSession_Quiesced(HdrSession *s,uint64_t token)
{
    if(!s || s->state!=HDR_DRAINING || token!=token_for(s))return -1;
    s->configuration_pending=0;
    for(unsigned i=0;i<2;++i)if(s->leases[i].state==HDR_DMA)s->leases[i].state=HDR_RESERVED;
    s->state=HDR_FAILED;return 0;
}
int HdrSession_Release(HdrSession *s)
{
    if(!s || (s->state!=HDR_READY && s->state!=HDR_FAILED))return -1;
    free_leases(s);s->state=HDR_IDLE;return 0;
}
