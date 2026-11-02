#include "LedsController.h"
#include <assert.h>
#include <stdio.h>
static unsigned writes;
int XGpio_Initialize(XGpio *g,uint32_t base) {g->base=base;return 0;}
void XGpio_SetDataDirection(XGpio *g,unsigned channel,uint32_t mask) {assert(channel==1);g->direction=mask;}
void XGpio_DiscreteWrite(XGpio *g,unsigned channel,uint32_t data) {assert(channel==1);g->value=data;++writes;}
int main(void)
{
    LEDS_CONTROLLER a,b;
    assert(!leds_controller_init(&a,1,0,1,LEDS_MODE_STATUS));assert(a.m_gpio.value==15);
    assert(!leds_controller_set_status(&a,1,1));assert(a.m_correntState==6 && a.m_gpio.value==9);
    unsigned before=writes;
    assert(!leds_controller_set_status(&a,1,1) && writes==before);
    for(unsigned n=0;n<10;++n) {
        assert(!leds_controller_process(&a));
        assert((a.m_correntState&6)==6 && !(a.m_correntState&8));
        assert(a.m_gpio.value==((~a.m_correntState)&15));
    }
    leds_controller_set_status(&a,0,1);assert((a.m_correntState&6)==4);
    leds_controller_set_status(&a,0,0);assert((a.m_correntState&6)==0);
    leds_controller_init(&b,2,0,1,LEDS_MODE_STATUS);leds_controller_process(&b);
    assert(b.m_correntState==1 && a.m_correntState==0); /* instance independence */
    leds_controller_set_mode(&a,LEDS_MODE_OFF);assert(a.m_gpio.value==15);
    leds_controller_set_mode(&a,LEDS_MODE_STATUS);
    /* Old FPGA bit3 is reset: never illuminate it through this API. */
    leds_controller_stream(&a,0,1,0);assert(!(a.m_correntState&8));
    leds_controller_set_status(&a,1,1);
    leds_controller_stream(&a,1,1,100);assert(a.m_correntState==14);
    before=writes;
    leds_controller_stream(&a,1,1,250099);assert(writes==before);
    leds_controller_stream(&a,1,1,250100);assert(a.m_correntState==6);
    leds_controller_process(&a);assert(a.m_correntState==7);
    leds_controller_stream(&a,1,1,500100);assert(a.m_correntState==15);
    leds_controller_set_status(&a,0,1);assert(a.m_correntState==13);
    /* Two missed half-periods preserve phase without catch-up writes. */
    before=writes;leds_controller_stream(&a,1,1,1000100);assert(writes==before);
    leds_controller_stream(&a,1,0,1000101);assert(a.m_correntState==5);
    leds_controller_stream(&a,1,1,UINT64_MAX-100000);assert(a.m_correntState==13);
    leds_controller_stream(&a,1,1,149999);assert(a.m_correntState==5);
    leds_controller_stream(&a,1,1,150000);assert(a.m_correntState==5);
    leds_controller_stream(&a,0,1,400000);assert(!(a.m_correntState&8));
    leds_controller_set_mode(&a,LEDS_MODE_OFF);assert(a.m_gpio.value==15);
    assert(leds_controller_stream(&a,1,1,500000)==MAX_RESULT_FAIL);
    puts("PASS stream LED: legacy gate, 2Hz cadence, delayed polls, wrap, immediate stop, status/heartbeat preservation");
    puts("PASS: active-low LEDs, deterministic startup, independent heartbeat/readiness, no redundant GPIO writes");
}
