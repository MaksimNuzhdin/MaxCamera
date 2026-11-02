#pragma once
#include "camera_isp.hpp"
// RGB10 order: R bits 9:0, G bits 19:10, B bits 29:20.
using ColorPixel = ap_axiu<32, 1, 1, 1>;
// Coefficients and offsets use signed Q20, constrained to [-8, 8).
// Gray-world statistics precede CCM and exclude dark/saturated samples.
void camera_color_adjust(hls::stream<ColorPixel>& input,
                         hls::stream<RgbPixel>& output,
                         int rows, int cols, bool ccm_enable, bool gamma_enable,
                         int matrix[3][3], int offsets[3],
                         unsigned char gamma[768],
                         unsigned long long& red_sum,
                         unsigned long long& green_sum,
                         unsigned long long& blue_sum,
                         unsigned int& valid_pixels);
