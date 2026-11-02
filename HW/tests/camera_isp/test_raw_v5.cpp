#include "raw_pipeline_v5.hpp"
#include "raw_lsc.hpp"
#include <cassert>
#include <algorithm>
#include <iostream>

static void check(int rows,int cols,bool enable) {
    unsigned short grid[4][7][9];
    for(int p=0;p<4;++p)for(int y=0;y<7;++y)for(int x=0;x<9;++x)
        grid[p][y][x]=2048+p*1024+y*128+x*256;
    hls::stream<RawPixel> input,base_input,corrected,reference,output;
    for(int y=0;y<rows;++y)for(int x=0;x<cols;++x) {
        RawPixel p;p.data=(x*37+y*53)%1024;p.keep=3;p.strb=3;
        p.user=!x&&!y;p.last=x==cols-1;p.id=0;p.dest=0;input.write(p);base_input.write(p);
    }
    camera_raw_correct_v5(input,output,rows,cols,true,31,128,256,512,enable,grid);
    camera_raw_correct(base_input,corrected,rows,cols,true,31,256,256,256);
    camera_raw_lsc(corrected,reference,rows,cols,enable,grid);
    for(int y=0;y<rows;++y)for(int x=0;x<cols;++x) {
        assert(!output.empty() && !reference.empty());auto p=output.read(),q=reference.read();
        unsigned gain=(y&1)?((x&1)?512:256):((x&1)?256:128);
        unsigned expected=std::min(1023U,(q.data.to_uint()*gain+128)>>8);
        assert(p.data==expected && p.keep==3 && p.strb==3 && p.user==(!x&&!y) && p.last==(x==cols-1));
    }
    assert(input.empty() && base_input.empty() && output.empty() && reference.empty());
}
int main() {
    check(16,32,false);check(16,32,true);check(8,MAX_COLS,true);check(MAX_ROWS,16,true);
    hls::stream<RawPixel> input,output;unsigned short grid[4][7][9]={};
    camera_raw_correct_v5(input,output,7,16,true,0,256,256,256,true,grid);
    assert(input.empty() && output.empty());
    std::cout<<"PASS RAW v5: black/BPC/LSC/manual-gain composition, grid interpolation, saturation, bypass, markers, maximum dimensions, bounds\n";
}
