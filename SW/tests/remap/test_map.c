#include "GigeRemapMap.h"
#include <assert.h>
#include <stdio.h>
static uint32_t a[GIGE_REMAP_MAX_WORDS] __attribute__((aligned(64)));
static uint32_t b[GIGE_REMAP_MAX_WORDS] __attribute__((aligned(64)));
static const uint32_t identity[]={0x80000000,0x80000001,0x80000400,0x80000401};
static void ready(GigeRemapMap *s)
{
    assert(!GigeRemapMap_Begin(s,2,2,0xa685fa7a,0)); /* independently computed zlib CRC32 */
    for(unsigned n=0;n<4;++n)assert(!GigeRemapMap_Write(s,n,identity[n],0));
    assert(!GigeRemapMap_Commit(s,0));
    assert(GigeRemapMap_Step(s,1)==0);assert(s->checked==1);
    assert(GigeRemapMap_Step(s,256)==1);
}
int main(void)
{
    GigeRemapMap s;const uint32_t *address;uint64_t token;
    assert(GigeRemapMap_Init(&s,a,a,GIGE_REMAP_MAX_WORDS));
    assert(GigeRemapMap_Init(&s,a+1,b,GIGE_REMAP_MAX_WORDS));
    assert(GigeRemapMap_Init(&s,a,b,1));
    assert(!GigeRemapMap_Init(&s,a,b,GIGE_REMAP_MAX_WORDS));
    assert(GigeRemapMap_Begin(&s,481,640,0,0));
    assert(GigeRemapMap_Begin(&s,2,2,0,1));
    assert(!GigeRemapMap_Begin(&s,2,2,0xa685fa7a,0));
    assert(GigeRemapMap_Write(&s,1,identity[1],0));assert(s.uploaded==0);
    assert(GigeRemapMap_Write(&s,0,identity[0],1));
    assert(!GigeRemapMap_Write(&s,0,identity[0],0));
    assert(!GigeRemapMap_Write(&s,0,identity[0],0));assert(s.uploaded==1);
    assert(GigeRemapMap_Write(&s,0,17,0));assert(a[0]==identity[0]);
    assert(GigeRemapMap_Commit(&s,0));
    ready(&s);assert(GigeRemapMap_Prepare(&s,3,2,&address,&token));
    assert(!GigeRemapMap_Prepare(&s,2,2,&address,&token));assert(address==a);
    assert(!GigeRemapMap_ReaderStarted(&s,token));
    assert(GigeRemapMap_Prepare(&s,2,2,&address,&token));
    ready(&s);assert(s.staging_bank==1 && s.active_bank==0);
    for(unsigned n=0;n<4;++n)assert(a[n]==identity[n]);
    assert(GigeRemapMap_Prepare(&s,2,2,&address,&token));
    GigeRemapMap_ReaderQuiesced(&s);
    assert(!GigeRemapMap_Prepare(&s,2,2,&address,&token));assert(address==b);
    uint64_t stale=token;GigeRemapMap_Abort(&s);
    assert(GigeRemapMap_ReaderStarted(&s,stale));assert(s.active_bank==0);
    ready(&s);assert(!GigeRemapMap_Prepare(&s,2,2,&address,&token));
    assert(token!=stale && GigeRemapMap_ReaderStarted(&s,stale));
    assert(!GigeRemapMap_ReaderStarted(&s,token));assert(s.active_bank==1);
    GigeRemapMap_Abort(&s);assert(s.reader_may_access && s.active_bank==1);
    for(unsigned fault=0;fault<3;++fault) {
        assert(!GigeRemapMap_Begin(&s,9,1,0,0));
        for(unsigned n=0;n<9;++n)assert(!GigeRemapMap_Write(&s,n,n?0:fault==0?1:fault==1?0x80080000:0x80002000,0));
        assert(!GigeRemapMap_Commit(&s,0));
        assert(GigeRemapMap_Step(&s,256)==-1 && s.error==1);
        assert(s.active_bank==1 && s.reader_may_access);
    }
    assert(!GigeRemapMap_Begin(&s,1,1,0,0));assert(!GigeRemapMap_Write(&s,0,0,0));
    assert(!GigeRemapMap_Commit(&s,0));assert(GigeRemapMap_Step(&s,1)==-1 && s.error==2);
    assert(!GigeRemapMap_Begin(&s,480,640,0x7fd9f67a,0));
    for(unsigned n=0;n<GIGE_REMAP_MAX_WORDS;++n)assert(!GigeRemapMap_Write(&s,n,0,0));
    assert(!GigeRemapMap_Commit(&s,0));assert(GigeRemapMap_Step(&s,257)==-1 && s.checked==0);
    unsigned calls=0;int result;
    do {uint32_t before=s.checked;result=GigeRemapMap_Step(&s,256);assert(s.checked-before<=256);++calls;}while(!result);
    assert(result==1 && calls==1200);
    s.generation=UINT64_MAX;assert(GigeRemapMap_Begin(&s,2,2,0,0));assert(s.state==GIGE_REMAP_READY);
    puts("PASS map bank isolation, sequential/idempotent upload, CRC/window validation, bounded work, stale tokens, explicit reader quiescence and generation exhaustion");
}
