#include "GigeImage.h"
#include "GigeVisionUnitDefines.h"
#include <string.h>
unsigned GigeImage_BytesPerPixel(uint32_t f)
{
    switch(f) {
    case GIGE_PIXEL_MONO8:case GIGE_PIXEL_BG8:return 1;
    case GIGE_PIXEL_RGB8:case GIGE_PIXEL_BGR8:case GIGE_PIXEL_YUV444:return 3;
    case GIGE_PIXEL_UYVY8:case GIGE_PIXEL_YUYV8:case GIGE_PIXEL_RGB565:
    case GIGE_PIXEL_RGB555:case GIGE_PIXEL_RGB444:case GIGE_PIXEL_YVYU8:case GIGE_PIXEL_VYUY8:case GIGE_PIXEL_BG10:return 2;
    default:return 0;
    }
}
int GigeImage_Raw(uint32_t f) {return f==GIGE_PIXEL_BG8 || f==GIGE_PIXEL_BG10 || f==GIGE_PIXEL_BG10P;}
unsigned GigeImage_WidthMax(uint32_t f) {return f==GIGE_PIXEL_YUV444?640:GIGE_UNIT_WIDTH_MAX;}
unsigned GigeImage_HeightMax(uint32_t f) {return f==GIGE_PIXEL_YUV444?480:GIGE_UNIT_HEIGHT_MAX;}
unsigned GigeImage_WidthIncrement(uint32_t f) {return f==GIGE_PIXEL_YUV420?8:f==GIGE_PIXEL_BG10P || f==GIGE_PIXEL_BG10?16:f==GIGE_PIXEL_YUV444?4:2;}
unsigned GigeImage_RowBytes(uint32_t w,uint32_t f)
{
    if(f==GIGE_PIXEL_BG10P)return w*5/4;
    if(f==GIGE_PIXEL_YUV420)return w*3/2;
    return w*GigeImage_BytesPerPixel(f);
}
unsigned GigeImage_Payload(uint32_t w,uint32_t h,uint32_t f) {return GigeImage_RowBytes(w,f)*h;}
int GigeImage_Valid(uint32_t w,uint32_t h,uint32_t f)
{
    return w>=GIGE_UNIT_WIDTH_MIN && w<=GigeImage_WidthMax(f) && !(w%GigeImage_WidthIncrement(f)) &&
           h>=GIGE_UNIT_HEIGHT_MIN && h<=GigeImage_HeightMax(f) && !(h&1U) && GigeImage_RowBytes(w,f)!=0;
}
static uint8_t clip(int v) {return (uint8_t)(v<0?0:v>255?255:v);}
void GigeImage_Convert(uint8_t *d,const uint8_t *s,uint32_t n,uint32_t f)
{
    if(f==GIGE_PIXEL_YUYV8) {memcpy(d,s,n*2);return;}
    for(uint32_t i=0;i<n;i+=2,s+=4) {
        if(f==GIGE_PIXEL_MONO8) {*d++=s[0];*d++=s[2];continue;}
        if(f==GIGE_PIXEL_UYVY8) {*d++=s[1];*d++=s[0];*d++=s[3];*d++=s[2];continue;}
        int u=(int)s[1]-128,v=(int)s[3]-128;
        for(unsigned k=0;k<2;++k) {
            int y=256*(int)s[k*2];
            uint8_t r=clip((y+359*v+128)>>8),g=clip((y-88*u-183*v+128)>>8),b=clip((y+454*u+128)>>8);
            if(f==GIGE_PIXEL_RGB565) {
                /* PFNC RGB565p: R in least significant 5 bits, little endian. */
                uint16_t p=(uint16_t)((r>>3)|((g>>2)<<5)|((b>>3)<<11));
                *d++=(uint8_t)p;*d++=(uint8_t)(p>>8);
            } else {*d++=f==GIGE_PIXEL_BGR8?b:r;*d++=g;*d++=f==GIGE_PIXEL_BGR8?r:b;}
        }
    }
}

