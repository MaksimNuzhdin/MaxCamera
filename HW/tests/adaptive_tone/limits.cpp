#include "adaptive_tone.hpp"
#include <cassert>
#include <cstdio>
static void constant(int rows,int cols,unsigned byte,unsigned epoch,unsigned gain,unsigned mean) {
 hls::stream<RgbPixel> in,out;
 for(int y=0;y<rows;++y)for(int x=0;x<cols;++x){RgbPixel p;p.data=byte*0x010101;p.keep=7;p.strb=7;p.user=!x&&!y;p.last=x==cols-1;p.id=0;p.dest=0;in.write(p);}
 unsigned g=0,m=0;camera_adaptive_tone(in,out,rows,cols,true,96,epoch,g,m);
 assert(in.empty() && g==gain && m==mean);
 for(int y=0;y<rows;++y)for(int x=0;x<cols;++x){assert(!out.empty());auto p=out.read();assert(p.data==byte*0x010101&&p.user==(!x&&!y)&&p.last==(x==cols-1));}
 assert(out.empty());
}
int main(){
 constant(16,16,255,91,256,0);
 for(unsigned i=1;i<=16;++i)constant(16,16,255,91,256-16*(i>8?8:i),255);
 constant(16,16,0,91,128,255);constant(16,16,0,91,128,0);
 hls::stream<RgbPixel> in,out;unsigned g=0,m=0;
 for(unsigned target: {0U,15U,225U,255U}){camera_adaptive_tone(in,out,16,16,true,target,91,g,m);assert(g==256&&m==0&&in.empty()&&out.empty());}
 constant(MAX_ROWS,MAX_COLS,255,92,256,0);
 constant(MAX_ROWS,MAX_COLS,0,92,240,255);
 std::puts("PASS adaptive native boundaries: gain lower clamp, zero-mean hold, invalid targets, maximum 2592x1944 accumulator and framing");
}
