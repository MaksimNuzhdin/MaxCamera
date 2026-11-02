#include "GigeRemapControl.h"
#include "GigeFeatureRegisters.h"
#include "GigeVisionRegisters.h"
#include <assert.h>
#include <stdio.h>
static uint32_t a[GIGE_REMAP_MAX_WORDS] __attribute__((aligned(64)));
static uint32_t b[GIGE_REMAP_MAX_WORDS] __attribute__((aligned(64)));
static uint16_t write(GigeRemapControl *s,uint32_t addr,uint32_t value)
{return GigeRemapControl_Write(s,addr,value,1,0,1,1,91,1);}
int main(void)
{
    GigeRemapControl s;uint32_t value;
    assert(!GigeRemapControl_Init(&s,a,b,GIGE_REMAP_MAX_WORDS));
    assert(!GigeRemapControl_Read(&s,GIGE_F_REMAP_AVAILABLE,&value,0)&&value==0);
    assert(GigeRemapControl_Read(&s,GIGE_F_REMAP_STATE,&value,0)==GIGE_STATUS_NOT_IMPLEMENTED);
    assert(GigeRemapControl_Write(&s,GIGE_F_REMAP_BEGIN,1,0,0,1,1,91,1)==GIGE_STATUS_NOT_IMPLEMENTED);
    assert(GigeRemapControl_Write(&s,GIGE_F_REMAP_ABORT,1,1,1,1,1,91,1)==GIGE_STATUS_BUSY);
    assert(write(&s,GIGE_F_REMAP_ENABLE,1)==GIGE_STATUS_INVALID_PARAM);
    assert(!write(&s,GIGE_F_REMAP_WIDTH,1));assert(!write(&s,GIGE_F_REMAP_HEIGHT,1));
    assert(!write(&s,GIGE_F_REMAP_EXPECTED_CRC,0xccfc5c3c));
    assert(!write(&s,GIGE_F_REMAP_BEGIN,1));
    assert(write(&s,GIGE_F_REMAP_WIDTH,2)==GIGE_STATUS_BUSY);
    assert(!write(&s,GIGE_F_REMAP_INDEX,0));assert(!write(&s,GIGE_F_REMAP_DATA,0x80000000));
    assert(GigeRemapControl_Write(&s,GIGE_F_REMAP_COMMIT,1,1,0,1,1,92,1)==GIGE_STATUS_INVALID_PARAM);
    assert(!write(&s,GIGE_F_REMAP_COMMIT,1));
    GigeRemapControl_Poll(&s,1);assert(s.map.state==GIGE_REMAP_VALIDATE);
    GigeRemapControl_Poll(&s,0);assert(s.map.state==GIGE_REMAP_READY);
    assert(GigeRemapControl_Write(&s,GIGE_F_REMAP_ENABLE,1,1,0,1,1,91,0)==GIGE_STATUS_INVALID_PARAM);
    assert(GigeRemapControl_Write(&s,GIGE_F_REMAP_ENABLE,1,1,0,2,1,91,1)==GIGE_STATUS_INVALID_PARAM);
    assert(!write(&s,GIGE_F_REMAP_ENABLE,1));assert(s.enabled);
    assert(write(&s,GIGE_F_REMAP_BEGIN,1)==GIGE_STATUS_BUSY);
    assert(write(&s,GIGE_F_REMAP_HARDWARE_STATUS,0)==GIGE_STATUS_WRITE_PROTECT);
    assert(!write(&s,GIGE_F_REMAP_ENABLE,0));assert(!write(&s,GIGE_F_REMAP_ABORT,1));
    assert(s.map.state==GIGE_REMAP_EMPTY);
    GigeRemapControl_Invalidate(&s);assert(!s.enabled);
    puts("PASS remap controls availability, stopped-only writes, transfer ordering, geometry binding, bounded validation and enable gating");
}
