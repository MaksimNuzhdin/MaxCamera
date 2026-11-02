#pragma once
#include "camera_isp.hpp"
#include "common/xf_common.hpp"
using LscImage=xf::cv::Mat<XF_10UC1,MAX_ROWS,MAX_COLS,XF_NPPC1,2>;

static void lsc_mat_frame(LscImage& input,LscImage& output,bool enable,
                           unsigned short gains[4][7][9]) {
#pragma HLS INLINE off
    int rows=input.rows,cols=input.cols;
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
            ap_uint<10> value=input.read(y*cols+x);
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
            output.write(y*cols+x,value);
        }
    }
}
