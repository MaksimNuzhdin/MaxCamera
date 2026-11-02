#include "AxiGpioSccbAsync.h"
#include <string.h>

enum { DRIVE, HIGH, IDLE_DATA, ACK, DATA };
enum { IDLE, ACTIVE, DONE };
static uint64_t after(uint64_t now, uint64_t interval)
{ return interval > UINT64_MAX-now ? UINT64_MAX : now+interval; }
static void op(SccbAsync *s, unsigned kind, unsigned argument, unsigned delay)
{
    SccbAsyncOp *p=&s->ops[s->count++];
    p->kind=(uint8_t)kind;p->argument=(uint8_t)argument;p->delay=(uint16_t)delay;
}
static void start(SccbAsync *s)
{
    op(s,DRIVE,3,0);op(s,HIGH,0,s->half_us);op(s,IDLE_DATA,0,0);
    op(s,DRIVE,1,s->half_us);op(s,DRIVE,0,1);
}
static void stop(SccbAsync *s)
{
    op(s,DRIVE,0,s->half_us);op(s,DRIVE,1,0);
    op(s,HIGH,0,s->half_us);op(s,DRIVE,3,s->half_us);
}
static void bit(SccbAsync *s, unsigned data, int sample)
{
    op(s,DRIVE,data?2:0,s->half_us);op(s,DRIVE,data?3:1,0);
    op(s,HIGH,0,s->half_us);
    if(sample>=0)op(s,(unsigned)sample,0,0);
    op(s,DRIVE,data?2:0,1);
}
static void send(SccbAsync *s, uint8_t value)
{
    for(unsigned n=0;n<8;++n)bit(s,(value>>(7-n))&1U,-1);
    bit(s,1,ACK);
}
int SccbAsync_Init(SccbAsync *s,const SccbAsyncIo *io,unsigned hz,
                   unsigned stretch_us,unsigned transaction_us)
{
    if(!s || !io || !io->release || !io->levels ||
       (hz!=10000 && hz!=50000 && hz!=100000) || !stretch_us ||
       !transaction_us)return SCCB_ASYNC_ARGUMENT;
    memset(s,0,sizeof(*s));s->io=*io;s->half_us=(500000U+hz-1)/hz;
    s->stretch_us=stretch_us;s->transaction_us=transaction_us;return 0;
}
int SccbAsync_Owned(const SccbAsync *s) {return s && s->state!=IDLE;}
int SccbAsync_BeginRead(SccbAsync *s,uint8_t address,uint16_t reg,uint64_t now)
{
    if(!s || !s->io.release || !s->io.levels || address<8 || address>0x77)
        return SCCB_ASYNC_ARGUMENT;
    if(SccbAsync_Owned(s))return SCCB_ASYNC_BUSY;
    s->count=s->position=0;s->value=0;s->result=0;s->waiting=s->cleaning=s->cancelled=0;
    start(s);send(s,(uint8_t)(address<<1));send(s,(uint8_t)(reg>>8));send(s,(uint8_t)reg);
    stop(s);start(s);send(s,(uint8_t)((address<<1)|1));
    for(unsigned n=0;n<8;++n)bit(s,1,DATA);
    bit(s,1,-1); /* Master NACK after the single received byte. */
    s->cleanup=s->count;stop(s);
    s->last_now=s->due=now;s->deadline=after(now,s->transaction_us);s->state=ACTIVE;return 0;
}
static void finish(SccbAsync *s)
{
    if(!s->result && s->cancelled)s->result=SCCB_ASYNC_CANCELLED;
    s->io.release(s->io.user,3);s->state=DONE;
}
static void fail(SccbAsync *s,int result,uint64_t now)
{
    if(!s->result)s->result=result;
    if(s->cleaning){finish(s);return;}
    s->cleaning=1;s->position=s->cleanup;s->waiting=0;s->due=now;
    s->deadline=after(now,(uint64_t)s->stretch_us+3U*s->half_us+4U);
}
void SccbAsync_Cancel(SccbAsync *s,uint64_t now)
{
    if(s && s->state==ACTIVE && !s->cleaning && !s->cancelled) {
        if(now<s->last_now){fail(s,SCCB_ASYNC_ARGUMENT,now);return;}
        s->last_now=now;
        /* Jumping directly to STOP during a read can leave the slave driving
         * SDA low. Finish its byte and master NACK before releasing ownership.
         * Cancellation neither extends deadlines nor hides transport errors. */
        s->cancelled=1;
    }
}
void SccbAsync_Poll(SccbAsync *s,uint64_t now)
{
    if(!s || s->state!=ACTIVE)return;
    if(now<s->last_now){s->last_now=now;fail(s,SCCB_ASYNC_ARGUMENT,now);return;}
    s->last_now=now;
    if(now>=s->deadline){fail(s,SCCB_ASYNC_TIMEOUT,now);return;}
    if(now<s->due)return;
    if(s->position==s->count){finish(s);return;}
    const SccbAsyncOp *p=&s->ops[s->position];
    switch(p->kind) {
    case DRIVE:
        s->io.release(s->io.user,p->argument);
        s->waiting=(p->argument&1U)!=0;
        if(s->waiting) {
            s->stretch_deadline=after(now,s->stretch_us);
            /* Observe an already-high SCL in this poll, before unrelated main
             * loop work can delay the HIGH operation. State 2 means the line
             * reached high within the stretch bound; no sleep is required. */
            if(s->io.levels(s->io.user)&1U)s->waiting=2;
        }
        break;
    case HIGH:
        if(s->waiting==2){s->waiting=0;break;}
        if(!s->waiting){s->waiting=1;s->stretch_deadline=after(now,s->stretch_us);}
        if(!(s->io.levels(s->io.user)&1U)) {
            if(now>=s->stretch_deadline)fail(s,SCCB_ASYNC_TIMEOUT,now);
            return;
        }
        /* A delayed poll must not accept a line after its stretch deadline. */
        if(now>=s->stretch_deadline){fail(s,SCCB_ASYNC_TIMEOUT,now);return;}
        s->waiting=0;break;
    case IDLE_DATA:
        if(!(s->io.levels(s->io.user)&2U)){fail(s,SCCB_ASYNC_BUS,now);return;}
        break;
    case ACK:
        if(s->io.levels(s->io.user)&2U){fail(s,SCCB_ASYNC_NACK,now);return;}
        break;
    case DATA:s->value=(uint8_t)((s->value<<1)|((s->io.levels(s->io.user)>>1)&1U));break;
    }
    ++s->position;s->due=after(now,p->delay);
}
int SccbAsync_Take(SccbAsync *s,uint8_t *value,int *result)
{
    if(!s || s->state!=DONE || !value || !result)return 0;
    *result=s->result;if(!s->result)*value=s->value;s->state=IDLE;return 1;
}
