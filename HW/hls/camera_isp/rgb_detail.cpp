#include "rgb_detail.hpp"
#include "rgb_detail_internal.hpp"

static void pack(DetailImage& image, hls::stream<RgbPixel>& output) {
    for (int y=0; y<image.rows; ++y) for (int x=0; x<image.cols; ++x) {
#pragma HLS PIPELINE II=1
        RgbPixel p;
        p.data=image.read(y*image.cols+x);p.keep=7;p.strb=7;
        p.user=!x&&!y;p.last=x==image.cols-1;p.id=0;p.dest=0;output.write(p);
    }
}

static void detail_frame(hls::stream<RgbPixel>& input, hls::stream<RgbPixel>& output,
                         int rows,int cols,bool bilateral_enable,
                         unsigned short color,unsigned short space,unsigned short strength) {
#pragma HLS INLINE off
#pragma HLS DATAFLOW
    DetailImage source(rows,cols),filtered(rows,cols),enhanced(rows,cols);
    unpack(input,source);
    bilateral_rgb(source,filtered,bilateral_enable,color,space);
    sharpen_rgb(filtered,enhanced,strength);
    pack(enhanced,output);
}

void camera_rgb_detail(hls::stream<RgbPixel>& input, hls::stream<RgbPixel>& output,
                       int rows,int cols,bool bilateral_enable,
                       unsigned short sigma_color,unsigned short sigma_space_q8,
                       unsigned short sharpen_strength) {
#pragma HLS INTERFACE axis port=input
#pragma HLS INTERFACE axis port=output
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=bilateral_enable bundle=control
#pragma HLS INTERFACE s_axilite port=sigma_color bundle=control
#pragma HLS INTERFACE s_axilite port=sigma_space_q8 bundle=control
#pragma HLS INTERFACE s_axilite port=sharpen_strength bundle=control
    if (rows<8 || rows>MAX_ROWS || cols<8 || cols>MAX_COLS || sharpen_strength>256 ||
        (bilateral_enable && (!sigma_color || sigma_color>255 ||
                              sigma_space_q8<64 || sigma_space_q8>1024))) return;
    detail_frame(input,output,rows,cols,bilateral_enable,sigma_color,sigma_space_q8,sharpen_strength);
}
