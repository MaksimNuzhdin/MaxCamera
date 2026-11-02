#include "rgb_output.hpp"
#include <cassert>
#include <cmath>
#include <algorithm>
#include <vector>
#include <iostream>
static unsigned byte(double x) {return unsigned(std::max(0.0,std::min(255.0,std::floor(x+.5))));}
static void check(int rows,int cols,unsigned format,bool full) {
    std::vector<unsigned> rgb,y,u,v;
    hls::stream<RgbPixel> input,output;
    for(int yy=0;yy<rows;++yy)for(int xx=0;xx<cols;++xx) {
        unsigned r=(xx*17+yy*31)%256,g=(xx*13+yy*7)%256,b=(xx*3+yy*23)%256;
        if(rows==2 && cols==2) {r=yy?0:255;g=xx?255:0;b=yy?255:0;}
        unsigned data=r|(g<<8)|(b<<16);rgb.push_back(data);
        if(full) {
            y.push_back(byte(.299*r+.587*g+.114*b));
            u.push_back(byte(128-.168736*r-.331264*g+.5*b));
            v.push_back(byte(128+.5*r-.418688*g-.081312*b));
        } else {
            y.push_back(byte(16+.257*r+.504*g+.098*b));
            u.push_back(byte(128-.148*r-.291*g+.439*b));
            v.push_back(byte(128+.439*r-.368*g-.071*b));
        }
        RgbPixel p;p.data=data;p.keep=7;p.strb=7;p.user=0;p.last=0;p.id=0;p.dest=0;input.write(p);
    }
    camera_rgb_output(input,output,rows,cols,format,full);
    unsigned stride=format==OUTPUT_YUV420?2:1;
    for(int row=0;row<rows;++row)for(int col=0;col<cols;col+=stride) {
        assert(!output.empty());auto p=output.read();
        unsigned keep=format>=OUTPUT_YUYV && format<=OUTPUT_VYUY?3:7;
        assert(p.keep==keep && p.strb==keep && p.user==(!row&&!col) && p.last==(col==cols-int(stride)));
        if(format==OUTPUT_RGB) {assert(p.data==rgb[row*cols+col]);continue;}
        unsigned expected[3]={};
        if(format==OUTPUT_UYV444) {expected[0]=u[row*cols+col];expected[1]=y[row*cols+col];expected[2]=v[row*cols+col];}
        else if(format==OUTPUT_YUV420) {
            unsigned top=row&~1U,i=top*cols+col;
            const auto& chroma=(row&1)?v:u;
            expected[0]=(chroma[i]+chroma[i+1]+chroma[i+cols]+chroma[i+cols+1]+2)>>2;
            expected[1]=y[row*cols+col];expected[2]=y[row*cols+col+1];
        } else {
            unsigned i=row*cols+(col&~1U),uu=(u[i]+u[i+1]+1)>>1,vv=(v[i]+v[i+1]+1)>>1;
            bool reverse=format==OUTPUT_YVYU || format==OUTPUT_VYUY;
            unsigned c=(col&1)?(reverse?uu:vv):(reverse?vv:uu);
            bool first=format==OUTPUT_UYVY || format==OUTPUT_VYUY;
            expected[0]=first?c:y[row*cols+col];expected[1]=first?y[row*cols+col]:c;
            assert((p.data.to_uint()>>16)==0);
        }
        for(unsigned channel=0;channel<(keep==3?2U:3U);++channel)
            assert(std::abs(int(p.data.range(channel*8+7,channel*8))-int(expected[channel]))<=2);
    }
    assert(input.empty() && output.empty());
}
int main() {
    for(bool full:{false,true})for(unsigned format=0;format<=OUTPUT_YUV420;++format)check(16,32,format,full);
    check(1,1,OUTPUT_RGB,false);check(3,7,OUTPUT_UYV444,true);
    check(2,2,OUTPUT_YUV420,false);check(2,2,OUTPUT_YUV420,true);
    check(8,MAX_COLS,OUTPUT_YUV420,true);check(MAX_ROWS,16,OUTPUT_YUV420,false);
    hls::stream<RgbPixel> input,output;
    camera_rgb_output(input,output,2,3,OUTPUT_YUYV,false);
    camera_rgb_output(input,output,1,2,OUTPUT_YUV420,false);
    camera_rgb_output(input,output,0,2,OUTPUT_RGB,false);
    assert(input.empty() && output.empty());
    std::cout<<"PASS RGB output: lossless bypass, BT601 limited/full range, four 422 orders, 444/420 layout, averaged chroma, framing/keep, maximum dimensions, guards; analytic reference within 2 DN\n";
}
