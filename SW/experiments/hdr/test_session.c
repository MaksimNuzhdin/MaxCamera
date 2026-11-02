#include "bracket_session.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint16_t short_raw[4]={116,316,1001,16},long_raw[4]={416,1023,1023,16};
static HdrFrame request[2]={
 {4,1,7,9,0,0,0,1,16,35550,16,1000,0,1},
 {4,1,7,9,0,0,0,4,16,35550,16,1000,0,1}
};
static void begin(HdrSession *s,uint64_t now)
{assert(!HdrSession_Begin(s,request,short_raw,long_raw,4,now,100,50));}
static uint64_t arm(HdrSession *s,uint64_t now)
{
 uint64_t token;HdrFrame command;uint16_t *p=NULL;
 assert(!HdrSession_Configure(s,now,&token,&command));
 assert(!command.effective_exposure_verified && !command.frame_id && !command.timestamp_us);
 assert(HdrSession_Configure(s,now,&token,&command)); /* no implicit retry */
 assert(HdrSession_Arm(s,token,now,&p) && !p);
 assert(!HdrSession_Configured(s,token,now,1));
 assert(!HdrSession_Arm(s,token,now,&p) && p==s->leases[s->phase].data);
 assert(HdrSession_Arm(s,token,now,&p));return token;
}
static HdrFrame evidence(unsigned phase)
{HdrFrame f=request[phase];f.effective_exposure_verified=1;f.frame_id=10+phase;f.timestamp_us=1000+phase*10;return f;}
int main(void)
{
 HdrSession s;HdrSession_Init(&s);begin(&s,0);
 assert(HdrSession_Begin(&s,request,short_raw,long_raw,4,0,100,50));
 uint64_t t=arm(&s,1);HdrFrame f=evidence(0);
 assert(!HdrSession_Complete(&s,t,2,&f));assert(s.leases[0].state==HDR_HELD);
 uint64_t old=t;t=arm(&s,3);f=evidence(1);
 assert(HdrSession_Complete(&s,old,4,&f));assert(s.leases[1].state==HDR_DMA);
 assert(!HdrSession_Complete(&s,t,4,&f));assert(s.state==HDR_READY);
 uint32_t radiance[4];uint8_t flags[4];
 assert(!HdrBracket_Merge(&s.frames[0],&s.frames[1],s.leases[0].data,s.leases[1].data,4,s.maximum_gap_us,radiance,flags));
 assert(radiance[0]==100*65536U && radiance[1]==300*65536U);
 HdrSession_Cancel(&s);assert(s.state==HDR_FAILED && s.leases[0].state==HDR_HELD && s.leases[1].state==HDR_HELD);
 assert(!HdrSession_Release(&s));assert(s.leases[0].state==HDR_FREE);
 /* Late completion cannot release/relabel the next session's DMA lease. */
 begin(&s,10);uint64_t current=arm(&s,11);f=evidence(0);
 assert(current!=old && HdrSession_Complete(&s,old,12,&f));
 HdrSession_Cancel(&s);assert(s.state==HDR_DRAINING && s.leases[0].state==HDR_DMA);
 assert(HdrSession_Release(&s));assert(HdrSession_Quiesced(&s,old));
 assert(!HdrSession_Quiesced(&s,current));assert(s.state==HDR_FAILED);
 assert(!HdrSession_Release(&s));
 /* Delayed callback beyond the deadline fails without needing a prior Poll. */
 begin(&s,0);t=arm(&s,1);f=evidence(0);
 assert(HdrSession_Complete(&s,t,100,&f));assert(s.state==HDR_DRAINING);
 assert(!HdrSession_Quiesced(&s,t));assert(!HdrSession_Release(&s));
 /* Timeout across uint64 clock wrap; command cancellation needs an ack too. */
 begin(&s,UINT64_MAX-49);HdrFrame command;
 assert(!HdrSession_Configure(&s,UINT64_MAX-48,&t,&command));
 HdrSession_Poll(&s,49);assert(s.state==HDR_CONFIGURE);
 HdrSession_Poll(&s,50);assert(s.state==HDR_DRAINING);
 assert(HdrSession_Configured(&s,t,50,1));assert(HdrSession_Release(&s));
 assert(!HdrSession_Quiesced(&s,t));assert(!HdrSession_Release(&s));
 /* Cached exposure, wrong config, reversed frames and excessive pair age fail. */
 for(unsigned bad=0;bad<5;++bad) {
  begin(&s,0);t=arm(&s,1);f=evidence(0);
  if(bad==0)f.effective_exposure_verified=0;
  if(bad==1)f.exposure_lines++;
  if(bad<2)assert(HdrSession_Complete(&s,t,2,&f));
  else {
   assert(!HdrSession_Complete(&s,t,2,&f));t=arm(&s,3);f=evidence(1);
   if(bad==2)f.configuration_generation++;
   if(bad==3)f.frame_id=9;
   if(bad==4)f.timestamp_us=1051;
   assert(HdrSession_Complete(&s,t,4,&f));
  }
  assert(s.state==HDR_FAILED);assert(!HdrSession_Release(&s));
 }
 /* Invalid starts are atomic, including aliases/overlap and generation exhaustion. */
 HdrSession saved=s;
 assert(HdrSession_Begin(&s,request,short_raw,short_raw,4,0,100,50));
 assert(!memcmp(&s,&saved,sizeof(s)));
 assert(HdrSession_Begin(&s,request,short_raw,short_raw+1,4,0,100,50));
 assert(HdrSession_Begin(&s,request,short_raw,long_raw,4,0,50,100));
 assert(HdrSession_Begin(&s,request,short_raw,long_raw,4,0,UINT64_MAX,50));
 assert(!memcmp(&s,&saved,sizeof(s)));
 s.generation=UINT64_MAX/2;
 assert(HdrSession_Begin(&s,request,short_raw,long_raw,4,0,100,50));
 puts("PASS single-shot HDR lease/configuration contract: pair merge, cancellation drain, stale tokens, late callbacks, wrap, verified exposure and atomic rejection; offline only");
}
