#include "GigeImage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    uint8_t src[]={0,128,255,128,76,85,76,255},d[16];
    GigeImage_Convert(d,src,4,GIGE_PIXEL_RGB8);
    assert(d[0]==0 && d[1]==0 && d[2]==0 && d[3]==255 && d[4]==255 && d[5]==255);
    assert(d[6]>=254 && d[7]<=1 && d[8]<=1);
    GigeImage_Convert(d,src+4,2,GIGE_PIXEL_BGR8);assert(d[0]<=1 && d[1]<=1 && d[2]>=254);
    GigeImage_Convert(d,src+4,2,GIGE_PIXEL_RGB565);assert(d[0]==31 && d[1]==0);
    GigeImage_Convert(d,src,4,GIGE_PIXEL_MONO8);assert(!memcmp(d,(uint8_t[]){0,255,76,76},4));
    GigeImage_Convert(d,src,4,GIGE_PIXEL_UYVY8);assert(!memcmp(d,(uint8_t[]){128,0,128,255,85,76,255,76},8));
    GigeImage_Convert(d,src,4,GIGE_PIXEL_YUYV8);assert(!memcmp(d,src,8));
    assert(GigeImage_Valid(2592,1944,GIGE_PIXEL_RGB8));
    assert(GigeImage_Valid(160,120,GIGE_PIXEL_MONO8));
    assert(!GigeImage_Valid(2593,1944,GIGE_PIXEL_RGB8));
    assert(!GigeImage_Valid(640,481,GIGE_PIXEL_RGB8));
    assert(!GigeImage_Valid(640,480,0x01100003));
    puts("PASS: PFNC packing, YUV ordering, black/white/red conversion, range and unsupported-format rejection");
}
