#include "GigeVision.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static GigeVision g;
static uint8_t pixels[3][21120], request[GIGE_CONTROL_MAX], reply[GIGE_ACK_MAX];
static uint8_t leader[64], replayed[64];
static size_t leader_bytes, replayed_bytes;
static unsigned request_id, frame_leaders;
static const GigePeer owner={0x0a000007,50000};

static int send_packet(void *u,uint32_t ip,uint16_t port,const uint8_t *p,size_t n,int df)
{
    (void)u;(void)ip;(void)port;(void)df;
    if(p[4]==1) {
        assert(n<=sizeof(leader));
        if(Gige_Get16(p)==0x100) {memcpy(replayed,p,n);replayed_bytes=n;}
        else {memcpy(leader,p,n);leader_bytes=n;++frame_leaders;}
    }
    return 0;
}
static void tick(uint64_t time)
{
    g.owner_time=time; /* Keep this artificial long-duration clock step owned. */
    GigeVision_Tick(&g,time);
}
static void command(unsigned opcode,unsigned bytes)
{
    request[0]=0x42;request[1]=1;
    Gige_Put16(request+2,(uint16_t)opcode);Gige_Put16(request+4,(uint16_t)bytes);
    Gige_Put16(request+6,(uint16_t)++request_id);
    int broadcast=0;
    assert(GigeVision_Control(&g,owner,request,bytes+8,reply,&broadcast)>=8);
    assert(!Gige_Get16(reply));
}
static void timestamp_command(unsigned bits)
{
    Gige_Put32(request+8,GIGE_REG_TIMESTAMP_CONTROL);Gige_Put32(request+12,bits);
    command(GIGE_CMD_WRITEREG,8);
}
static uint64_t current(void)
{
    Gige_Put32(request+8,GIGE_REG_TIMESTAMP_CURRENT_HI);
    Gige_Put16(request+12,0);Gige_Put16(request+14,8);command(GIGE_CMD_READMEM,8);
    return ((uint64_t)Gige_Get32(reply+12)<<32)|Gige_Get32(reply+16);
}
static uint64_t wire_timestamp(const uint8_t *p)
{return ((uint64_t)Gige_Get32(p+12)<<32)|Gige_Get32(p+16);}
static void drain(void)
{while(GigeVision_FrameBusy(&g))GigeVision_StreamPoll(&g);}
static void init(void)
{
    GigeIO io={0};io.send_stream=send_packet;GigeVision_Init(&g,&io);
    g.privilege=2;g.owner=owner;g.active=1;g.width=160;g.height=120;
    g.payload_size=19200;g.pixel_format=GIGE_PIXEL_MONO8;
    g.destination=owner.ip;g.destination_port=50000;g.packet_delay=0;
    leader_bytes=replayed_bytes=frame_leaders=0;
}
int main(void)
{
    init();tick(1000000);
    uint64_t captured=GigeVision_TimestampAt(&g,g.now);
    tick(1000040);timestamp_command(1);assert(current()==0 && g.active);
    tick(1000080);timestamp_command(1); /* Repeated resets while one DMA is in flight. */
    tick(1000120);
    assert(!GigeVision_SubmitFrameSized(&g,pixels[0],19200,captured));
    /* This fails on the original code: it subtracts the new origin at submission. */
    assert(g.frame_timestamp==captured);
    GigeVision_StreamPoll(&g);assert(wire_timestamp(leader)==captured);
    tick(1000130);timestamp_command(1);assert(g.frame_timestamp==captured && current()==0);
    drain();uint16_t previous_block=g.block_id;uint8_t original[64];
    memcpy(original,leader,leader_bytes);size_t original_bytes=leader_bytes;
    tick(1000150);captured=GigeVision_TimestampAt(&g,g.now);assert(captured==20);
    assert(!GigeVision_SubmitFrameStrided(&g,pixels[1],160,176,captured));
    drain();assert(wire_timestamp(leader)==20);
    memset(request+8,0,12);Gige_Put16(request+10,previous_block);
    command(GIGE_CMD_RESEND,12);GigeVision_StreamPoll(&g);
    assert(replayed_bytes==original_bytes && wire_timestamp(replayed)==1000000);
    /* Only the Resend status differs from the original leader. */
    assert(!memcmp(replayed+2,original+2,original_bytes-2));

    tick(1000200);timestamp_command(2);uint64_t latched=g.timestamp_latch;
    tick(1000250);timestamp_command(1);assert(g.timestamp_latch==latched && !current());
    uint8_t reset_request[16],reset_reply[GIGE_ACK_MAX];
    timestamp_command(1);memcpy(reset_request,request,16);memcpy(reset_reply,reply,12);
    uint64_t origin=g.timestamp_origin;tick(1000300);
    int broadcast=0;
    assert(GigeVision_Control(&g,owner,reset_request,16,reply,&broadcast)==12);
    assert(!memcmp(reply,reset_reply,12) && g.timestamp_origin==origin && current()==50);
    timestamp_command(3);assert(!current() && !g.timestamp_latch);

    origin=g.timestamp_origin;tick(origin+0xfffffffeULL);assert(current()==0xfffffffeULL);
    captured=current();assert(!GigeVision_SubmitFrame(&g,pixels[2],captured));
    tick(origin+0x100000002ULL);assert(current()==0x100000002ULL);
    drain();assert(wire_timestamp(leader)==0xfffffffeULL);
    timestamp_command(1);tick(origin+0x100000009ULL);assert(current()==7);
    g.compression=1;captured=current();
    assert(!GigeVision_SubmitFrameSized(&g,pixels[0],777,captured));
    tick(g.now+1);timestamp_command(1);drain();assert(wire_timestamp(leader)==7);
    assert(g.active && frame_leaders==4);
    puts("PASS: capture-latched timestamps across repeated active resets, in-flight TX, strided/JPEG payloads, immutable pre-reset Resend, latch units, reset retry, and low-word rollover");
}
