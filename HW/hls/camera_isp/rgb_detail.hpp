#pragma once
#include "camera_isp.hpp"

// RGB8 after gamma, before ROI/downscale. Strength is unsigned Q8 in [0,256].
// This standalone IP is not advertised as a camera capability until integrated.
void camera_rgb_detail(hls::stream<RgbPixel>& input,
                       hls::stream<RgbPixel>& output,
                       int rows, int cols, bool bilateral_enable,
                       unsigned short sigma_color, unsigned short sigma_space_q8,
                       unsigned short sharpen_strength);
