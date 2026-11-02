#pragma once
#include "camera_isp.hpp"
#include "common/xf_common.hpp"
#include "imgproc/xf_bilateral_filter.hpp"
#include "imgproc/xf_custom_convolution.hpp"

using DetailImage = xf::cv::Mat<XF_8UC3, MAX_ROWS, MAX_COLS, XF_NPPC1, 2>;

static void unpack(hls::stream<RgbPixel>& input, DetailImage& image) {
    for (int i=0; i<image.rows*image.cols; ++i) {
#pragma HLS PIPELINE II=1
        image.write(i,input.read().data);
    }
}

static void copy_image(DetailImage& input, DetailImage& output) {
    for (int i=0; i<input.rows*input.cols; ++i) {
#pragma HLS PIPELINE II=1
        output.write(i,input.read(i));
    }
}

static void bilateral_rgb(DetailImage& input, DetailImage& output, bool enable,
                    unsigned short color, unsigned short space) {
    if (enable)
        xf::cv::bilateralFilter<3,XF_BORDER_REPLICATE,XF_8UC3,MAX_ROWS,MAX_COLS,
                                XF_NPPC1,2,2>(input,output,float(color),float(space)/256.0f);
    else copy_image(input,output);
}

static void sharpen_rgb(DetailImage& input, DetailImage& output, unsigned short strength) {
    if (strength) {
        short s=strength;
        short kernel[9]={0,short(-s),0,short(-s),short(256+4*s),short(-s),0,short(-s),0};
        xf::cv::filter2D<XF_BORDER_CONSTANT,3,3,XF_8UC3,XF_8UC3,MAX_ROWS,MAX_COLS,
                         XF_NPPC1,2,2>(input,output,kernel,8);
    } else copy_image(input,output);
}

