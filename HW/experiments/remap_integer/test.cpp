#include "integer_remap.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
static unsigned pixel(int x,int y,int salt) {return ((x*17+y*7+salt)&255)|(((x+y*3+salt)&255)<<8)|(((y+x*5+salt)&255)<<16);}
static void run(int rows,int cols,int dx,int dy,int salt,int fault=0,bool enable=true) {
    hls::stream<IntegerRemapPixel> in,out;
#ifdef REMAP_RTL_TEST
    // HLS wrap-C serializes the declared m_axi depth even for a small frame.
    // Supply that complete host backing store; native tests retain nullptr bypass.
    std::vector<ap_uint<32>> map(307200);
#else
    std::vector<ap_uint<32>> map(rows*cols);
#endif
    for(int y=0;y<rows;++y)for(int x=0;x<cols;++x) {
        IntegerRemapPixel p;p.data=pixel(x,y,salt);p.keep=p.strb=7;
        p.user=!y&&!x;p.last=x==cols-1;p.id=enable?0:1;p.dest=enable?0:1;
        if(fault==3 && !y&&!x)p.user=0;
        in.write(p);
        int sx=x+dx,sy=y+dy;
        map[y*cols+x]=(sx>=0&&sx<cols&&sy>=0&&sy<rows)?(0x80000000U|unsigned(sx)|(unsigned(sy)<<10)):0;
    }
    if(fault==1)map[0]=1; // invalid encoding, not an optical border
    if(fault==2)map[0]=0x80000000U|(8U<<10); // out of permitted row window
    unsigned status=999;
#ifdef REMAP_RTL_TEST
    integer_remap(in,out,map.data(),rows,cols,enable,status);
#else
    integer_remap(in,out,enable?map.data():nullptr,rows,cols,enable,status);
#endif
    assert(status==(fault==3?REMAP_FRAMING:fault?REMAP_MAP:0));
    assert(in.empty());
    for(int y=0;y<rows;++y)for(int x=0;x<cols;++x) {
        unsigned expected=0;int sx=x+dx,sy=y+dy;
        if(!enable)expected=pixel(x,y,salt);
        else if(sx>=0&&sx<cols&&sy>=0&&sy<rows)expected=pixel(sx,sy,salt);
        if((fault==1||fault==2)&&!x&&!y)expected=0;
        auto p=out.read();assert(p.data==expected);
        assert(p.user==(!y&&!x)&&p.last==(x==cols-1)&&p.keep==7&&p.strb==7);
        assert(p.id==!enable&&p.dest==!enable);
    }
    assert(out.empty());
}
int main() {
#ifdef REMAP_RTL_TEST
    // Short transaction-level RTL suite. Maximum geometry stays in full Csim.
    run(24,128,0,0,7);run(24,128,5,7,19);run(24,128,-5,-7,29);
    run(17,19,0,0,3,1);run(17,19,0,0,5,2);run(17,19,0,0,9,3);
    run(17,19,0,0,11,0,false);run(1,1,0,0,13);
    run(1,2592,0,0,17,0,false);run(1944,1,0,0,19,0,false);
    std::puts("PASS bounded integer remap RTL transaction suite: pixel oracle, maps, borders, errors, bypass and repeated invocations");
#else
    run(480,640,0,0,7);run(480,640,5,7,19);run(480,640,-5,-7,29);
    for(int rows: {1,2,7,8,9,17,31})for(int cols: {1,3,19})
        for(int dy=-7;dy<=7;++dy)run(rows,cols,0,dy,rows+dy);
    run(17,19,0,0,3,1);run(17,19,0,0,5,2);run(17,19,0,0,9,3);
    run(17,19,0,0,11,0,false);run(480,640,0,0,13,0,false);
    run(1,2592,0,0,17,0,false);run(1944,1,0,0,19,0,false);
    hls::stream<IntegerRemapPixel> in,out;unsigned status;
    IntegerRemapPixel p;p.data=123;in.write(p);
    integer_remap(in,out,nullptr,481,640,true,status);
    assert(status==REMAP_GEOMETRY&&in.size()==1&&out.empty());assert(in.read().data==123);
    std::puts("PASS integer remap exact raster oracle, maximum dimensions, +/-7 rows, edge geometries, malformed maps, framing, no-map bypass and invalid geometry; synthetic data only");
#endif
}
