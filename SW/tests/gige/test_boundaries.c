#include "GigeVision.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t frame[3][19200],q[524],ack[524];
static GigeVision g;
static unsigned leaders,resent;
static const unsigned blocks[]={65535,1,2};
static int send_packet(void *u,uint32_t ip,uint16_t port,const uint8_t *p,size_t n,int df)
{
 (void)u;(void)ip;(void)port;(void)n;(void)df;
 if(Gige_Get16(p)==0x100){assert(Gige_Get16(p+2)==65535 && !(Gige_Get32(p+4)&0xffffff));++resent;}
 else if(p[4]==1){assert(leaders<3 && Gige_Get16(p+2)==blocks[leaders]);++leaders;}
 return 0;
}
static void command(unsigned op,unsigned id,unsigned bytes)
{
 q[0]=0x42;q[1]=1;Gige_Put16(q+2,op);Gige_Put16(q+4,bytes);Gige_Put16(q+6,id);int b;
 assert(GigeVision_Control(&g,(GigePeer){0x0a000007,50000},q,bytes+8,ack,&b)>=8);
 assert(!Gige_Get16(ack));
}
int main(void)
{
 GigeIO io={0};io.send_stream=send_packet;GigeVision_Init(&g,&io);
 GigeVision_Tick(&g,0xfffffffeULL);
 Gige_Put32(q+8,GIGE_REG_TIMESTAMP_CURRENT_HI);Gige_Put16(q+12,0);Gige_Put16(q+14,8);command(GIGE_CMD_READMEM,65535,8);
 assert(!Gige_Get32(ack+12) && Gige_Get32(ack+16)==0xfffffffeU);
 GigeVision_Tick(&g,0x100000002ULL);command(GIGE_CMD_READMEM,1,8);assert(Gige_Get32(ack+12)==1 && Gige_Get32(ack+16)==2);
 g.privilege=2;g.owner=(GigePeer){0x0a000007,50000};g.owner_time=g.now;g.active=1;g.width=160;g.height=120;g.payload_size=sizeof(frame[0]);g.pixel_format=GIGE_PIXEL_MONO8;
 g.destination=g.owner.ip;g.destination_port=50000;g.block_id=65534;g.packet_delay=0;
 for(unsigned i=0;i<3;++i){assert(!GigeVision_SubmitFrame(&g,frame[i],GigeVision_TimestampAt(&g,g.now)));while(GigeVision_FrameBusy(&g))GigeVision_StreamPoll(&g);}
 memset(q+8,0,12);Gige_Put16(q+10,65535);command(GIGE_CMD_RESEND,2,12);
 GigeVision_StreamPoll(&g);assert(resent==1 && leaders==3);
 Gige_Put32(q+8,GIGE_REG_TIMESTAMP_CONTROL);Gige_Put32(q+12,1);command(GIGE_CMD_WRITEREG,3,8);
 Gige_Put32(q+8,GIGE_REG_TIMESTAMP_CURRENT_HI);Gige_Put16(q+12,0);Gige_Put16(q+14,8);command(GIGE_CMD_READMEM,4,8);assert(!Gige_Get32(ack+12) && !Gige_Get32(ack+16));
 puts("PASS: coherent 64-bit timestamp across low-word rollover/reset, request-ID rollover, classic block 65535->1->2, pre-wrap frame resend after wrap");
}
