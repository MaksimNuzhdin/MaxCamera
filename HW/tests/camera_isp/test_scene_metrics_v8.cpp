#include "scene_metrics_v8.hpp"
#include <cassert>
#include <vector>
#include <iostream>
#include <cstdint>
#include <cstdlib>

static unsigned sequence=0,last_rows=0,last_cols=0;
static std::vector<unsigned> previous;
static unsigned check(int rows,int cols,int pattern,bool enable=true) {
    hls::stream<RgbPixel> input,output;std::vector<unsigned> pixels,luma;
    uint64_t sum=0;unsigned sums[1024]={},counts[1024]={};
    for(int y=0;y<rows;++y)for(int x=0;x<cols;++x) {
        unsigned value=pattern<2?64+10*pattern:pattern==2?((x+y)%2?220:32):128;
        if(pattern==5)value=0;else if(pattern==6)value=255;
        unsigned p=value*0x010101;
        if(pattern==4) {
            unsigned r=(x*23+y*17)%256,g=(x*7+y*29+53)%256,b=(x*31+y*11+107)%256;
            p=r|(g<<8)|(b<<16);value=(77*r+150*g+29*b+128)>>8;
        }
        pixels.push_back(p);luma.push_back(value);
        unsigned bin=(y*32/rows)*32+x*32/cols;sums[bin]+=value;++counts[bin];
        RgbPixel word;word.data=p;word.keep=0;word.strb=0;word.user=0;word.last=0;word.id=0;word.dest=0;input.write(word);
    }
    for(int y=1;y<rows-1;++y)for(int x=1;x<cols-1;++x)
        sum+=std::abs(4*int(luma[y*cols+x])-int(luma[(y-1)*cols+x])-int(luma[(y+1)*cols+x])-int(luma[y*cols+x-1])-int(luma[y*cols+x+1]));
    static bool previous_valid=false;
    bool active=enable && rows>=32 && cols>=32;
    bool compare=previous_valid && active && unsigned(rows)==last_rows && unsigned(cols)==last_cols;
    previous_valid=active;
    unsigned motion=0;std::vector<unsigned> means;
    for(unsigned i=0;i<1024;++i) {
        unsigned mean=counts[i]?(sums[i]+counts[i]/2)/counts[i]:0;means.push_back(mean);
        if(compare)motion+=std::abs(int(mean)-int(previous[i]));
    }
    previous=means;last_rows=rows;last_cols=cols;
    unsigned focus=0,movement=0,valid=0,seq=0,total=0,high=0,low=0;
    camera_scene_metrics_v8(input,output,rows,cols,enable,focus,movement,valid,seq,total,high,low);
    assert(focus==(active?((sum<<16)/((rows-2)*(cols-2))):0) && movement==motion*64 && valid==unsigned(compare));
    uint64_t expected_sum=0;unsigned expected_high=0,expected_low=0;
    for(unsigned i=0;i<pixels.size();++i) {
        unsigned p=pixels[i],r=p&255,g=(p>>8)&255,b=(p>>16)&255;
        expected_sum+=luma[i];expected_high+=(r>=250 || g>=250 || b>=250);expected_low+=(luma[i]<=5);
    }
    assert(total==expected_sum && high==expected_high && low==expected_low);
    assert(seq==2*++sequence);
    for(int y=0;y<rows;++y)for(int x=0;x<cols;++x) {
        assert(!output.empty());auto p=output.read();
        assert(p.data==pixels[y*cols+x] && p.keep==7 && p.strb==7 && p.user==(!x&&!y) && p.last==(x==cols-1));
    }
    assert(input.empty() && output.empty());return focus;
}
int main() {
    assert(check(32,32,0)==0);check(32,32,1);check(32,32,1);
    unsigned sharp=check(32,32,2),smooth=check(32,32,3);assert(sharp>smooth);
    check(32,32,0,false);check(32,32,0,true);check(32,640,0);check(640,32,1);check(34,66,2);check(640,640,4);check(MAX_ROWS,MAX_COLS,4);check(8,16,0);check(8,16,5,false);check(32,32,6,false);
    hls::stream<RgbPixel> input,output;unsigned f=1,m=1,v=1,s=1,total=1,high=1,low=1;
    camera_scene_metrics_v8(input,output,0,32,false,f,m,v,s,total,high,low);
    assert(input.empty() && output.empty() && !f && !m && !v && !s && !total && !high && !low);
    std::cout<<"PASS post-scaler scene v8 metrics: intensity/clipping/darkness, lossless pixels/markers, Laplacian reference, thumbnail motion, stable/changed frames, history reset, geometry changes, bounds\n";
}
