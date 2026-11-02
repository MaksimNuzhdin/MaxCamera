#include "AxiGpioSccb.h"
#include "xil_io.h"
#include "sleep.h"
#include <stddef.h>

static void lines(AXI_GPIO_SCCB *b,unsigned scl,unsigned sda)
{ Xil_Out32(b->base+4U,(scl?1U:0U)|(sda?2U:0U)); }
static unsigned levels(AXI_GPIO_SCCB *b)
{ b->status=Xil_In32(b->base)&3U;return b->status; }
static int high(AXI_GPIO_SCCB *b)
{
    for(unsigned elapsed=0;elapsed<b->timeout_us;++elapsed) {
        if(levels(b)&1U)return 0;
        usleep(1);
    }
    return -4;
}
static int bit(AXI_GPIO_SCCB *b,unsigned data,unsigned *sample)
{
    lines(b,0,data);usleep(b->half_us);
    lines(b,1,data);
    int e=high(b);if(e)return e;
    usleep(b->half_us);
    if(sample)*sample=(levels(b)>>1)&1U;
    lines(b,0,data);usleep(1);
    return 0;
}
static int stop(AXI_GPIO_SCCB *b)
{
    lines(b,0,0);usleep(b->half_us);
    lines(b,1,0);
    int e=high(b);
    if(!e)usleep(b->half_us);
    lines(b,1,1);usleep(b->half_us);
    return e;
}
static int start(AXI_GPIO_SCCB *b)
{
    lines(b,1,1);
    int e=high(b);if(e)return e;
    usleep(b->half_us);
    if(!(levels(b)&2U))return -3;
    lines(b,1,0);usleep(b->half_us);
    lines(b,0,0);usleep(1);
    return 0;
}
static int send(AXI_GPIO_SCCB *b,uint8_t value)
{
    for(unsigned n=0;n<8;++n) {
        int e=bit(b,(value>>(7U-n))&1U,NULL);if(e)return e;
    }
    unsigned nack=1;
    int e=bit(b,1,&nack);
    return e?e:nack?-5:0;
}
static int receive(AXI_GPIO_SCCB *b,uint8_t *value)
{
    uint8_t result=0;
    for(unsigned n=0;n<8;++n) {
        unsigned sample=0;int e=bit(b,1,&sample);if(e)return e;
        result=(uint8_t)((result<<1)|sample);
    }
    int e=bit(b,1,NULL); /* Master NACK terminates this one-byte read. */
    if(!e)*value=result;
    return e;
}
int AxiGpioSccb_Init(AXI_GPIO_SCCB *b,uint32_t base,uint32_t hz)
{
    if(!b || base!=0xa00d0000U || (hz!=10000 && hz!=50000 && hz!=100000))return -1;
    b->base=base;b->half_us=(500000U+hz-1U)/hz;b->timeout_us=1000;
    b->status=b->interrupts=0;b->phase=0;b->ready=0;
    Xil_Out32(base,0); /* Never drive a logic1, including during recovery. */
    lines(b,1,1);
    int e=high(b);if(e)return e;
    if(!(levels(b)&2U)) {
        /* Explicit bus recovery during Init only, never a transaction retry. */
        for(unsigned n=0;n<9;++n) {e=bit(b,1,NULL);if(e)return e;}
        e=stop(b);if(e)return e;
        if((levels(b)&3U)!=3U)return -3;
    }
    b->ready=1;return 0;
}
static int transaction(AXI_GPIO_SCCB *b,uint8_t address,uint16_t reg,uint8_t *value,int reading)
{
    uint8_t received=0;
    b->phase=1;
    int e=start(b);
    if(!e){b->phase=2;e=send(b,(uint8_t)(address<<1));}
    if(!e){b->phase=3;e=send(b,(uint8_t)(reg>>8));}
    if(!e){b->phase=4;e=send(b,(uint8_t)reg);}
    if(!e && reading) {
        /* SCCB read-address phase ends with STOP. The register pointer stays
         * selected for the subsequent address/read phase. */
        e=stop(b);
        if(!e)e=start(b);
        if(!e){b->phase=5;e=send(b,(uint8_t)((address<<1)|1U));}
        if(!e){b->phase=6;e=receive(b,&received);}
    } else if(!e) {b->phase=5;e=send(b,*value);}
    uint32_t sampled=b->status;
    int finish=stop(b);
    if(e)b->status=sampled;
    if(!e && !finish && reading)*value=received;
    return e?e:finish;
}
static int valid(AXI_GPIO_SCCB *b,uint8_t address,uint16_t reg,const uint8_t *p,uint16_t count)
{
    return b && b->ready && b->base==0xa00d0000U && p && count &&
           address>=8 && address<=0x77 && (uint32_t)reg+count<=65536U;
}
int AxiGpioSccb_Read(void *user,uint8_t address,uint16_t reg,uint8_t *p,uint16_t count)
{
    AXI_GPIO_SCCB *b=(AXI_GPIO_SCCB *)user;
    if(!valid(b,address,reg,p,count))return -1;
    for(unsigned n=0;n<count;++n) {
        int e=transaction(b,address,(uint16_t)(reg+n),p+n,1);if(e)return e;
    }
    return 0;
}
int AxiGpioSccb_Write(void *user,uint8_t address,uint16_t reg,const uint8_t *p,uint16_t count)
{
    AXI_GPIO_SCCB *b=(AXI_GPIO_SCCB *)user;
    if(!valid(b,address,reg,p,count))return -1;
    for(unsigned n=0;n<count;++n) {
        uint8_t value=p[n];
        int e=transaction(b,address,(uint16_t)(reg+n),&value,0);if(e)return e;
    }
    return 0;
}
