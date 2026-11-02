#include "color_pipeline_v9.hpp"
#include "color_pipeline_v5.hpp"
#include "raw_pipeline_v5.hpp"
#include "color_adjust.hpp"
#include "rgb_detail.hpp"
#include "raw_lsc.hpp"
#include <cassert>
#include <algorithm>
#include <iostream>

static unsigned sequence=0,next_red=256,next_blue=256;
static void run(bool ccm,bool gm,bool awb,unsigned expected_red,unsigned expected_blue,
                bool dark=false,int rows=16,int cols=32,bool bilateral=false,unsigned strength=0,bool lsc=false) {
    hls::stream<RawPixel> input,base_input,corrected,shaded,prepared;
    unsigned short grid[4][7][9];
    for(int p=0;p<4;++p)for(int y=0;y<7;++y)for(int x=0;x<9;++x)grid[p][y][x]=p==0?8192:p==3?2048:4096;
    hls::stream<RgbPixel> output,base,colored,filtered,detailed,reference;
    hls::stream<ColorPixel> color_input;
    int matrix[3][3]={{0,0,1048576},{0,1048576,0},{1048576,0,0}};
    int offsets[3]={0,0,0};unsigned char gamma[768];
    for(int c=0;c<3;++c)for(int i=0;i<256;++i)gamma[c*256+i]=(i*i+127)/255;
    for(int y=0;y<rows;++y)for(int x=0;x<cols;++x) {
        RawPixel p;p.data=dark?0:(y&1)?((x&1)?768:384):((x&1)?384:128);
        p.keep=3;p.strb=3;p.user=!y&&!x;p.last=x==cols-1;p.id=0;p.dest=0;
        input.write(p);base_input.write(p);
    }
    unsigned long long sum,rs,gs,bs;unsigned high,low,count,ar,ag,ab,seq;
    camera_raw_correct_v5(input,prepared,rows,cols,true,0,256,256,256,lsc,grid);
    camera_color_pipeline_v9(prepared,output,rows,cols,256,256,256,true,
        3,3,cols-6,rows-6,4,8,sum,high,low,ccm,gm,awb,matrix,offsets,gamma,
        rs,gs,bs,count,ar,ag,ab,seq,bilateral,24,256,strength);
    assert(ar==expected_red && ag==256 && ab==expected_blue && seq==2*++sequence);
    camera_raw_correct(base_input,corrected,rows,cols,true,0,256,256,256);
    camera_raw_lsc(corrected,shaded,rows,cols,lsc,grid);
    camera_raw_isp(shaded,base,rows,cols,false,0,256,256,256);
    unsigned long long expected_sums[3]={0,0,0};unsigned expected_count=0;
    while(!base.empty()) {
        RgbPixel p=base.read();ColorPixel q;q.data=0;
        unsigned values[3];
        for(int c=0;c<3;++c)values[c]=unsigned(p.data.range(c*8+7,c*8))*4;
        if(values[0]>=8 && values[1]>=8 && values[2]>=8 && values[0]<1000 && values[1]<1000 && values[2]<1000) {
            for(int c=0;c<3;++c)expected_sums[c]+=values[c];++expected_count;
        }
        const unsigned gains[3]={expected_red,256,expected_blue};
        for(int c=0;c<3;++c)q.data.range(c*10+9,c*10)=std::min(1023U,(values[c]*gains[c]+128)>>8);
        q.keep=15;q.strb=15;q.user=p.user;q.last=p.last;q.id=0;q.dest=0;color_input.write(q);
    }
    assert(rs==expected_sums[0] && gs==expected_sums[1] && bs==expected_sums[2] && count==expected_count);
    if(awb && expected_count>=64 && rs && gs && bs) {
        unsigned tr=std::max(64U,std::min(1024U,unsigned((gs*256+rs/2)/rs)));
        unsigned tb=std::max(64U,std::min(1024U,unsigned((gs*256+bs/2)/bs)));
        next_red=(3*expected_red+tr+2)/4;next_blue=(3*expected_blue+tb+2)/4;
    }
    unsigned long long ignored;unsigned ih,il;
    camera_color_adjust(color_input,colored,rows,cols,ccm,gm,matrix,offsets,gamma,ignored,ignored,ignored,ih);
    camera_rgb_denoise(colored,filtered,rows,cols,true);
    camera_rgb_detail(filtered,detailed,rows,cols,bilateral,24,256,strength);
    camera_ai_resize(detailed,reference,rows,cols,3,3,cols-6,rows-6,4,8,ignored,ih,il);
    assert(sum==ignored && high==ih && low==il);
    for(int y=0;y<4;++y)for(int x=0;x<8;++x) {
        assert(!output.empty() && !reference.empty());
        auto p=output.read(),q=reference.read();assert(p.data==q.data);
        assert(p.keep==7 && p.user==(!x&&!y) && p.last==(x==7));
    }
    assert(input.empty() && output.empty() && reference.empty());
}

