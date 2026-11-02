#include "bracket_merge.h"
int HdrFrame_Validate(const HdrFrame *f)
{
    return !(f && f->width && f->width<=2592 && f->height && f->height<=1944 &&
        f->bayer_phase<4 && f->exposure_lines && f->exposure_lines<=65535 &&
        f->gain16>=16 && f->gain16<=1023 && f->line_period_ns &&
        f->white<=1023 && f->black<f->white &&
        f->effective_exposure_verified==1 && f->manual_exposure==1);
}
int HdrBracket_Validate(const HdrFrame *s,const HdrFrame *l,uint64_t gap)
{
    if(HdrFrame_Validate(s) || HdrFrame_Validate(l) || !gap)return -1;
    if(s->width!=l->width || s->height!=l->height || s->bayer_phase!=l->bayer_phase ||
       s->geometry_generation!=l->geometry_generation ||
       s->configuration_generation!=l->configuration_generation ||
       s->line_period_ns!=l->line_period_ns || l->frame_id<=s->frame_id ||
       l->timestamp_us<=s->timestamp_us || l->timestamp_us-s->timestamp_us>gap)return -1;
    return (uint64_t)s->exposure_lines*s->gain16 <
        (uint64_t)l->exposure_lines*l->gain16 ? 0:-1;
}
static void merge_pixel(const HdrFrame *s,const HdrFrame *l,uint16_t sr,uint16_t lr,
                        uint32_t *out,uint8_t *flags)
{
    const HdrFrame *f=l;uint16_t raw=lr;
    *flags=HDR_VALID;*out=0;
    if(raw>=l->white) {
        f=s;raw=sr;
        if(raw>=s->white){*flags=HDR_BOTH_SATURATED;return;}
    }
    if(raw<=f->black){*flags=HDR_BELOW_BLACK;return;}
    uint64_t denominator=(uint64_t)f->exposure_lines*f->gain16;
    uint64_t numerator=(uint64_t)(raw-f->black)*16U*65536U;
    *out=(uint32_t)((numerator+denominator/2)/denominator);
}
int HdrBracket_Merge(const HdrFrame *s,const HdrFrame *l,
                     const uint16_t *sr,const uint16_t *lr,size_t count,uint64_t gap,
                     uint32_t *out,uint8_t *flags)
{
    if(HdrBracket_Validate(s,l,gap) || !sr || !lr || !out || !flags ||
       count!=(size_t)s->width*s->height)return -1;
    /* Reject the entire transaction before writing output on malformed data. */
    for(size_t i=0;i<count;++i)if(sr[i]>1023 || lr[i]>1023)return -1;
    for(size_t i=0;i<count;++i)merge_pixel(s,l,sr[i],lr[i],&out[i],&flags[i]);
    return 0;
}
void HdrMerge_Init(HdrMergeJob *j)
{if(j){*j=(HdrMergeJob){0};j->state=HDR_MERGE_FAILED;}}
int HdrMerge_Begin(HdrMergeJob *j,const HdrFrame *s,const HdrFrame *l,
                   const uint16_t *sr,const uint16_t *lr,size_t count,uint64_t gap,
                   uint32_t *out,uint8_t *flags)
{
    if(!j || (j->state!=HDR_MERGE_FAILED && j->state!=HDR_MERGE_DONE) ||
       HdrBracket_Validate(s,l,gap) || !sr || !lr || !out || !flags ||
       count!=(size_t)s->width*s->height)return -1;
    *j=(HdrMergeJob){HDR_MERGE_VALIDATE,*s,*l,sr,lr,out,flags,count,0};return 0;
}
int HdrMerge_Step(HdrMergeJob *j,unsigned budget)
{
    if(!j || !budget || budget>256)return -1;
    if(j->state==HDR_MERGE_DONE)return 1;
    if(j->state!=HDR_MERGE_VALIDATE && j->state!=HDR_MERGE_PIXELS)return -1;
    size_t left=j->samples-j->cursor,n=left<budget?left:budget;
    for(size_t i=0;i<n;++i) {
        size_t at=j->cursor++;
        if(j->state==HDR_MERGE_VALIDATE) {
            if(j->short_raw[at]>1023 || j->long_raw[at]>1023) {
                j->state=HDR_MERGE_FAILED;return -1;
            }
        } else merge_pixel(&j->short_frame,&j->long_frame,j->short_raw[at],j->long_raw[at],
                           &j->radiance[at],&j->flags[at]);
    }
    if(j->cursor==j->samples) {
        if(j->state==HDR_MERGE_VALIDATE){j->cursor=0;j->state=HDR_MERGE_PIXELS;}
        else {j->state=HDR_MERGE_DONE;return 1;}
    }
    return 0;
}
void HdrMerge_Cancel(HdrMergeJob *j){if(j)j->state=HDR_MERGE_FAILED;}
