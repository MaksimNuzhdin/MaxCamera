#include "rgb_detail.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>

static std::vector<unsigned> run(const std::vector<unsigned>& values,int rows,int cols,
                                 bool bilateral,unsigned strength) {
    hls::stream<RgbPixel> input,output;
    for (unsigned v:values) {RgbPixel p;p.data=v;p.keep=7;p.strb=7;p.user=0;p.last=0;p.id=0;p.dest=0;input.write(p);}
    camera_rgb_detail(input,output,rows,cols,bilateral,24,256,strength);
    std::vector<unsigned> result;
    for(int y=0;y<rows;++y)for(int x=0;x<cols;++x) {
        assert(!output.empty());auto p=output.read();
        assert(p.keep==7 && p.strb==7 && p.user==(!x&&!y) && p.last==(x==cols-1));
        result.push_back(p.data.to_uint());
    }
    assert(input.empty() && output.empty());return result;
}

static void check(int rows,int cols) {
    std::vector<unsigned> source(rows*cols);
    for(int y=0;y<rows;++y)for(int x=0;x<cols;++x) {
        unsigned p=0;
        for(int c=0;c<3;++c)p|=((x*17+y*31+c*53)%256)<<(8*c);
        source[y*cols+x]=p;
    }
    assert(run(source,rows,cols,false,0)==source);
    for(unsigned strength:{64U,256U}) {
        auto result=run(source,rows,cols,false,strength);
        for(int y=0;y<rows;++y)for(int x=0;x<cols;++x)for(int c=0;c<3;++c) {
            auto sample=[&](int yy,int xx) {return yy<0 || yy>=rows || xx<0 || xx>=cols ? 0 : int((source[yy*cols+xx]>>(8*c))&255);};
            int v=(256+4*strength)*sample(y,x)-strength*(sample(y-1,x)+sample(y+1,x)+sample(y,x-1)+sample(y,x+1));
            int expected=std::max(0,std::min(255,v>>8));
            assert(int((result[y*cols+x]>>(8*c))&255)==expected);
        }
    }
    auto bilateral=run(source,rows,cols,true,0);
    assert(run(source,rows,cols,true,0)==bilateral);
    assert(run(bilateral,rows,cols,false,64)==run(source,rows,cols,true,64));
}

int main() {
    check(16,32);check(8,MAX_COLS);
    const int rows=32,cols=64;
    std::vector<unsigned> noisy(rows*cols);
    for(int y=0;y<rows;++y)for(int x=0;x<cols;++x) {
        int v=(x<cols/2?64:192)+((x*13+y*7)%17)-8;
        noisy[y*cols+x]=unsigned(v)*0x010101;
    }
    auto filtered=run(noisy,rows,cols,true,0);
    long before=0,after=0;
    for(int y=3;y<rows-3;++y)for(int x=3;x<cols-3;++x) {
        int ideal=x<cols/2?64:192;
        int a=int(noisy[y*cols+x]&255)-ideal,b=int(filtered[y*cols+x]&255)-ideal;
        before+=a*a;after+=b*b;
        assert(std::abs(b)<=10);
    }
    assert(after<before);
    hls::stream<RgbPixel> input,output;
    camera_rgb_detail(input,output,8,8,true,0,256,0);
    camera_rgb_detail(input,output,8,8,false,24,256,257);
    assert(input.empty() && output.empty());
    std::cout<<"PASS RGB detail: bypass, signed convolution/saturation, bilateral noise/edge behavior, composition, markers, full width, bounds\n";
}
