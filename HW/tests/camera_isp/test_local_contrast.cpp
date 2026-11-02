#include "local_contrast.hpp"
#include <cassert>
#include <iostream>
#include "clahe_bounded.hpp"
#include "imgproc/xf_clahe.hpp"
#include <vector>
static void constant(unsigned r,unsigned g,unsigned b,bool enable,unsigned clip,unsigned epoch,
                     unsigned er,unsigned eg,unsigned eb) {
    hls::stream<RgbPixel> input,output;
    for(int y=0;y<64;++y)for(int x=0;x<64;++x) {
        RgbPixel p;p.data=r|(g<<8)|(b<<16);p.keep=7;p.strb=7;
        p.user=!x&&!y;p.last=x==63;p.id=0;p.dest=0;input.write(p);
    }
    camera_local_contrast(input,output,64,64,enable,clip,epoch);
    assert(input.empty());
    for(int y=0;y<64;++y)for(int x=0;x<64;++x) {
        assert(!output.empty());auto p=output.read();
        assert(p.data==(er|(eg<<8)|(eb<<16)));
        assert(p.keep==7 && p.strb==7 && p.user==(!x&&!y) && p.last==(x==63));
    }
    assert(output.empty());
}

static void normalization() {
    for(unsigned d: {1U,16U,64U,256U,19200U,314928U})
        for(unsigned q=0;q<301;++q)for(unsigned r: {0U,d/2,d-1}) {
            unsigned n=q*d+r,expected=n/d;
            assert(xf::cv::bounded_clahe::divide_saturated(n,d)==(expected>255?255:expected));
        }
}
static void differential() {
    using Original=xf::cv::clahe::CLAHEImpl<XF_8UC1,MAX_ROWS,MAX_COLS,XF_NPPC1,8,4,4,4,4,2,2>;
    using Bounded=xf::cv::bounded_clahe::CLAHEImpl<XF_8UC1,MAX_ROWS,MAX_COLS,XF_NPPC1,8,4,4,4,4,2,2>;
    using Gray=xf::cv::Mat<XF_8UC1,MAX_ROWS,MAX_COLS,XF_NPPC1,2>;
    static ap_uint<Original::HIST_COUNTER_BITS> ow[4][4][2][256],orr[4][4][2][256],bw[4][4][2][256],br[4][4][2][256];
    static ap_uint<Original::CLIP_COUNTER_BITS> oc[4][4],bc[4][4];
    for(int y=0;y<4;++y)for(int x=0;x<4;++x)for(int c=0;c<2;++c)for(int v=0;v<256;++v)orr[y][x][c][v]=br[y][x][c][v]=v;
    for(unsigned f=0;f<3;++f) {
        Gray original(64,64),bounded(64,64),expected(64,64),actual(64,64);
        hls::stream<RgbPixel> input,output;std::vector<unsigned> rgb;
        for(int y=0;y<64;++y)for(int x=0;x<64;++x) {
            unsigned r=(x*7+y*3+f*11)%256,g=(x*2+y*5+f*31)%256,b=(x^y^(f*57))%256;
            unsigned v=std::max(r,std::max(g,b)),p=r|(g<<8)|(b<<16);rgb.push_back(p);
            original.write(y*64+x,v);bounded.write(y*64+x,v);
            RgbPixel pixel;pixel.data=p;pixel.keep=7;pixel.strb=7;pixel.user=!x&&!y;pixel.last=x==63;
            pixel.id=0;pixel.dest=0;input.write(pixel);
        }
        Original gold;Bounded optimized;
        gold.process(expected,original,ow,orr,oc,64,64,4,4,4);
        optimized.process(actual,bounded,bw,br,bc,64,64,4,4,4);
        camera_local_contrast(input,output,64,64,true,4,77);
        for(int i=0;i<4096;++i) {
            unsigned mapped=expected.read(i);assert(actual.read(i)==mapped);
            unsigned p=rgb[i],v=std::max(p&255,std::max((p>>8)&255,(p>>16)&255));
            auto pixel=output.read();
            for(int c=0;c<3;++c) {
                unsigned channel=(p>>(c*8))&255;
                unsigned e=!f?channel:v?(channel*mapped+v/2)/v:0;
                int error=int(pixel.data.range(c*8+7,c*8))-int(e);
                assert(error>=-1 && error<=1);
            }
            if(!f)assert(pixel.data==p);
        }
        for(int y=0;y<4;++y)for(int x=0;x<4;++x)for(int c=0;c<2;++c)for(int v=0;v<256;++v) {
            assert(ow[y][x][c][v]==bw[y][x][c][v]);orr[y][x][c][v]=ow[y][x][c][v];br[y][x][c][v]=bw[y][x][c][v];
        }
        assert(input.empty() && output.empty());
    }
}

int main() {
    normalization();differential();
    constant(100,50,25,false,0,1,100,50,25);
    constant(100,50,25,true,4,1,100,50,25);
    // A 16x16 constant tile clips its sole histogram bin to four. Redistributing
    // 252 counts gives CDF(100)=105 and floor(105*255/256)=104.
    constant(100,50,25,true,4,1,104,52,26);
    constant(100,50,25,true,4,1,104,52,26);
    constant(100,50,25,true,4,2,100,50,25);
    constant(100,50,25,true,8,2,100,50,25);
    constant(100,50,25,true,8,2,108,54,27);
    constant(0,0,0,true,8,3,0,0,0);
    constant(0,0,0,true,8,3,0,0,0);
    constant(255,255,255,true,4,4,255,255,255);
    constant(255,255,255,true,4,4,255,255,255);
    hls::stream<RgbPixel> input,output;
    camera_local_contrast(input,output,31,64,true,4,4);
    camera_local_contrast(input,output,64,63,true,4,4);
    camera_local_contrast(input,output,64,64,true,9,4);
    assert(input.empty() && output.empty());
    std::cout<<"PASS Xilinx local contrast: analytical CDF, RGB ratio, first frame, epoch/clip reset, black/white, bypass, framing and bounds\n";
}