void GigeImage_FromCsi(uint8_t *d,const uint8_t *s,uint32_t w,uint32_t h,uint32_t stride,uint32_t f)
{
    uint32_t row=GigeImage_RowBytes(w,f);
    for(uint32_t y=0;y<h;++y) {
        const uint8_t *p=s+y*stride;uint8_t *q=d+y*row;
        if(f==GIGE_PIXEL_BG10 || f==GIGE_PIXEL_BG10P) {
            for(uint32_t x=0;x<w;x+=4,p+=5) {
                uint16_t v[4];for(unsigned k=0;k<4;++k)v[k]=(uint16_t)((p[k]<<2)|((p[4]>>(2*k))&3));
                if(f==GIGE_PIXEL_BG10)for(unsigned k=0;k<4;++k){*q++=(uint8_t)v[k];*q++=(uint8_t)(v[k]>>8);}
                else { /* PFNC10p: continuous little-endian bitstream, NOT CSI RAW10. */
                    *q++=(uint8_t)v[0];*q++=(uint8_t)((v[0]>>8)|(v[1]<<2));
                    *q++=(uint8_t)((v[1]>>6)|(v[2]<<4));*q++=(uint8_t)((v[2]>>4)|(v[3]<<6));*q++=(uint8_t)(v[3]>>2);
                }
            }
        } else if(f==GIGE_PIXEL_RGB8 || f==GIGE_PIXEL_BGR8) {
            for(uint32_t x=0;x<w;x+=2,p+=4) { /* YUYV in reversed CSI32 byte lanes. */
                int u=(int)p[2]-128,v=(int)p[0]-128;
                for(unsigned k=0;k<2;++k) {
                    int t=256*(int)p[k?1:3];
                    uint8_t r=clip((t+359*v+128)>>8),g=clip((t-88*u-183*v+128)>>8),b=clip((t+454*u+128)>>8);
                    *q++=f==GIGE_PIXEL_RGB8?r:b;*q++=g;*q++=f==GIGE_PIXEL_BGR8?r:b;
                }
            }
        } else if(f==GIGE_PIXEL_YUV444) {
            for(uint32_t x=0;x<w;++x,p+=3){*q++=p[1];*q++=p[0];*q++=p[2];} /* PFNC U,Y,V */
        } else if(f==GIGE_PIXEL_UYVY8 || f==GIGE_PIXEL_YUYV8 || f==GIGE_PIXEL_YVYU8 || f==GIGE_PIXEL_VYUY8 || f==GIGE_PIXEL_YUV420) {
            /* Both legacy420 and422 use MS-byte-first CSI32 memory words.
             * A lane1/2 swap alone keeps bars plausible but reorders luma
             * within each eight-pixel group, producing jagged live edges. */
            for(uint32_t x=0;x<row;x+=4){q[x]=p[x+3];q[x+1]=p[x+2];q[x+2]=p[x+1];q[x+3]=p[x];}
        } else if(f==GIGE_PIXEL_RGB565) {
            for(uint32_t x=0;x<w;++x,p+=2) { /* sensor BE R5G6B5 -> PFNC LE B5G6R5 */
                uint16_t v=(uint16_t)((p[0]<<8)|p[1]);v=(uint16_t)((v>>11)|(v&0x7e0)|((v&31)<<11));
                *q++=(uint8_t)v;*q++=(uint8_t)(v>>8);
            }
        } else memcpy(q,p,row); /* Y8, RAW8, custom big-endian RGB words */
    }
}

uint32_t GigeImage_JpegBytes(const uint8_t *p,uint32_t n)
{
    if(n<4 || p[0]!=255 || p[1]!=216)return 0;
    uint32_t at=2;int entropy=0;
    while(at<n) {
        if(entropy) {
            while(at<n && p[at]!=255)++at;
            if(at+1>=n)return 0;
            uint8_t c=p[at+1];
            if(c==0 || (c>=0xd0 && c<=0xd7)){at+=2;continue;}
            if(c==255){++at;continue;}
            entropy=0;
        }
        if(p[at]!=255)return 0;
        while(at<n && p[at]==255)++at;
        if(at>=n)return 0;
        uint8_t c=p[at++];if(c==0xd9)return at;
        if(!c || c==0xd8)return 0;
        if(c==1 || (c>=0xd0 && c<=0xd7))continue;
        if(at+2>n)return 0;
        uint32_t size=((uint32_t)p[at]<<8)|p[at+1];
        if(size<2 || size>n-at)return 0;
        at+=size;if(c==0xda)entropy=1;
    }
    return 0;
}
