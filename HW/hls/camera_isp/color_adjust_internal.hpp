#pragma once
#include "color_adjust.hpp"
#include "common/xf_common.hpp"
#include "imgproc/xf_colorcorrectionmatrix.hpp"
#include "imgproc/xf_gammacorrection.hpp"
using ColorImage = xf::cv::Mat<XF_10UC3, MAX_ROWS, MAX_COLS, XF_NPPC1, 2>;
using ByteImage = xf::cv::Mat<XF_8UC3, MAX_ROWS, MAX_COLS, XF_NPPC1, 2>;

static void read_color(hls::stream<ColorPixel>& input, ColorImage& image,
                       unsigned long long& red_sum, unsigned long long& green_sum,
                       unsigned long long& blue_sum, unsigned int& valid_pixels) {
    ap_uint<40> sums[3] = {0, 0, 0};
#pragma HLS ARRAY_PARTITION variable=sums complete
    ap_uint<23> count = 0;
    for (int i = 0; i < image.rows * image.cols; ++i) {
#pragma HLS PIPELINE II=1
        ap_uint<30> p = input.read().data.range(29, 0);
        ap_uint<10> r = p.range(9, 0), g = p.range(19, 10), b = p.range(29, 20);
        bool valid = r >= 8 && g >= 8 && b >= 8 && r < 1000 && g < 1000 && b < 1000;
        if (valid) {sums[0] += r; sums[1] += g; sums[2] += b; ++count;}
        image.write(i, p);
    }
    red_sum = sums[0]; green_sum = sums[1]; blue_sum = sums[2]; valid_pixels = count;
}

static void correct_color(ColorImage& input, ColorImage& output,
                          bool enable, int matrix[3][3], int offsets[3]) {
    if (enable) {
        xf::cv::colorcorrectionmatrix<XF_10UC3, XF_10UC3, MAX_ROWS, MAX_COLS,
                                     XF_NPPC1, 2, 2>(input, output, matrix, offsets);
    } else {
        for (int i = 0; i < input.rows * input.cols; ++i) {
#pragma HLS PIPELINE II=1
            output.write(i, input.read(i));
        }
    }
}

static void quantize_color(ColorImage& input, ByteImage& output) {
    for (int i = 0; i < input.rows * input.cols; ++i) {
#pragma HLS PIPELINE II=1
        ap_uint<30> p = input.read(i);
        ap_uint<24> result;
        for (int c = 0; c < 3; ++c) {
#pragma HLS UNROLL
            ap_uint<11> rounded = (ap_uint<11>(p.range(c * 10 + 9, c * 10)) + 2) >> 2;
            result.range(c * 8 + 7, c * 8) = rounded > 255 ? 255 : rounded.to_uint();
        }
        output.write(i, result);
    }
}

static void apply_gamma(ByteImage& input, ByteImage& output,
                        bool enable, unsigned char gamma[768]) {
    if (enable) {
        xf::cv::gammacorrection<XF_8UC3, XF_8UC3, MAX_ROWS, MAX_COLS,
                              XF_NPPC1, 2, 2>(input, output, gamma);
    } else {
        for (int i = 0; i < input.rows * input.cols; ++i) {
#pragma HLS PIPELINE II=1
            output.write(i, input.read(i));
        }
    }
}

static void write_color(ByteImage& image, hls::stream<RgbPixel>& output) {
    for (int y = 0; y < image.rows; ++y) for (int x = 0; x < image.cols; ++x) {
#pragma HLS PIPELINE II=1
        RgbPixel p;
        p.data = image.read(y * image.cols + x); p.keep = 7; p.strb = 7;
        p.user = !y && !x; p.last = x == image.cols - 1; p.id = 0; p.dest = 0;
        output.write(p);
    }
}

