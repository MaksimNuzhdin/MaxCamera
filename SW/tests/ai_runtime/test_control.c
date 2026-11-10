#include "GigeAI.h"
#include "GigeFeatureRegisters.h"
#include <assert.h>
#include <stdio.h>
static uint32_t m[GIGE_AI_BYTES/4];
static uint32_t readreg(uint32_t a){uint32_t v;assert(!GigeAI_Read(a,&v));return v;}
static void result(uint32_t seq,uint32_t cfg,uint32_t width)
{
    m[AI_COPY_ACK]=seq;m[AI_OUTPUT]=seq;m[AI_OUTPUT+1]=cfg;m[AI_OUTPUT+2]=1;
    m[AI_OBJECTS]=0;m[AI_OBJECTS+2]=900000;m[AI_OBJECTS+3]=1;m[AI_OBJECTS+4]=2;
    m[AI_OBJECTS+5]=width;m[AI_OBJECTS+6]=10;m[AI_RESULT]=seq;
}
int main(void)
{
    GigeAI_Init(0);assert(!readreg(GIGE_F_AI_AVAILABLE));assert(GigeAI_Write(GIGE_F_AI_ENABLE,1));
    GigeAI_Init(m);m[AI_HEARTBEAT]=1;m[AI_SUPPORTED]=255;GigeAI_Poll(1);
    assert(readreg(GIGE_F_AI_AVAILABLE));assert(!GigeAI_Write(GIGE_F_AI_ENABLE,1));
    const void *frame=(void *)0x200000;
    GigeAI_Offer(frame,320,240,960,0x02180014,123456,99);
    assert(GigeAI_Pinned(frame));uint32_t seq=m[AI_REQUEST],cfg=m[AI_CONFIG];assert(seq==1);
    GigeAI_Offer((void *)0x300000,320,240,960,0x02180014,123457,100);assert(m[AI_REQUEST]==seq);
    GigeAI_Poll(4000000);assert(!readreg(GIGE_F_AI_AVAILABLE));assert(GigeAI_Pinned(frame));
    result(seq,cfg,20);m[AI_HEARTBEAT]=2;GigeAI_Poll(4000001);assert(!GigeAI_Pinned(frame));
    assert(!GigeAI_Write(GIGE_F_AI_LATCH,1));assert(readreg(GIGE_F_AI_VALID));
    assert(readreg(GIGE_F_AI_COUNT)==1);assert(readreg(GIGE_F_AI_CAPTURE)==99);
    assert(readreg(GIGE_F_AI_RESULT_WIDTH)==320);assert(readreg(GIGE_F_AI_RESULT_HEIGHT)==240);
    assert(readreg(GIGE_F_AI_RESULT_CONFIG)==cfg);assert(readreg(GIGE_F_AI_RESULT_AGE)==4000000);
    GigeAI_Offer(frame,320,240,960,0x02180014,123458,100);
    assert(m[AI_REQUEST]==2);assert(readreg(GIGE_F_AI_VALID));
    result(2,cfg,999);GigeAI_Poll(4000002);
    /* Invalid new output cannot mutate an already latched snapshot. */
    assert(readreg(GIGE_F_AI_OBJECT_WIDTH)==20);assert(readreg(GIGE_F_AI_CAPTURE)==99);
    assert(!GigeAI_Write(GIGE_F_AI_LATCH,1));assert(!readreg(GIGE_F_AI_VALID));
    assert(GigeAI_Write(GIGE_F_AI_SELECTOR,32));assert(GigeAI_Write(GIGE_F_AI_PERIOD,1));
    GigeAI_Poll(5000000);GigeAI_Offer(frame,320,240,960,0x02180014,123459,101);
    assert(m[AI_REQUEST]==3);assert(GigeAI_Pinned(frame));
    assert(!GigeAI_Write(GIGE_F_AI_ENABLE,0));assert(GigeAI_Pinned(frame));
    result(3,cfg,20);GigeAI_Poll(5000001);assert(!GigeAI_Pinned(frame));
    assert(!GigeAI_Write(GIGE_F_AI_LATCH,1));assert(!readreg(GIGE_F_AI_VALID));
    GigeAI_RevokeControl();cfg=m[AI_CONFIG];GigeAI_RevokeControl();assert(cfg==m[AI_CONFIG]);
    assert(!(cfg&1));assert(!readreg(GIGE_F_AI_ENABLE));
    /* Web changes are atomic, preserve capture leases, and defer to GVCP. */
    GigeAI_Init(m);assert(m[14]==0x31424557U);
    m[AI_HEARTBEAT]=1;m[AI_SUPPORTED]=255;GigeAI_Poll(1);
    m[651]=1;m[647]=17;m[648]=600000;m[649]=1000000;m[650]=550000;m[644]=1;
    assert(GigeAI_WebPoll(1,0));assert(m[645]==1 && !m[646]);
    assert(readreg(GIGE_F_AI_ENABLE)==17);assert(readreg(GIGE_F_AI_SCORE)==600000);
    assert(!(m[AI_CONFIG]&1));GigeAI_Offer(frame,320,240,960,0x02180014,1,1);
    assert(GigeAI_Pinned(frame));
    m[647]=4;m[644]=2;assert(!GigeAI_WebPoll(2,1));
    assert(m[646]==0x8006U && readreg(GIGE_F_AI_ENABLE)==17);
    m[647]=128;m[644]=3;assert(!GigeAI_WebPoll(3,0));assert(m[646]==0x8002U);
    m[647]=1;m[644]=4;assert(GigeAI_WebPoll(4,0));assert(GigeAI_Pinned(frame));
    assert(!GigeAI_WebPoll(3000001,0));assert(!readreg(GIGE_F_AI_ENABLE));
    assert(GigeAI_Pinned(frame));
    puts("AI web control: PASS (atomic configuration, ownership priority, expiry, capture lease)");
    puts("AI control: PASS (leases, timeout, malformed boxes, immutable latch, configuration, unavailable worker)");
}
