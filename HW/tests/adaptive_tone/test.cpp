#include "adaptive_tone.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
using Frame=std::vector<unsigned>;
static unsigned calls=0;
static Frame run(const Frame& src,int rows,int cols,bool enable,unsigned target,unsigned epoch,unsigned expected_gain,unsigned expected_previous) {
 hls::stream<RgbPixel> in,out;
 for(unsigned i=0;i<src.size();++i){RgbPixel p;p.data=src[i];p.keep=7;p.strb=7;p.user=i==0;p.last=i%cols==unsigned(cols-1);p.id=i%2;p.dest=(i/2)%2;in.write(p);}
 unsigned gain=0,mean=0;camera_adaptive_tone(in,out,rows,cols,enable,target,epoch,gain,mean);
 assert(gain==expected_gain && mean==expected_previous && in.empty());Frame result;
 for(unsigned i=0;i<src.size();++i){assert(!out.empty());auto p=out.read();assert(p.keep==7&&p.strb==7&&p.user==(i==0)&&p.last==(i%cols==unsigned(cols-1))&&p.id==i%2&&p.dest==(i/2)%2);result.push_back(unsigned(p.data));
  for(unsigned c=0;c<3;++c){unsigned x=(src[i]>>(8*c))&255,y=(unsigned(p.data)>>(8*c))&255;double g=double(expected_gain)/256;unsigned oracle=unsigned(std::floor((g*x)/(1+(g-1)*x/255)+0.5));assert(y==oracle);}}
 assert(out.empty());++calls;return result;
}
int main(){
 Frame ramp;for(unsigned x=0;x<256;++x)ramp.push_back(x*0x010101);
 assert(run(ramp,16,16,false,96,1,256,0)==ramp);
 Frame dark(256,32*0x010101),white(256,0xffffff),black(256,0);
 assert(run(dark,16,16,true,96,1,256,0)==dark);
 auto lifted=run(ramp,16,16,true,96,1,272,32);assert(lifted.front()==0&&lifted.back()==0xffffff);
 for(unsigned i=1;i<lifted.size();++i)assert(lifted[i]>=lifted[i-1]);
 run(white,16,16,true,96,1,256,128);
 run(ramp,16,16,true,96,1,240,255);
 assert(run(black,16,16,true,96,2,256,0)==black);
 assert(run(black,16,16,true,96,2,256,0)==black);
 assert(run(dark,16,16,true,128,2,256,0)==dark);
 assert(run(dark,8,32,true,128,2,256,0)==dark);
 for(unsigned i=1;i<=20;++i)run(dark,8,32,true,128,2,256+(i>16?16:i)*16,32);
 assert(run(dark,8,32,false,128,2,256,0)==dark);
 assert(run(dark,8,32,true,128,2,256,0)==dark);
 Frame color;for(unsigned x=0;x<256;++x)color.push_back(x|((255-x)<<8)|((x/2)<<16));
 run(color,8,32,true,128,2,272,32);
 hls::stream<RgbPixel> in,out;unsigned g=123,m=456;camera_adaptive_tone(in,out,0,32,true,96,2,g,m);assert(in.empty()&&out.empty()&&g==256&&m==0);
 std::printf("PASS adaptive RGB tone: %u frames, independent floating oracle, endpoints/monotonicity, bounded gain, bypass/history/geometry/target reset, metadata and zero-input handling\n",calls);
}
