#include "color_adjust_internal.hpp"
static void color_adjust_frame(hls::stream<ColorPixel>& input,
                               hls::stream<RgbPixel>& output,
                               int rows, int cols, bool ccm_enable, bool gamma_enable,
                               int matrix[3][3], int offsets[3], unsigned char gamma[768],
                               unsigned long long& red_sum, unsigned long long& green_sum,
                               unsigned long long& blue_sum, unsigned int& valid_pixels) {
#pragma HLS INLINE off
#pragma HLS DATAFLOW
    ColorImage raw(rows, cols), corrected(rows, cols);
    ByteImage quantized(rows, cols), mapped(rows, cols);
    read_color(input, raw, red_sum, green_sum, blue_sum, valid_pixels);
    correct_color(raw, corrected, ccm_enable, matrix, offsets);
    quantize_color(corrected, quantized);
    apply_gamma(quantized, mapped, gamma_enable, gamma);
    write_color(mapped, output);
}

void camera_color_adjust(hls::stream<ColorPixel>& input,
                         hls::stream<RgbPixel>& output,
                         int rows, int cols, bool ccm_enable, bool gamma_enable,
                         int matrix[3][3], int offsets[3], unsigned char gamma[768],
                         unsigned long long& red_sum, unsigned long long& green_sum,
                         unsigned long long& blue_sum, unsigned int& valid_pixels) {
#pragma HLS INTERFACE axis port=input
#pragma HLS INTERFACE axis port=output
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=ccm_enable bundle=control
#pragma HLS INTERFACE s_axilite port=gamma_enable bundle=control
#pragma HLS INTERFACE s_axilite port=matrix bundle=control
#pragma HLS INTERFACE s_axilite port=offsets bundle=control
#pragma HLS INTERFACE s_axilite port=gamma bundle=control
#pragma HLS INTERFACE s_axilite port=red_sum bundle=control
#pragma HLS INTERFACE s_axilite port=green_sum bundle=control
#pragma HLS INTERFACE s_axilite port=blue_sum bundle=control
#pragma HLS INTERFACE s_axilite port=valid_pixels bundle=control
    bool valid = rows >= 1 && rows <= MAX_ROWS && cols >= 1 && cols <= MAX_COLS;
    if (ccm_enable) {
        for (int i = 0; i < 3; ++i) {
            valid &= offsets[i] >= -8388608 && offsets[i] <= 8388607;
            for (int j = 0; j < 3; ++j)
                valid &= matrix[i][j] >= -8388608 && matrix[i][j] <= 8388607;
        }
    }
    if (!valid) {red_sum = green_sum = blue_sum = 0; valid_pixels = 0; return;}
    // Valid frames update statistics once, after consumption, never at restart.
    color_adjust_frame(input, output, rows, cols, ccm_enable, gamma_enable,
                       matrix, offsets, gamma, red_sum, green_sum, blue_sum, valid_pixels);
}
