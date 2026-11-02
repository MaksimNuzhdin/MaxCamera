#include "GigeRemapMap.h"
#include <limits.h>
#include <string.h>
static uint32_t crc_word(uint32_t crc,uint32_t word)
{
    for(unsigned i=0;i<4;++i) {
        crc^=word&255U;word>>=8;
        for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((0U-(crc&1U))&0xedb88320U);
    }
    return crc;
}
int GigeRemapMap_Init(GigeRemapMap *s,uint32_t *a,uint32_t *b,size_t capacity)
{
    if(!s || !a || !b || capacity<GIGE_REMAP_MAX_WORDS || capacity>SIZE_MAX/4U)return -1;
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b,bytes=capacity*4U;
    if((x|y)&63U || x>UINTPTR_MAX-bytes || y>UINTPTR_MAX-bytes ||
       (x<y+bytes && y<x+bytes))return -1;
    memset(s,0,sizeof(*s));s->bank[0]=a;s->bank[1]=b;s->capacity=capacity;s->active_bank=2;
    return 0;
}
int GigeRemapMap_Begin(GigeRemapMap *s,uint32_t rows,uint32_t cols,uint32_t crc,int busy)
{
    if(!s || !s->bank[0] || busy || !rows || rows>480 || !cols || cols>640 || s->generation==UINT64_MAX)return -1;
    ++s->generation;s->prepared_generation=0;
    s->staging_bank=s->active_bank==0?1:0;
    s->rows=rows;s->cols=cols;s->uploaded=0;s->checked=0;s->expected_crc=crc;
    s->crc=UINT32_MAX;s->error=0;s->state=GIGE_REMAP_UPLOAD;
    return 0;
}
int GigeRemapMap_Write(GigeRemapMap *s,uint32_t index,uint32_t word,int busy)
{
    if(!s || busy || s->state!=GIGE_REMAP_UPLOAD || index>=s->rows*s->cols || index>s->uploaded)return -1;
    uint32_t *p=s->bank[s->staging_bank];
    if(index<s->uploaded)return p[index]==word?0:-1;
    p[index]=word;++s->uploaded;return 0;
}
int GigeRemapMap_Commit(GigeRemapMap *s,int busy)
{
    if(!s || busy || s->state!=GIGE_REMAP_UPLOAD || s->uploaded!=s->rows*s->cols)return -1;
    s->state=GIGE_REMAP_VALIDATE;return 0;
}
int GigeRemapMap_Step(GigeRemapMap *s,unsigned budget)
{
    if(!s || !budget || budget>256 || s->state!=GIGE_REMAP_VALIDATE)return -1;
    uint32_t total=s->rows*s->cols;
    for(unsigned n=0;n<budget && s->checked<total;++n) {
        uint32_t word=s->bank[s->staging_bank][s->checked],x=word&1023U,y=(word>>10)&511U;
        uint32_t row=s->checked/s->cols;
        if((word&0x7ff80000U) || (!(word&0x80000000U) && word) ||
           ((word&0x80000000U) && (x>=s->cols || y>=s->rows || y+7U<row || y>row+7U))) {
            s->error=1;s->state=GIGE_REMAP_ERROR;return -1;
        }
        s->crc=crc_word(s->crc,word);++s->checked;
    }
    if(s->checked!=total)return 0;
    if((s->crc^UINT32_MAX)!=s->expected_crc) {s->error=2;s->state=GIGE_REMAP_ERROR;return -1;}
    s->state=GIGE_REMAP_READY;return 1;
}
void GigeRemapMap_Abort(GigeRemapMap *s)
{if(s){s->state=GIGE_REMAP_EMPTY;s->prepared_generation=0;}}
void GigeRemapMap_ReaderQuiesced(GigeRemapMap *s)
{if(s)s->reader_may_access=0;}
int GigeRemapMap_Prepare(GigeRemapMap *s,uint32_t rows,uint32_t cols,
                        const uint32_t **address,uint64_t *generation)
{
    if(!s || !address || !generation || s->reader_may_access || s->state!=GIGE_REMAP_READY ||
       rows!=s->rows || cols!=s->cols)return -1;
    s->prepared_generation=s->generation;*generation=s->generation;
    *address=s->bank[s->staging_bank];return 0;
}
int GigeRemapMap_ReaderStarted(GigeRemapMap *s,uint64_t generation)
{
    if(!s || s->reader_may_access || s->state!=GIGE_REMAP_READY || !generation ||
       generation!=s->generation || generation!=s->prepared_generation)return -1;
    s->active_bank=s->staging_bank;s->reader_may_access=1;s->prepared_generation=0;return 0;
}
