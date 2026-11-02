#include "LedsController.h"
#include <string.h>
#define LED_OUTPUTS 0x0fU
static void output(LEDS_CONTROLLER *l)
{
    /* Board LEDs are active-low. The fourth LED is gated by the image-specific streaming API. */
    XGpio_DiscreteWrite(&l->m_gpio,l->m_channel,(~l->m_correntState)&LED_OUTPUTS);
}
MAX_RESULT leds_controller_init(LEDS_CONTROLLER *l,uint32_t base,uint32_t mask,uint32_t channel,LEDS_MODE mode)
{
    if(!l || (channel!=1 && channel!=2) || mode>LEDS_MODE_STATUS)return MAX_RESULT_FAIL;
    memset(l,0,sizeof(*l));l->m_base=base;l->m_mask=mask;l->m_channel=channel;l->m_mode=mode;
    if(XGpio_Initialize(&l->m_gpio,base)!=XST_SUCCESS)return MAX_RESULT_FAIL;
    /* Program inactive data before enabling GPIO outputs: no startup flash. */
    output(l);XGpio_SetDataDirection(&l->m_gpio,channel,mask);return MAX_RESULT_OK;
}
MAX_RESULT leds_controller_set_mode(LEDS_CONTROLLER *l,LEDS_MODE mode)
{
    if(!l || mode>LEDS_MODE_STATUS)return MAX_RESULT_FAIL;
    l->m_mode=mode;l->m_correntState=0;l->m_direction=0;l->m_streaming=0;output(l);return MAX_RESULT_OK;
}
MAX_RESULT leds_controller_set_status(LEDS_CONTROLLER *l,int sensor,int gige)
{
    if(!l || l->m_mode!=LEDS_MODE_STATUS)return MAX_RESULT_FAIL;
    uint32_t state=(l->m_correntState&9U)|(sensor?2U:0U)|(gige?4U:0U);
    if(state!=l->m_correntState) {l->m_correntState=state;output(l);}
    return MAX_RESULT_OK;
}
MAX_RESULT leds_controller_process(LEDS_CONTROLLER *l)
{
    if(!l)return MAX_RESULT_FAIL;
    switch(l->m_mode) {
    case LEDS_MODE_STATUS:l->m_correntState^=1U;break;
    case LEDS_MODE_OFF:l->m_correntState=0;break;
    case LEDS_MODE_BLINK_ALL:l->m_correntState^=LED_OUTPUTS;break;
    case LEDS_MODE_FORWARD_BACKWARD:
        if(!l->m_correntState)l->m_correntState=1;
        else {
            if(l->m_correntState==1)l->m_direction=0;
            if(l->m_correntState==8)l->m_direction=1;
            l->m_correntState=l->m_direction?l->m_correntState>>1:l->m_correntState<<1;
        }
        break;
    default:return MAX_RESULT_FAIL;
    }
    output(l);return MAX_RESULT_OK;
}

MAX_RESULT leds_controller_stream(LEDS_CONTROLLER *l,int independent,int streaming,uint64_t now)
{
    if(!l || l->m_mode!=LEDS_MODE_STATUS)return MAX_RESULT_FAIL;
    uint32_t previous=l->m_correntState;
    if(!independent || !streaming) {
        l->m_streaming=0;l->m_correntState&=~8U;
    } else if(!l->m_streaming) {
        l->m_streaming=1;l->m_stream_tick=now;l->m_correntState|=8U;
    } else {
        /* Unsigned elapsed time tolerates wrap. Skip missed edges, never catch up
         * in a loop or delay the camera poll. 250ms half-period gives 2Hz. */
        uint64_t elapsed=now-l->m_stream_tick;
        if(elapsed>=250000U) {
            uint64_t periods=elapsed/250000U;
            l->m_stream_tick+=periods*250000U;
            if(periods&1U)l->m_correntState^=8U;
        }
    }
    if(previous!=l->m_correntState)output(l);
    return MAX_RESULT_OK;
}
