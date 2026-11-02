#include "GigeSensorTelemetry.h"
#include <string.h>
static const uint16_t registers[GIGE_TELEMETRY_REGISTERS]={
    0x3500,0x3501,0x3502,0x350a,0x350b,0x3503,0x3400,
    0x3401,0x3402,0x3403,0x3404,0x3405,0x3406,0x56a1};
int GigeSensorTelemetry_Init(GigeSensorTelemetry *s,SccbAsync *bus,uint8_t address,
    uint32_t interval_us,void *user,void (*publish)(void *,uint16_t,uint8_t))
{
    if(!s || !bus || !bus->io.release || !bus->io.levels || SccbAsync_Owned(bus) ||
       address<8 || address>0x77 || !interval_us || !publish)return SCCB_ASYNC_ARGUMENT;
    memset(s,0,sizeof(*s));s->bus=bus;s->address=address;
    s->interval_us=interval_us;s->user=user;s->publish=publish;return 0;
}
void GigeSensorTelemetry_Poll(GigeSensorTelemetry *s,uint64_t now,int allowed)
{
    if(!s || !s->bus)return;
    if(s->time_valid && now<s->last_now) {
        s->last_error=SCCB_ASYNC_ARGUMENT;
        s->error_register=s->pending?registers[s->pending_index]:0;
        GigeSensorTelemetry_Pause(s,now);
    }
    s->last_now=now;s->time_valid=1;
    if(s->pending) {
        uint8_t value=0;int result=0;
        SccbAsync_Poll(s->bus,now);
        if(!SccbAsync_Take(s->bus,&value,&result))return;
        s->pending=0;
        if(!s->paused && !result) {
            s->values[s->pending_index]=value;s->valid|=(uint16_t)(1U<<s->pending_index);
            s->publish(s->user,registers[s->pending_index],value);
        } else if(result && result!=SCCB_ASYNC_CANCELLED) {
            s->last_error=result;s->error_register=registers[s->pending_index];
        }
        /* Completion never starts another transaction in the same poll. */
        return;
    }
    if(s->paused || !allowed || now<s->next_due || SccbAsync_Owned(s->bus))return;
    int result=SccbAsync_BeginRead(s->bus,s->address,registers[s->index],now);
    if(result){s->last_error=result;s->error_register=registers[s->index];return;}
    s->pending_index=s->index;s->index=(uint8_t)((s->index+1U)%GIGE_TELEMETRY_REGISTERS);
    s->pending=1;
    s->next_due=s->interval_us>UINT64_MAX-now?UINT64_MAX:now+s->interval_us;
}
void GigeSensorTelemetry_Pause(GigeSensorTelemetry *s,uint64_t now)
{
    if(!s || !s->bus)return;
    s->paused=1;if(s->pending)SccbAsync_Cancel(s->bus,now);
}
void GigeSensorTelemetry_Resume(GigeSensorTelemetry *s,uint64_t now)
{
    if(!s || !s->bus || s->pending || SccbAsync_Owned(s->bus))return;
    s->paused=0;s->last_now=s->next_due=now;s->time_valid=1;
}
int GigeSensorTelemetry_Read(const GigeSensorTelemetry *s,uint16_t reg,uint8_t *value)
{
    if(!s || !value)return 0;
    for(unsigned i=0;i<GIGE_TELEMETRY_REGISTERS;++i)
        if(registers[i]==reg && (s->valid&(1U<<i))){*value=s->values[i];return 1;}
    return 0;
}
