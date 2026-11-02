#include "integer_remap.hpp"
static ap_uint<24> take_pixel(hls::stream<IntegerRemapPixel>& input,
                              int row,int col,int cols,unsigned& errors) {
#pragma HLS INLINE
    IntegerRemapPixel p=input.read();
    if(p.user!=(!row&&!col) || p.last!=(col==cols-1) || p.keep!=7 || p.strb!=7)
        errors|=REMAP_FRAMING;
    return p.data;
}
void integer_remap(hls::stream<IntegerRemapPixel>& input,
                   hls::stream<IntegerRemapPixel>& output,
                   const ap_uint<32>* maps,int rows,int cols,bool enabled,
                   unsigned& status) {
#pragma HLS INTERFACE axis port=input
#pragma HLS INTERFACE axis port=output
#pragma HLS INTERFACE m_axi port=maps offset=slave bundle=map_memory depth=307200 max_read_burst_length=64 num_read_outstanding=2
#pragma HLS INTERFACE s_axilite port=maps bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=enabled bundle=control
#pragma HLS INTERFACE s_axilite port=status bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
    unsigned errors=0;
    status=0;
    // Disabled correction must not restrict the established camera resolutions.
    if(rows<1 || rows>1944 || cols<1 || cols>2592 ||
       (enabled && (rows>480 || cols>640))) {status=REMAP_GEOMETRY;return;}
    if(!enabled) {
        for(int y=0;y<rows;++y) {
#pragma HLS LOOP_FLATTEN off
          for(int x=0;x<cols;++x) {
#pragma HLS PIPELINE II=1
            output.write(input.read());
          }
        }
        return;
    }
    ap_uint<24> lines[16][640];
#pragma HLS ARRAY_PARTITION variable=lines complete dim=1
#pragma HLS BIND_STORAGE variable=lines type=ram_2p impl=bram
    // Before row y is emitted, rows through y+7 are complete. While emitting,
    // write y+8 into the bank of y-8, outside the permitted read window.
    for(int y=0;y<8 && y<rows;++y) {
#pragma HLS LOOP_FLATTEN off
      for(int x=0;x<cols;++x) {
#pragma HLS PIPELINE II=1
        lines[y][x]=take_pixel(input,y,x,cols,errors);
      }
    }
    ap_uint<19> index=0;
    for(int y=0;y<rows;++y) {
#pragma HLS LOOP_FLATTEN off
      for(int x=0;x<cols;++x) {
#pragma HLS PIPELINE II=1
        if(y+8<rows)lines[(y+8)&15][x]=take_pixel(input,y+8,x,cols,errors);
        ap_uint<32> word=maps[index++];
        unsigned sx=word.range(9,0),sy=word.range(18,10);
        bool valid=word[31];
        bool malformed=word.range(30,19)!=0 || (!valid && word!=0);
        bool supported=sx<(unsigned)cols && sy<(unsigned)rows &&
                       (int)sy>=y-7 && (int)sy<=y+7;
        IntegerRemapPixel p;
        p.data=0;
        if(malformed || (valid&&!supported))errors|=REMAP_MAP;
        else if(valid)p.data=lines[sy&15][sx];
        p.keep=7;p.strb=7;p.user=!y&&!x;p.last=x==cols-1;p.id=0;p.dest=0;
        output.write(p);
      }
    }
    status=errors;
}
