#include "color_adjust.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

static void run(bool ccm, bool gamma_enable, int rows, int cols) {
    int matrix[3][3] = {{1572864,-524288,0},{262144,524288,262144},{-524288,0,1572864}};
    int offsets[3] = {2097152,-2097152,3145728};
    unsigned char gamma[768];
    for(int v=0;v<256;++v) {
        gamma[v]=255-v;gamma[256+v]=(v*v+127)/255;gamma[512+v]=v^0x55;
    }
    hls::stream<ColorPixel> input;hls::stream<RgbPixel> output;
    unsigned long long expected_sum[3]={0,0,0};unsigned expected_count=0;
    for(int y=0;y<rows;++y)for(int x=0;x<cols;++x) {
        ColorPixel p;p.data=0;
        unsigned values[3];
        for(int c=0;c<3;++c) {
            values[c]=(y*53+x*37+c*71)%1024;
            p.data.range(c*10+9,c*10)=values[c];
        }
        if(values[0]>=8 && values[1]>=8 && values[2]>=8 &&
           values[0]<1000 && values[1]<1000 && values[2]<1000) {
            ++expected_count;for(int c=0;c<3;++c)expected_sum[c]+=values[c];
        }
        p.keep=15;p.strb=15;p.user=!x&&!y;p.last=x==cols-1;p.id=0;p.dest=0;input.write(p);
    }
    unsigned long long rs=99,gs=99,bs=99;unsigned count=99;
    camera_color_adjust(input,output,rows,cols,ccm,gamma_enable,matrix,offsets,gamma,rs,gs,bs,count);
    assert(rs==expected_sum[0] && gs==expected_sum[1] && bs==expected_sum[2] && count==expected_count);
    for(int y=0;y<rows;++y)for(int x=0;x<cols;++x) {
        assert(!output.empty());RgbPixel p=output.read();
        assert(p.keep==7 && p.strb==7 && p.user==(!x&&!y) && p.last==(x==cols-1));
        unsigned source[3];for(int c=0;c<3;++c)source[c]=(y*53+x*37+c*71)%1024;
        for(int c=0;c<3;++c) {
            // Independent double reference; these quarter-step coefficients
            // are exactly representable by the vendor's Q8 intermediate.
            double value=source[c];
            if(ccm) {
                value=double(offsets[c])/1048576;
                for(int k=0;k<3;++k)value+=double(matrix[c][k])*source[k]/1048576;
                value=std::floor(value);
            }
            unsigned clipped=unsigned(std::max(0.0,std::min(1023.0,value)));
            unsigned expected=std::min(255U,(clipped+2)>>2);
            if(gamma_enable)expected=gamma[c*256+expected];
            assert(p.data.range(c*8+7,c*8)==expected);
        }
    }
    assert(input.empty() && output.empty());
}
int main() {
    run(false,false,12,32);run(true,false,12,32);
    run(false,true,12,32);run(true,true,12,32);
    run(true,true,1,1);run(true,true,8,MAX_COLS);
    hls::stream<ColorPixel> input;hls::stream<RgbPixel> output;
    int matrix[3][3]={};int offsets[3]={};unsigned char gamma[768]={};
    unsigned long long rs=99,gs=99,bs=99;unsigned count=99;
    matrix[0][0]=8388608;
    camera_color_adjust(input,output,8,16,true,true,matrix,offsets,gamma,rs,gs,bs,count);
    assert(input.empty() && output.empty() && !rs && !gs && !bs && !count);
    std::cout<<"PASS color adjustment: RGB10 order, CCM signed mix/saturation, per-channel gamma, bypass, gray-world statistics, markers, full width, bounds\n";
}
