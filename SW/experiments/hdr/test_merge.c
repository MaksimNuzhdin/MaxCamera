#include "bracket_merge.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    HdrFrame s={4,1,7,9,0,10,1000,1,16,35550,16,1000,1,1};
    HdrFrame l=s;l.frame_id=14;l.timestamp_us=2000;l.exposure_lines=4;
    uint16_t sr[]={116,316,1001,16},lr[]={416,1023,1023,16};
    uint32_t out[4]={0};uint8_t flags[4]={0};
    assert(!HdrBracket_Merge(&s,&l,sr,lr,4,1000,out,flags));
    assert(out[0]==100*65536U && out[1]==300*65536U);
    assert(flags[0]==HDR_VALID && flags[1]==HDR_VALID);
    assert(flags[2]==HDR_BOTH_SATURATED && !out[2]);
    assert(flags[3]==HDR_BELOW_BLACK && !out[3]);
    /* Gain and exposure must both participate in normalization. */
    l.gain16=32;lr[0]=816;
    assert(!HdrBracket_Merge(&s,&l,sr,lr,4,1000,out,flags));
    assert(out[0]==100*65536U);
    uint32_t original[4];memcpy(original,out,sizeof(out));
    lr[3]=1024;assert(HdrBracket_Merge(&s,&l,sr,lr,4,1000,out,flags));
    assert(!memcmp(out,original,sizeof(out)));lr[3]=16;
    assert(HdrBracket_Merge(&s,&l,sr,lr,3,1000,out,flags));
    assert(HdrBracket_Validate(&s,&l,999));
    HdrFrame bad=l;bad.effective_exposure_verified=0;assert(HdrBracket_Validate(&s,&bad,1000));
    bad=l;bad.manual_exposure=0;assert(HdrBracket_Validate(&s,&bad,1000));
    bad=l;bad.configuration_generation++;assert(HdrBracket_Validate(&s,&bad,1000));
    bad=l;bad.geometry_generation++;assert(HdrBracket_Validate(&s,&bad,1000));
    bad=l;bad.bayer_phase=1;assert(HdrBracket_Validate(&s,&bad,1000));
    bad=l;bad.frame_id=s.frame_id;assert(HdrBracket_Validate(&s,&bad,1000));
    bad=l;bad.timestamp_us=999;assert(HdrBracket_Validate(&s,&bad,1000));
    bad=l;bad.exposure_lines=1;bad.gain16=16;assert(HdrBracket_Validate(&s,&bad,1000));
    bad=l;bad.line_period_ns++;assert(HdrBracket_Validate(&s,&bad,1000));
    bad=l;bad.black=bad.white;assert(HdrBracket_Validate(&s,&bad,1000));
    puts("PASS RAW10 bracket validation, radiance normalization, saturation fallback and atomic invalid-input rejection; synthetic only");
}
