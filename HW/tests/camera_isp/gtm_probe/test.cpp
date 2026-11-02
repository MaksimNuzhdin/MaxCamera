#include "probe.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <utility>
static std::vector<unsigned> run(int rows,int cols) {
    hls::stream<ToneInput> input;hls::stream<RgbPixel> output;
    for(int y=0;y<rows;++y)for(int x=0;x<cols;++x) {
        ToneInput p;p.data=0;
        for(int c=0;c<3;++c)p.data.range(c*16+15,c*16)=256+(x*197+y*59+c*503)%20000;
        p.keep=63;p.strb=63;p.user=!x&&!y;p.last=x==cols-1;p.id=0;p.dest=0;input.write(p);
    }
    unsigned mean=0,maximum=0,minimum=0;
    camera_gtm_probe(input,output,rows,cols,768,384,mean,maximum,minimum);
    assert(minimum<=mean && mean<=maximum && maximum>minimum);
    assert(input.empty());std::vector<unsigned> pixels;
    for(int y=0;y<rows;++y)for(int x=0;x<cols;++x) {
        assert(!output.empty());RgbPixel p=output.read();
        assert(p.keep==7 && p.strb==7 && p.user==(!x&&!y) && p.last==(x==cols-1));
        assert(p.id==0 && p.dest==0);pixels.push_back(p.data.to_uint());
    }
    assert(output.empty());return pixels;
}
int main() {
    for(auto shape:std::vector<std::pair<int,int>>{{32,40},{64,96}}) {
        auto a=run(shape.first,shape.second),b=run(shape.first,shape.second);
        assert(a==b);bool varying=false;for(auto p:a)varying|=p!=a[0];assert(varying);
    }
    std::cout<<"PASS native Xilinx GTM resource probe: deterministic positive stimulus and AXI framing; not camera acceptance\n";
}