static void compare_legacy(int rows,int cols,unsigned black,bool bpc,bool awb) {
    hls::stream<RawPixel> old_input,new_input,prepared;
    hls::stream<RgbPixel> expected,actual;
    unsigned short grid[4][7][9];
    for(int p=0;p<4;++p)for(int y=0;y<7;++y)for(int x=0;x<9;++x)grid[p][y][x]=2300+p*500+y*80+x*100;
    int matrix[3][3]={{1572864,-524288,0},{262144,524288,262144},{-524288,0,1572864}};
    int offsets[3]={2097152,-2097152,3145728};unsigned char gamma[768];
    for(int i=0;i<768;++i)gamma[i]=i<256?255-i:i<512?((i%256)*(i%256)+127)/255:i^85;
    for(int y=0;y<rows;++y)for(int x=0;x<cols;++x) {
        RawPixel p;p.data=(x==cols/2 && y==rows/2)?1023:(x*17+y*29+(x&1)*73+(y&1)*131)%1024;
        p.keep=3;p.strb=3;p.user=!x&&!y;p.last=x==cols-1;p.id=0;p.dest=0;
        old_input.write(p);new_input.write(p);
    }
    struct Stats {unsigned long long sum,rs,gs,bs;unsigned high,low,count,red,green,blue,seq;} a,b;
    camera_color_pipeline_v5(old_input,expected,rows,cols,bpc,black,128,384,768,true,
        3,1,cols-4,rows-2,rows/2,cols/2,a.sum,a.high,a.low,true,true,awb,matrix,offsets,gamma,
        a.rs,a.gs,a.bs,a.count,a.red,a.green,a.blue,a.seq,true,24,256,64,true,grid);
    // Pre-demosaic gains remain unity: color/WB gains belong after demosaic.
    camera_raw_correct_v5(new_input,prepared,rows,cols,bpc,black,256,256,256,true,grid);
    camera_color_pipeline_v9(prepared,actual,rows,cols,128,384,768,true,
        3,1,cols-4,rows-2,rows/2,cols/2,b.sum,b.high,b.low,true,true,awb,matrix,offsets,gamma,
        b.rs,b.gs,b.bs,b.count,b.red,b.green,b.blue,b.seq,true,24,256,64);
    assert(a.sum==b.sum && a.high==b.high && a.low==b.low && a.rs==b.rs && a.gs==b.gs && a.bs==b.bs && a.count==b.count);
    assert(a.red==b.red && a.green==b.green && a.blue==b.blue && b.seq==2*++sequence);
    for(int y=0;y<rows/2;++y)for(int x=0;x<cols/2;++x) {
        auto a=expected.read(),b=actual.read();
        assert(a.data==b.data && a.keep==b.keep && a.strb==b.strb && a.user==b.user && a.last==b.last);
    }
    assert(old_input.empty() && new_input.empty() && prepared.empty() && expected.empty() && actual.empty());
}

int main() {
    run(false,false,false,256,256);run(true,false,false,256,256);
    run(true,true,false,256,256,false,16,MAX_COLS);
    run(false,true,false,256,256);run(true,true,false,256,256);
    for(int i=0;i<12;++i)run(false,false,true,next_red,next_blue);
    assert(next_red>=64 && next_red<160 && next_blue>650 && next_blue<=1024);
    unsigned old_red=next_red,old_blue=next_blue;
    run(false,false,true,next_red,next_blue,true);
    assert(next_red==old_red && next_blue==old_blue);
    run(false,false,false,256,256);
    run(false,false,true,256,256); // Mode transition restarts AWB at unity.

    run(true,true,false,256,256,false,16,32,true,0);
    run(true,true,false,256,256,false,16,32,false,64);
    run(true,true,false,256,256,false,16,32,true,256);
    run(true,true,false,256,256,false,16,32,true,64,true);
    run(false,false,false,256,256,false,16,MAX_COLS,false,0,true);
    hls::stream<RawPixel> input;hls::stream<RgbPixel> output;
    unsigned short grid[4][7][9]={};
    int m[3][3]={};int o[3]={};unsigned char lut[768]={};
    unsigned long long sum,rs,gs,bs;unsigned h,l,n,r,g,b,s;
    camera_color_pipeline_v9(input,output,8,16,256,256,256,false,
        15,0,2,8,4,8,sum,h,l,false,false,false,m,o,lut,rs,gs,bs,n,r,g,b,s,false,24,256,0);
    assert(input.empty() && output.empty() && !sum && !n && !s);
    compare_legacy(8,16,0,false,false);
    compare_legacy(32,64,31,true,false);
    compare_legacy(16,MAX_COLS,31,true,false);
    compare_legacy(MAX_ROWS,16,31,true,false);
    compare_legacy(32,64,1023,true,false);
    for(int i=0;i<3;++i)compare_legacy(32,64,31,true,true);
    std::cout<<"PASS shared RAW/color v9: RAW LSC, bilateral/sharpening, RGB10 CCM/gamma, median/ROI/area composition, raw statistics, AWB start, framing, invalid geometry\n";
}
