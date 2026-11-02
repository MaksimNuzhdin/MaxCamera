#include "adaptive_tone.hpp"

void camera_adaptive_tone(hls::stream<RgbPixel>& input,hls::stream<RgbPixel>& output,
    int rows,int cols,bool enable,unsigned char target,unsigned epoch,
    unsigned& applied_gain_q8,unsigned& previous_mean) {
#pragma HLS INTERFACE axis port=input
#pragma HLS INTERFACE axis port=output
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=enable bundle=control
#pragma HLS INTERFACE s_axilite port=target bundle=control
#pragma HLS INTERFACE s_axilite port=epoch bundle=control
#pragma HLS INTERFACE s_axilite port=applied_gain_q8 bundle=control
#pragma HLS INTERFACE s_axilite port=previous_mean bundle=control
    static bool valid=false;
    static unsigned old_epoch=0,old_target=0,mean=0,gain=256;
    static int old_rows=0,old_cols=0;
#pragma HLS RESET variable=valid
    ap_uint<8> lut[3][256];
#pragma HLS ARRAY_PARTITION variable=lut complete dim=1
#pragma HLS BIND_STORAGE variable=lut type=ram_1p impl=bram
    if(rows<1 || rows>MAX_ROWS || cols<1 || cols>MAX_COLS || target<16 || target>224) {
        applied_gain_q8=256;previous_mean=0;valid=false;return;
    }
    const ap_uint<23> pixels=ap_uint<11>(rows)*ap_uint<12>(cols);
    bool reset=!valid || epoch!=old_epoch || target!=old_target || rows!=old_rows || cols!=old_cols;
    if(reset || !enable) {gain=256;mean=0;}
    if(enable && !reset && mean) {
        unsigned desired=(unsigned(target)*256+mean/2)/mean;
        desired=desired<128?128:desired>512?512:desired;
        // Bound temporal changes, including large scene cuts, to 1/16 gain/frame.
        if(desired>gain)gain+=desired-gain>16?16:desired-gain;
        else gain-=gain-desired>16?16:gain-desired;
    }
    applied_gain_q8=gain;previous_mean=mean;
    if(enable && !reset)for(int x=0;x<256;++x) {
        // Rational shoulder fixes both endpoints and remains monotonic for g>0.
        ap_uint<18> scaled=ap_uint<10>(gain)*ap_uint<8>(x);
        unsigned denominator=65280+unsigned(scaled)-(unsigned(x)<<8);
        unsigned numerator=(unsigned(scaled)<<8)-unsigned(scaled);
        ap_uint<8> value=(numerator+denominator/2)/denominator;
        for(int c=0;c<3;++c) {
#pragma HLS UNROLL
            lut[c][x]=value;
        }
    }
    ap_uint<31> sum=0;
    for(unsigned i=0;i<pixels;++i) {
#pragma HLS PIPELINE II=1
        RgbPixel p=input.read();
        ap_uint<10> luma=ap_uint<10>(p.data.range(7,0))+ap_uint<10>(p.data.range(15,8))*2+ap_uint<10>(p.data.range(23,16));
        sum+=luma>>2;
        if(enable && !reset)for(int c=0;c<3;++c) {
#pragma HLS UNROLL
            ap_uint<8> index=p.data.range(c*8+7,c*8);
            p.data.range(c*8+7,c*8)=lut[c][index];
        }
        output.write(p);
    }
    mean=(unsigned(sum)+pixels/2)/pixels;
    valid=enable;old_epoch=epoch;old_target=target;old_rows=rows;old_cols=cols;
}
