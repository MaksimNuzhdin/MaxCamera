#include "raw_lsc.hpp"

static void lsc_frame(hls::stream<RawPixel>& input,hls::stream<RawPixel>& output,
                      int rows,int cols,bool enable,unsigned short gains[4][7][9]) {
    ap_uint<15> table[4][7][9];
#pragma HLS ARRAY_PARTITION variable=table complete dim=1
#pragma HLS ARRAY_PARTITION variable=table cyclic factor=2 dim=2
#pragma HLS ARRAY_PARTITION variable=table cyclic factor=2 dim=3
    ap_uint<20> xpos[MAX_COLS];
#pragma HLS BIND_STORAGE variable=xpos type=ram_1p impl=bram
    if(enable) {
        for(int p=0;p<4;++p)for(int y=0;y<7;++y)for(int x=0;x<9;++x) {
#pragma HLS PIPELINE II=1
            unsigned g=gains[p][y][x];table[p][y][x]=g>16384?16384:g;
        }
        for(int x=0;x<cols;++x)xpos[x]=(unsigned(x)*8U*65536U)/unsigned(cols-1);
    }
    for(int y=0;y<rows;++y) {
        unsigned cy=enable?(unsigned(y)*6U*65536U)/unsigned(rows-1):0U;
        unsigned iy=cy>>16,fy=cy&65535;
        if(iy==6){iy=5;fy=65536;}
        for(int x=0;x<cols;++x) {
#pragma HLS PIPELINE II=1
            RawPixel p=input.read();ap_uint<10> value=p.data.range(9,0);
            if(enable) {
                unsigned cx=xpos[x],ix=cx>>16,fx=cx&65535;
                if(ix==8){ix=7;fx=65536;}
                unsigned phase=(y&1)*2+(x&1);
                ap_uint<33> top=table[phase][iy][ix]*(65536-fx)+table[phase][iy][ix+1]*fx;
                ap_uint<33> bottom=table[phase][iy+1][ix]*(65536-fx)+table[phase][iy+1][ix+1]*fx;
                ap_uint<51> mix=top*(65536-fy)+bottom*fy;
                ap_uint<15> gain=(mix+ap_uint<51>(2147483648ULL))>>32;
                ap_uint<26> scaled=value*gain+2048;
                ap_uint<14> corrected=scaled>>12;
                value=corrected>1023?1023:corrected.to_uint();
            }
            p.data=value;p.keep=3;p.strb=3;p.user=!x&&!y;p.last=x==cols-1;p.id=0;p.dest=0;
            output.write(p);
        }
    }
}

void camera_raw_lsc(hls::stream<RawPixel>& input,hls::stream<RawPixel>& output,
                    int rows,int cols,bool enable,unsigned short gains[4][7][9]) {
#pragma HLS INTERFACE axis port=input
#pragma HLS INTERFACE axis port=output
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=enable bundle=control
#pragma HLS INTERFACE s_axilite port=gains bundle=control
    if(rows<2 || rows>MAX_ROWS || cols<2 || cols>MAX_COLS)return;
    lsc_frame(input,output,rows,cols,enable,gains);
}
