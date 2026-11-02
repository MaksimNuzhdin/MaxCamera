#include "GigeVision.h"
#include "GigeVisionXmlRom.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static GigeVision g;
static GigePeer a={0x0a000007,44000},b={0x0a000008,44000};
static uint8_t q[600],ack[600],frame[GIGE_UNIT_PAYLOAD_SIZE],received[GIGE_UNIT_PAYLOAD_SIZE];
static unsigned starts,stops,packets,offset,test_packets;
static uint16_t id=1;
static int blocked,config_failure,staging_blocked;
static uint8_t staging[1472];
static uint8_t *stage(void *u){(void)u;return staging_blocked?0:staging;}
static uint16_t feature_write(void *u,uint32_t addr,uint32_t value) {(void)u;(void)addr;(void)value;return config_failure?GIGE_STATUS_BUSY:0;}
static int acquisition(void *u,int enable) {(void)u;if(enable)++starts;else ++stops;return 0;}
static int output(void *u,uint32_t ip,uint16_t port,const uint8_t *p,size_t n,int df)
{
    (void)u;(void)df;assert(ip==a.ip && port==50000);
    if(g.io.stream_buffer)assert(p==staging);
    if(blocked)return -1;
    unsigned kind=p[4];assert(Gige_Get16(p)==0);
    if(!Gige_Get16(p+2)) {assert(n==1472 && kind==3);++test_packets;return 0;}
    unsigned packet=Gige_Get32(p+4)&0xffffff;
    assert(packet==packets++);
    if(kind==1) {assert(n==44);assert(Gige_Get32(p+20)==GIGE_UNIT_PIXEL_FORMAT);}
    else if(kind==3) {assert(offset+n-8<=sizeof(received));memcpy(received+offset,p+8,n-8);offset+=(unsigned)n-8;}
    else {assert(kind==2 && n==16 && offset==sizeof(frame));assert(Gige_Get32(p+12)==g.height);}
    return 0;
}
static size_t command(GigePeer peer,unsigned cmd,size_t n)
{
    q[0]=0x42;q[1]=1;Gige_Put16(q+2,(uint16_t)cmd);Gige_Put16(q+4,(uint16_t)n);
    Gige_Put16(q+6,id++);int broadcast=0;
    size_t bytes=GigeVision_Control(&g,peer,q,n+8,ack,&broadcast);
    assert(bytes>=8 && Gige_Get16(ack+2)==cmd+1 && Gige_Get16(ack+6)==id-1);
    return bytes;
}
static unsigned write_as(GigePeer peer,uint32_t addr,uint32_t value)
{Gige_Put32(q+8,addr);Gige_Put32(q+12,value);command(peer,GIGE_CMD_WRITEREG,8);return Gige_Get16(ack);}
static unsigned write_reg(uint32_t addr,uint32_t value) {return write_as(a,addr,value);}
static void password_tests(void)
{
    Gige_Put32(q+8,0x12000);memset(q+12,0,32);memcpy(q+12,"DinxTechCam",11);
    assert(command(b,0x86,36)==8 && Gige_Get16(ack)==GIGE_STATUS_ACCESS_DENIED && !GigeVision_FactoryAccess(&g));
    assert(command(a,0x86,36)==12 && !Gige_Get16(ack) && GigeVision_FactoryAccess(&g));
    assert(g.factory_until==g.now+300000000ULL && g.replay_next==0);
    q[12]='x';command(b,0x86,36);assert(Gige_Get16(ack)==GIGE_STATUS_ACCESS_DENIED && GigeVision_FactoryAccess(&g));
    command(a,0x86,36);assert(Gige_Get16(ack)==GIGE_STATUS_ACCESS_DENIED && !GigeVision_FactoryAccess(&g));
    q[12]='D';command(a,0x86,36);assert(!Gige_Get16(ack));
    Gige_Put32(q+8,0x12000);command(a,0x86,4);assert(Gige_Get16(ack) && !GigeVision_FactoryAccess(&g));
    memset(q+12,0,32);memcpy(q+12,"DinxTechCam",11);command(a,0x86,36);assert(!Gige_Get16(ack));
    uint64_t expiry=g.factory_until;g.owner_time=expiry;GigeVision_Tick(&g,expiry);
    assert(g.privilege && !GigeVision_FactoryAccess(&g));
    Gige_Put32(q+8,0x12000);Gige_Put16(q+12,0);Gige_Put16(q+14,32);
    command(a,GIGE_CMD_READMEM,8);assert(Gige_Get16(ack)==GIGE_STATUS_INVALID_ADDRESS);
    Gige_Put32(q+8,0x12000);memset(q+12,0,32);memcpy(q+12,"DinxTechCam",11);command(a,0x86,36);assert(!Gige_Get16(ack));
    assert(!write_reg(GIGE_REG_CCP,0) && !GigeVision_FactoryAccess(&g));
    assert(!write_reg(GIGE_REG_CCP,2));
}
int main(void)
{
    GigeIO io={0,acquisition,output,0,feature_write,0};GigeVision_Init(&g,&io);GigeVision_Tick(&g,1000000);
    assert(command(a,GIGE_CMD_DISCOVERY,0)==256);
    assert(Gige_Get32(ack+8+GIGE_REG_MAC_LOW)==GIGE_UNIT_MAC_LOW);
    assert(write_as(b,GIGE_REG_HEARTBEAT,1000)==GIGE_STATUS_ACCESS_DENIED);
    assert(write_reg(GIGE_REG_CCP,2)==0);
    assert(write_as(b,GIGE_REG_CCP,1)==GIGE_STATUS_ACCESS_DENIED);
    password_tests();
    assert(write_reg(GIGE_REG_WIDTH,1)==GIGE_STATUS_INVALID_PARAM);
    assert(g.width==2592 && g.height==1944 && g.payload_size==2592U*1944U*2U);
    assert(write_reg(GIGE_REG_WIDTH,640)==0 && g.payload_size==640U*1944U*2U);
    assert(write_reg(GIGE_REG_HEIGHT,480)==0);
    assert(write_reg(GIGE_REG_PIXEL_FORMAT,GIGE_PIXEL_RGB8)==0 && g.payload_size==640U*480U*3U);
    config_failure=1;assert(write_reg(GIGE_REG_WIDTH,1280)==GIGE_STATUS_BUSY && g.width==640);
    config_failure=0;
    assert(write_reg(GIGE_REG_PIXEL_FORMAT,0x01100003)==GIGE_STATUS_INVALID_PARAM);
    assert(write_reg(GIGE_REG_WIDTH,GIGE_UNIT_WIDTH)==0);
    assert(write_reg(GIGE_REG_HEIGHT,GIGE_UNIT_HEIGHT)==0);
    assert(write_reg(GIGE_REG_PIXEL_FORMAT,GIGE_UNIT_PIXEL_FORMAT)==0);
    assert(write_reg(GIGE_REG_ACQUISITION_COMMAND,1)==GIGE_STATUS_INVALID_PARAM);
    assert(write_reg(GIGE_REG_DESTINATION_IP,a.ip)==0);
    assert(write_reg(GIGE_REG_STREAM_PORT,50000)==0);
    assert(write_reg(GIGE_REG_PACKET_SIZE,9000)==GIGE_STATUS_INVALID_PARAM);
    assert(write_reg(GIGE_REG_PACKET_SIZE,1500|GIGE_PACKET_FIRE_TEST)==0);
    GigeVision_StreamPoll(&g);assert(test_packets==1 && !g.test_pending);
    assert(write_reg(GIGE_REG_TL_PARAMS_LOCKED,1)==0);
    assert(write_reg(GIGE_REG_PACKET_DELAY,1)==GIGE_STATUS_ACCESS_DENIED);
    assert(write_reg(GIGE_REG_WIDTH,640)==GIGE_STATUS_ACCESS_DENIED);
    assert(write_reg(GIGE_REG_PIXEL_FORMAT,GIGE_PIXEL_MONO8)==GIGE_STATUS_ACCESS_DENIED);
    assert(write_reg(GIGE_REG_ACQUISITION_COMMAND,1)==0 && starts==1);
    int broadcast=0;
    assert(GigeVision_Control(&g,a,q,16,ack,&broadcast)==12 && starts==1);
    for(unsigned i=0;i<sizeof(frame);++i)frame[i]=(uint8_t)(i^(i>>8));
    assert(GigeVision_SubmitFrame(&g,frame,GigeVision_TimestampAt(&g,g.now))==0);
    blocked=1;GigeVision_StreamPoll(&g);assert(packets==0 && g.packet_id==0);
    blocked=0;
    while(GigeVision_FrameBusy(&g)) {GigeVision_Tick(&g,g.now+50);GigeVision_StreamPoll(&g);}
    assert(packets==(sizeof(frame)+1463)/1464+2 && g.frames_sent==1 && !memcmp(frame,received,sizeof(frame)));
    /* Real row padding crosses packet boundaries. Failed sends must retain
     * the pinned source and logical offset for an identical retry. */
    static uint8_t padded[(GIGE_UNIT_WIDTH*2U+16U)*GIGE_UNIT_HEIGHT];
    uint32_t rb=g.width*2U, pitch=rb+16U;
    memset(padded,0xa5,sizeof(padded));
    for(unsigned y=0;y<g.height;++y)memcpy(padded+y*pitch,frame+y*rb,rb);
    packets=offset=0;g.io.stream_buffer=stage;
    assert(GigeVision_SubmitFrameStrided(&g,padded,rb,pitch-32,GigeVision_TimestampAt(&g,g.now))==-1);
    assert(GigeVision_SubmitFrameStrided(&g,padded,rb-1,pitch,GigeVision_TimestampAt(&g,g.now))==-1);
    assert(GigeVision_SubmitFrameStrided(&g,padded,rb,pitch,GigeVision_TimestampAt(&g,g.now))==0);
    while(GigeVision_FrameBusy(&g)) {
        GigeVision_Tick(&g,g.now+50);
        uint32_t old_offset=g.frame_offset,old_id=g.packet_id;
        staging_blocked=1;GigeVision_StreamPoll(&g);
        assert(g.frame_offset==old_offset && g.packet_id==old_id);
        staging_blocked=0;
        blocked=1;GigeVision_StreamPoll(&g);
        assert(g.frame==padded && g.frame_offset==old_offset && g.packet_id==old_id);
        blocked=0;GigeVision_StreamPoll(&g);
    }
    assert(g.frames_sent==2 && !memcmp(frame,received,sizeof(frame)));
    assert(write_reg(GIGE_REG_ACQUISITION_COMMAND,0)==0 && stops==1);
    Gige_Put32(q+8,GIGE_REG_CURRENT_IP);command(b,GIGE_CMD_READREG,4);
    assert(!Gige_Get16(ack));
    uint8_t cached_monitor_read[12];memcpy(cached_monitor_read,q,12);
    assert(write_reg(GIGE_REG_CCP,1)==0);
    assert(GigeVision_Control(&g,b,cached_monitor_read,12,ack,&broadcast)==8);
    assert(Gige_Get16(ack)==GIGE_STATUS_ACCESS_DENIED);
    Gige_Put32(q+8,GIGE_REG_CURRENT_IP);command(b,GIGE_CMD_READREG,4);
    assert(Gige_Get16(ack)==GIGE_STATUS_ACCESS_DENIED);
    command(b,GIGE_CMD_DISCOVERY,0);assert(!Gige_Get16(ack));
    /* Readmem ROM exact bytes including aligned final transfer. */
    for(unsigned off=0;off<GIGE_XML_LENGTH;off+=512) {
        unsigned n=GIGE_XML_LENGTH-off;if(n>512)n=512;unsigned padded=(n+3)&~3U;
        Gige_Put32(q+8,GIGE_XML_BASE+off);Gige_Put16(q+12,0);Gige_Put16(q+14,(uint16_t)padded);
        assert(command(a,GIGE_CMD_READMEM,8)==12+padded);
        assert(!Gige_Get16(ack) && !memcmp(ack+12,gige_xml_rom+off,n));
    }
    Gige_Put32(q+8,0xfffffffc);Gige_Put16(q+12,0);Gige_Put16(q+14,512);
    command(a,GIGE_CMD_READMEM,8);assert(Gige_Get16(ack)==GIGE_STATUS_INVALID_ADDRESS);
    GigeVision_Tick(&g,g.now+4000000);assert(g.privilege==0 && !g.destination && !g.locked);
    assert(write_as(b,GIGE_REG_CCP,2)==0);
    assert(write_as(b,GIGE_REG_CCP,0)==0);
    /* ForceIP MAC filtering and byte layout. */
    memset(q+8,0,56);Gige_Put16(q+10,(uint16_t)GIGE_UNIT_MAC_HIGH);Gige_Put32(q+12,GIGE_UNIT_MAC_LOW);
    Gige_Put32(q+28,0x0a000032);Gige_Put32(q+44,0xffffff00);Gige_Put32(q+60,0x0a000001);
    command(a,GIGE_CMD_FORCEIP,56);assert(!Gige_Get16(ack) && g.force_ip_pending && g.force_ip==0x0a000032);
    /* Length/range/alignment fuzzing over real parser under ASan/UBSan. */
    srand(1);
    for(unsigned k=0;k<50000;++k) {
        size_t n=(size_t)(rand()%sizeof(q));
        for(size_t j=0;j<n;++j)q[j]=(uint8_t)rand();
        if(k%2==0 && n>=8) {q[0]=0x42;Gige_Put16(q+4,(uint16_t)(n-8));Gige_Put16(q+2,(uint16_t)(0x80+2*(k%4)));}
        memset(ack,0xa5,sizeof(ack));
        size_t bytes=GigeVision_Control(&g,a,q,n,ack,&broadcast);assert(bytes<=GIGE_ACK_MAX);
        for(size_t j=GIGE_ACK_MAX;j<sizeof(ack);++j)assert(ack[j]==0xa5);
    }
    puts("PASS: discovery, ownership, heartbeat, XML, ForceIP, test packet, full GVSP frame, retries, parser fuzz");
}
