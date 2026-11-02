#include "raw_lsc.hpp"
#include <cassert>
#include <algorithm>
#include <iostream>

static void check(int rows,int cols,bool enable,int pattern) {
    unsigned short gains[4][7][9];
    for(int p=0;p<4;++p)for(int y=0;y<7;++y)for(int x=0;x<9;++x)
        gains[p][y][x]=pattern==0?4096:pattern==1?4096+p*1024:2048+p*512+y*128+x*256;
    hls::stream<RawPixel> input,output;
    for(int y=0;y<rows;++y)for(int x=0;x<cols;++x) {
        RawPixel p;p.data=(x*37+y*53)%1024;p.keep=3;p.strb=3;p.user=0;p.last=0;p.id=0;p.dest=0;input.write(p);
    }
    camera_raw_lsc(input,output,rows,cols,enable,gains);
    for(int y=0;y<rows;++y)for(int x=0;x<cols;++x) {
        unsigned cx=uint64_t(x)*8*65536/(cols-1),cy=uint64_t(y)*6*65536/(rows-1);
        unsigned ix=std::min(7U,cx>>16),iy=std::min(5U,cy>>16),fx=cx-ix*65536,fy=cy-iy*65536;
        unsigned phase=(y%2)*2+x%2;
        uint64_t t=uint64_t(gains[phase][iy][ix])*(65536-fx)+uint64_t(gains[phase][iy][ix+1])*fx;
        uint64_t b=uint64_t(gains[phase][iy+1][ix])*(65536-fx)+uint64_t(gains[phase][iy+1][ix+1])*fx;
        unsigned gain=(t*(65536-fy)+b*fy+(1ULL<<31))>>32;
        unsigned value=(x*37+y*53)%1024,expected=enable?std::min(1023U,(value*gain+2048)>>12):value;
        assert(!output.empty());auto p=output.read();
        assert(p.data==expected && p.keep==3 && p.strb==3 && p.user==(!x&&!y) && p.last==(x==cols-1));
    }
    assert(input.empty() && output.empty());
}
int main() {
    check(16,32,false,2);check(16,32,true,0);check(16,32,true,1);
    check(16,32,true,2);check(8,MAX_COLS,true,2);check(2,2,true,2);
    hls::stream<RawPixel> input,output;unsigned short gains[4][7][9]={};
    camera_raw_lsc(input,output,1,16,true,gains);assert(input.empty() && output.empty());
    std::cout<<"PASS RAW LSC: unity/bypass, four BGGR phases, bilinear grid/edges, rounding/saturation, markers, full width, bounds\n";
}
