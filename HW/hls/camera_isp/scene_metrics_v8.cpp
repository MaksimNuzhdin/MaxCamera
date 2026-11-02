#include "scene_metrics_v8.hpp"

void camera_scene_metrics_v8(hls::stream<RgbPixel>& input,hls::stream<RgbPixel>& output,
                          int rows,int cols,bool enable,
                          volatile unsigned int& focus_q16,
                          volatile unsigned int& motion_q16,
                          volatile unsigned int& motion_valid,
                          volatile unsigned int& frame_sequence,
                          volatile unsigned int& intensity_sum,
                          volatile unsigned int& clipped_pixels,
                          volatile unsigned int& dark_pixels) {
#pragma HLS INTERFACE axis port=input
#pragma HLS INTERFACE axis port=output
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=enable bundle=control
#pragma HLS INTERFACE s_axilite port=focus_q16 bundle=control
#pragma HLS INTERFACE s_axilite port=motion_q16 bundle=control
#pragma HLS INTERFACE s_axilite port=motion_valid bundle=control
#pragma HLS INTERFACE s_axilite port=frame_sequence bundle=control
#pragma HLS INTERFACE s_axilite port=intensity_sum bundle=control
#pragma HLS INTERFACE s_axilite port=clipped_pixels bundle=control
#pragma HLS INTERFACE s_axilite port=dark_pixels bundle=control
    static ap_uint<8> previous[1024];
    static bool valid=false;
    static unsigned last_rows=0,last_cols=0,sequence=0;
#pragma HLS BIND_STORAGE variable=previous type=ram_2p impl=bram
#pragma HLS RESET variable=valid
#pragma HLS RESET variable=last_rows
#pragma HLS RESET variable=last_cols
#pragma HLS RESET variable=sequence
    if(rows<1 || rows>MAX_ROWS || cols<1 || cols>MAX_COLS) {
        focus_q16=0;motion_q16=0;motion_valid=0;intensity_sum=0;clipped_pixels=0;dark_pixels=0;frame_sequence=0;return;
    }
    bool active=enable && rows>=32 && cols>=32;
    bool compare=valid && active && last_rows==unsigned(rows) && last_cols==unsigned(cols);
    ap_uint<8> north[MAX_COLS],far[MAX_COLS];
#pragma HLS ARRAY_PARTITION variable=north cyclic factor=2
#pragma HLS BIND_STORAGE variable=north type=ram_2p impl=bram
#pragma HLS BIND_STORAGE variable=far type=ram_2p impl=bram
    ap_uint<21> sums[1024];ap_uint<13> counts[1024];
#pragma HLS BIND_STORAGE variable=sums type=ram_2p impl=bram
#pragma HLS BIND_STORAGE variable=counts type=ram_2p impl=bram
    ap_uint<5> column_bin[MAX_COLS];
    if(active)for(int i=0;i<1024;++i) {
#pragma HLS PIPELINE II=1
        sums[i]=0;counts[i]=0;
    }
    if(active)for(int x=0;x<cols;++x) {
#pragma HLS PIPELINE II=1
        north[x]=0;far[x]=0;column_bin[x]=ap_uint<17>(unsigned(x)*32U)/ap_uint<12>(cols);
    }
    ap_uint<40> focus=0;ap_uint<32> output_sum=0;ap_uint<23> high=0,low=0;
    for(int y=0;y<rows;++y) {
        unsigned by=active?(ap_uint<16>(unsigned(y)*32U)/ap_uint<11>(rows)).to_uint():0,bin=by*32;
        ap_uint<15> block_sum=0;ap_uint<7> block_count=0;ap_uint<8> west=0;
        for(int x=0;x<cols;++x) {
#pragma HLS PIPELINE II=1
            // Each bin is flushed once per row, in strictly increasing order.
#pragma HLS DEPENDENCE variable=sums inter false
#pragma HLS DEPENDENCE variable=counts inter false
            RgbPixel p=input.read();
            unsigned r=p.data.range(7,0),g=p.data.range(15,8),b=p.data.range(23,16);
            ap_uint<8> luma=(77*r+150*g+29*b+128)>>8;
            output_sum+=luma;high+=(r>=250 || g>=250 || b>=250);low+=(luma<=5);
            if(active) {
            ap_uint<8> center=north[x],up=far[x],east=x+1<cols?north[x+1]:ap_uint<8>(0);
            if(y>=2 && x>=1 && x+1<cols) {
                int lap=4*int(center)-int(west)-int(east)-int(up)-int(luma);
                focus+=lap<0?-lap:lap;
            }
            west=center;far[x]=center;north[x]=luma;
            unsigned target=by*32+column_bin[x].to_uint();
            if(target!=bin) {
                if(active){sums[bin]+=block_sum;counts[bin]+=block_count;}
                bin=target;block_sum=0;block_count=0;
            }
            block_sum+=luma.to_uint();++block_count;
            }
            p.keep=7;p.strb=7;p.user=!x&&!y;p.last=x==cols-1;p.id=0;p.dest=0;output.write(p);
        }
        if(active){sums[bin]+=block_sum;counts[bin]+=block_count;}
    }
    unsigned motion=0;
    if(active)for(int i=0;i<1024;++i) {
        ap_uint<22> rounded=sums[i]+(counts[i]>>1);
        unsigned mean=(rounded/counts[i]).to_uint();
        if(compare) {int delta=int(mean)-int(previous[i]);motion+=delta<0?-delta:delta;}
        previous[i]=mean;
    }
    valid=active;last_rows=rows;last_cols=cols;
    frame_sequence=(++sequence<<1)|1U;
    focus_q16=active?(focus.to_uint64()<<16)/unsigned((rows-2)*(cols-2)):0;
    motion_q16=motion*64;motion_valid=compare;
    intensity_sum=output_sum;clipped_pixels=high;dark_pixels=low;frame_sequence=sequence<<1;
}
