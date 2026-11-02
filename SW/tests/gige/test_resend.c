#include "GigeVision.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static GigeVision g;
static uint8_t wire[64][1472],staging[1472],raw[170*120],ack[524],request[128];
static size_t lengths[64];
static unsigned originals,retransmissions,unavailable;
static int blocked;
static GigePeer owner={0x0a000007,40000},receiver={0x0a000007,50000},stranger={0x0a000008,50000};
static uint8_t *stage(void *u){(void)u;return staging;}
static int output(void *u,uint32_t ip,uint16_t port,const uint8_t *p,size_t n,int df)
{
 (void)u;(void)df;assert(ip==owner.ip && port==50000);if(blocked)return -1;
 uint32_t id=Gige_Get32(p+4)&0xffffff;
 if(Gige_Get16(p)==0x800c){assert(n==8);++unavailable;return 0;}
 if(Gige_Get16(p)==0x100){assert(id<originals && n==lengths[id] && !memcmp(p+2,wire[id]+2,n-2));++retransmissions;return 0;}
 assert(!Gige_Get16(p) && id==originals && id<64);memcpy(wire[id],p,n);lengths[id]=n;++originals;return 0;
}
static uint16_t cmd(GigePeer p,unsigned code,const void *body,unsigned bytes)
{
 static uint16_t id=1;request[0]=0x42;request[1]=1;Gige_Put16(request+2,code);Gige_Put16(request+4,bytes);Gige_Put16(request+6,id++);
 if(bytes)memcpy(request+8,body,bytes);int broadcast;
 assert(GigeVision_Control(&g,p,request,bytes+8,ack,&broadcast)>=8);return Gige_Get16(ack);
}
static uint16_t resend(GigePeer p,uint16_t block,unsigned first,unsigned last)
{uint8_t b[12]={0};Gige_Put16(b+2,block);Gige_Put32(b+4,first);Gige_Put32(b+8,last);return cmd(p,GIGE_CMD_RESEND,b,12);}
static void drain(void)
{for(unsigned i=0;g.resend_count && i<1000;++i){GigeVision_Tick(&g,g.now+1);GigeVision_StreamPoll(&g);}assert(!g.resend_count);}
int main(void)
{
 GigeIO io={0,0,output,0,0,stage};GigeVision_Init(&g,&io);g.privilege=1;g.owner=owner;g.destination=owner.ip;g.destination_port=50000;
 g.packet_delay=0;g.active=1;g.width=160;g.height=120;g.pixel_format=GIGE_PIXEL_MONO8;g.payload_size=19200;
 for(unsigned i=0;i<sizeof(raw);++i)raw[i]=(uint8_t)(i^(i>>7));
 assert(!GigeVision_SubmitFrameStrided(&g,raw,160,170,123456));
 while(GigeVision_FrameBusy(&g)){GigeVision_Tick(&g,g.now+50);GigeVision_StreamPoll(&g);}
 assert(originals==16);
 assert(resend(stranger,g.block_id,0,15)==GIGE_STATUS_ACCESS_DENIED);
 assert(resend(receiver,g.block_id,4,3)==GIGE_STATUS_INVALID_PARAM);
 assert(!resend(receiver,g.block_id,0,15));blocked=1;GigeVision_StreamPoll(&g);assert(!retransmissions && g.resend_count==1);blocked=0;drain();assert(retransmissions==16);
 /* Snapshot geometry/format is retained; retransmission must match old wire. */
 g.width=320;g.height=240;g.pixel_format=GIGE_PIXEL_RGB8;
 assert(!resend(receiver,g.block_id,0,0));drain();assert(retransmissions==17);
 for(unsigned i=0;i<GIGE_RESEND_QUEUE;++i)assert(!resend(receiver,g.block_id,1,1));
 assert(resend(receiver,g.block_id,1,1)==GIGE_STATUS_BUSY);drain();assert(retransmissions==25 && g.resend_overflow==1);
 GigeVision_ForgetBuffer(&g,raw);memset(raw,0,sizeof(raw));assert(!resend(receiver,g.block_id,1,2));drain();assert(unavailable==1 && retransmissions==25);
 /* Invalid and expired block requests terminate with one bounded error. */
 assert(!resend(receiver,0x1234,0,0xffffff));drain();assert(unavailable==2);
 g.width=160;g.height=120;g.compression=1;g.pixel_format=GIGE_PIXEL_YUYV8;g.payload_size=38400;originals=0;
 assert(!GigeVision_SubmitFrameSized(&g,raw,777,987654));while(GigeVision_FrameBusy(&g)){GigeVision_Tick(&g,g.now+50);GigeVision_StreamPoll(&g);}
 assert(originals==3);assert(!resend(receiver,g.block_id,0,2));drain();assert(retransmissions==28);
 GigeVision_Release(&g);assert(!g.resend_count);for(unsigned i=0;i<GIGE_HISTORY_COUNT;++i)assert(!g.history[i].data);
 puts("PASS: byte-exact strided/JPEG leader-data-trailer resends, receiver socket authorization, retry, queue bounds, expired-buffer refusal, snapshot metadata and release");
}
