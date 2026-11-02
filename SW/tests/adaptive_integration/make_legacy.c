/* Compile against the sealed v9 codec/header to create a genuine old record. */
#include "GigePreset.h"
#include "GigeImage.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
int main(int argc,char **argv) {
    assert(argc==2);GigePreset p;memset(&p,0,sizeof(p));
    p.width=640;p.height=480;p.pixel_format=GIGE_PIXEL_RGB8;p.frame_rate=14293;p.line_period_ns=35550;
    for(unsigned i=0;i<GIGE_PRESET_COUNT;++i)p.values[i]=(uint32_t)gige_feature_descriptors[(gige_preset_addresses[i]-GIGE_FEATURE_FIRST)/4].minimum;
    p.ae_thresholds[0]=48;p.ae_thresholds[1]=40;p.ae_thresholds[2]=96;
    p.ae_thresholds[3]=48;p.ae_thresholds[4]=38;p.ae_thresholds[5]=20;
    p.jpeg_quality=4;
    for(unsigned i=0;i<16;++i)p.gamma[i]=i*16;
    GigeFpgaConfig_Default(&p.fpga);p.fpga.value[GIGE_ISP_ENABLE]=1;
    p.fpga.value[GIGE_ISP_SCALER]=1;p.fpga.value[GIGE_ISP_INTERPOLATION]=1;
    p.fpga.value[GIGE_ISP_CLAHE]=1;p.fpga.value[GIGE_ISP_CLAHE_CLIP]=7;
    p.fpga.value[GIGE_ISP_RED]=384;p.fpga.value[GIGE_ISP_CCM]=1;p.fpga.value[GIGE_ISP_GAMMA]=1;
    for(unsigned i=0;i<9;++i)p.fpga.matrix[i]=(i%4==0)?1310720:-(int32_t)i;
    p.fpga.offsets[0]=-7;p.fpga.offsets[1]=11;
    for(unsigned i=0;i<768;++i)p.fpga.gamma[i]=(i*13)&255;
    for(unsigned i=0;i<252;++i)p.fpga.lsc[i]=(i*37)%16385;
    unsigned char bytes[4096];unsigned n=GigePreset_Encode(&p,bytes,sizeof(bytes));assert(n==1764);
    FILE *f=fopen(argv[1],"wb");assert(f && fwrite(bytes,1,n,f)==n);fclose(f);
    puts("PASS genuine sealed-v9 encoder fixture: ISP9/28 fields/1764 bytes");
}
