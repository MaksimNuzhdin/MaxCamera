#include "GigeRemapReader.h"
#include <assert.h>
#include <stdio.h>
static uint32_t regs[64];static unsigned reads,writes;
static uint32_t rd(void *u,uint32_t a)
{(void)u;assert(a>=GIGE_REMAP_READER_BASE && a<GIGE_REMAP_READER_BASE+256);++reads;return regs[(a-GIGE_REMAP_READER_BASE)/4];}
static void wr(void *u,uint32_t a,uint32_t v)
{(void)u;assert(a>=GIGE_REMAP_READER_BASE && a<GIGE_REMAP_READER_BASE+256);++writes;regs[(a-GIGE_REMAP_READER_BASE)/4]=v;}
int main(void)
{
    GigeRemapReader s;GigeFpgaIO io={0,rd,wr};uint32_t status;
    GigeRemapReader_Init(&s,&io,0);assert(GigeRemapReader_CanReset(&s));
    assert(GigeRemapReader_Configure(&s,480,640,0,0));assert(GigeRemapReader_Start(&s));
    assert(!reads&&!writes);GigeRemapReader_Init(&s,&io,1);regs[0]=4;
    assert(!GigeRemapReader_Configure(&s,1944,2592,0,0));
    assert(regs[0x10/4]==0 && regs[0x14/4]==0 && regs[0x1c/4]==1944);
    regs[0]=4;assert(!GigeRemapReader_Start(&s));assert(regs[0]==1);
    assert(GigeRemapReader_CanReset(&s)); /* bypass issues no AXI reads */
    regs[0]=4;assert(GigeRemapReader_Configure(&s,481,640,0x1000000,1));
    assert(GigeRemapReader_Configure(&s,480,640,0x1000001,1));
    assert(!GigeRemapReader_Configure(&s,480,640,0x1234000000ULL,1));
    assert(regs[0x10/4]==0x34000000 && regs[0x14/4]==0x12);
    regs[0]=4;assert(!GigeRemapReader_Start(&s));assert(!GigeRemapReader_CanReset(&s));
    unsigned before=writes;assert(GigeRemapReader_ResetObserved(&s));
    assert(GigeRemapReader_Configure(&s,480,640,0,0));assert(writes==before);
    assert(!GigeRemapReader_Completed(&s,&status));
    regs[0]=4;regs[0x34/4]=4;assert(GigeRemapReader_Completed(&s,&status)==1 && status==4);
    assert(GigeRemapReader_CanReset(&s));assert(!GigeRemapReader_ResetObserved(&s));
    assert(GigeRemapReader_Start(&s));
    puts("PASS remap reader absent-bank exclusion, ABI, full-resolution bypass, manual start, completion status and outstanding-reader reset guard");
}
