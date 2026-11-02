#include "GigePreset.h"
#include "GigeImage.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t get(const uint8_t *p) {return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put(uint8_t *p,uint32_t v) {for(unsigned i=0;i<4;++i)p[i]=v>>(8*i);}
int main(int argc,char **argv) {
    assert(argc==2);uint8_t old[4096],data[4096];FILE *f=fopen(argv[1],"rb");assert(f);
    unsigned size=fread(old,1,sizeof(old),f);fclose(f);assert(size==1764);
    const unsigned timing=32+GIGE_PRESET_COUNT*4+84+12;
    assert(get(old+timing)==0x39505349U && get(old+timing+4)==28);
    GigePreset p;assert(!GigePreset_Decode(&p,old,size));
    assert(p.fpga.value[GIGE_ISP_ADAPTIVE_TONE]==0 && p.fpga.value[GIGE_ISP_TONE_TARGET]==128);
    for(unsigned i=0;i<28;++i)assert(p.fpga.value[i]==get(old+timing+8+4*i));
    unsigned tables=timing+8+28*4;
    for(unsigned i=0;i<9;++i)assert(p.fpga.matrix[i]==(int32_t)get(old+tables+4*i));
    for(unsigned i=0;i<3;++i)assert(p.fpga.offsets[i]==(int32_t)get(old+tables+36+4*i));
    assert(!memcmp(p.fpga.gamma,old+tables+48,768));
    for(unsigned i=0;i<252;++i)assert(p.fpga.lsc[i]==(uint16_t)(old[tables+816+2*i]|old[tables+817+2*i]<<8));
    const unsigned counts[]={6,15,18,22,23,24,25,26};
    for(unsigned version=1;version<=8;++version) {
        unsigned count=counts[version-1],n=timing+8+count*4;
        memcpy(data,old,n);put(data+timing,0x30505349U+(version<<24));put(data+timing+4,count);
        if(version>=3){memcpy(data+n,old+tables,816);n+=816;}
        if(version>=5){memcpy(data+n,old+tables+816,504);n+=504;}
        put(data+28,n);assert(!GigePreset_Decode(&p,data,n));
        assert(p.fpga.value[GIGE_ISP_ADAPTIVE_TONE]==0 && p.fpga.value[GIGE_ISP_TONE_TARGET]==128);
        assert(p.fpga.value[GIGE_ISP_ENABLE]==(version==1?0:1));
        // Preserve historical native YUV bypass for profiles before ISP7.
        put(data+16,GIGE_PIXEL_YUYV8);assert(!GigePreset_Decode(&p,data,n));
        assert(p.fpga.value[GIGE_ISP_ENABLE]==(version<7?0:1));
    }
    memcpy(data,old,size);put(data+16,GIGE_PIXEL_YUYV8);
    assert(!GigePreset_Decode(&p,data,size) && p.fpga.value[GIGE_ISP_ENABLE]==1);
    assert(!GigePreset_Decode(&p,old,size));
    p.fpga.value[GIGE_ISP_ADAPTIVE_TONE]=1;p.fpga.value[GIGE_ISP_TONE_TARGET]=176;
    unsigned n=GigePreset_Encode(&p,data,sizeof(data));assert(n==1772 && get(data+timing)==0x41505349U && get(data+timing+4)==30);
    GigePreset back;assert(!GigePreset_Decode(&back,data,n));assert(!memcmp(&back,&p,sizeof(p)));
    GigePreset sentinel=back;data[timing]^=1;assert(GigePreset_Decode(&back,data,n));assert(!memcmp(&sentinel,&back,sizeof(back)));
    data[timing]^=1;assert(GigePreset_Decode(&back,data,n-1));
    p.fpga.value[GIGE_ISP_TONE_TARGET]=15;assert(!GigePreset_Encode(&p,data,sizeof(data)));
    puts("PASS genuine ISP9 tables/settings migration, ISP1-8 defaults/YUV semantics, ISPA tone roundtrip, corrupt/truncated atomic rejection");
}
