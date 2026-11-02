#include "GigeVision.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static unsigned packets;
static uint8_t received[8];
static int send_packet(void *u,uint32_t ip,uint16_t port,const uint8_t *p,size_t n,int df)
{
    (void)u;(void)ip;(void)port;(void)df;
    uint32_t id=Gige_Get32(p+4)&0xffffff;
    if(!packets){assert(n==40 && id==0 && Gige_Get16(p+10)==6);assert(!Gige_Get32(p+20) && Gige_Get32(p+24)==8);assert(!Gige_Get32(p+28) && Gige_Get32(p+32)==1000000);}
    else if(packets==1){assert(n==16 && id==1);memcpy(received,p+8,8);}
    else {assert(packets==2 && n==12 && id==2 && Gige_Get32(p+8)==6);}
    ++packets;return 0;
}
int main(void)
{
    uint8_t src[32]={0,1,2,255,0xe4},d[32];memcpy(src+16,src,5);memset(d,0xa5,sizeof(d));
    GigeImage_FromCsi(d,src,4,2,16,GIGE_PIXEL_BG10);
    const uint8_t unpacked[]={0,0,5,0,10,0,255,3};
    assert(!memcmp(d,unpacked,8) && !memcmp(d+8,unpacked,8) && d[16]==0xa5);
    GigeImage_FromCsi(d,src,4,2,16,GIGE_PIXEL_BG10P);
    const uint8_t packed[]={0,20,160,192,255};
    assert(!memcmp(d,packed,5) && !memcmp(d+5,packed,5));
    assert(GigeImage_Payload(2592,1944,GIGE_PIXEL_BG10P)==6298560);
    assert(GigeImage_Payload(640,480,GIGE_PIXEL_YUV420)==460800);
    assert(GigeImage_WidthIncrement(GIGE_PIXEL_BG10P)==16);
    assert(GigeImage_WidthMax(GIGE_PIXEL_YUV444)==640 && GigeImage_HeightMax(GIGE_PIXEL_YUV444)==480);
    assert(!GigeImage_Valid(1280,960,GIGE_PIXEL_YUV444));
    assert(!GigeImage_Valid(642,480,GIGE_PIXEL_BG10P));
    assert(!GigeImage_Valid(644,480,GIGE_PIXEL_YUV420));
    /* Spatial ramp crosses three CSI32 words and two padded DMA rows.
     * Constant color bars cannot detect the old eight-pixel permutation. */
    for(unsigned y=0;y<2;++y)for(unsigned x=0;x<12;++x)src[y*16+x]=(uint8_t)(y*16+(x^3));
    memset(d,0xa5,sizeof(d));GigeImage_FromCsi(d,src,8,2,16,GIGE_PIXEL_YUV420);
    for(unsigned y=0;y<2;++y)for(unsigned x=0;x<12;++x)assert(d[y*12+x]==y*16+x);
    assert(d[24]==0xa5);
    const uint8_t red565[]={0xf8,0,0,0x1f};GigeImage_FromCsi(d,red565,2,1,4,GIGE_PIXEL_RGB565);
    assert(d[0]==31 && d[1]==0 && d[2]==0 && d[3]==248);
    /* Marker-like EOI inside a segment and byte-stuffed entropy must not end the JPEG. */
    const uint8_t jpg[]={255,216,255,224,0,4,255,217,255,218,0,2,0x12,255,0,0x34,255,208,0x56,255,217,0,0};
    assert(GigeImage_JpegBytes(jpg,sizeof(jpg))==21);
    for(unsigned n=0;n<21;++n)assert(!GigeImage_JpegBytes(jpg,n));
    GigeVision g;GigeIO io={0};io.send_stream=send_packet;GigeVision_Init(&g,&io);
#if MAX_CAMERA_GIGE_CSI_BYTES
    assert(Gige_Get32(g.bootstrap)==0x00020000U);
#else
    assert(Gige_Get32(g.bootstrap)==0x00010002U);
#endif
    g.active=1;g.compression=1;g.payload_size=32;
    assert(GigeVision_SubmitFrameSized(&g,unpacked,0,0)==-1);
    assert(GigeVision_SubmitFrameSized(&g,unpacked,33,0)==-1);
    assert(!GigeVision_SubmitFrameSized(&g,unpacked,8,0));
    while(GigeVision_FrameBusy(&g)){g.now+=100;GigeVision_StreamPoll(&g);}
    assert(packets==3 && !memcmp(received,unpacked,8) && g.frames_sent==1);
    puts("PASS: CSI/PFNC10p distinction, stride stripping, packed payload sizes, RGB565 endian, bounded JPEG parsing and JPEG GVSP leader/payload/trailer");
}
