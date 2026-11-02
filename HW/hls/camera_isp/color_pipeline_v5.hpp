#pragma once
#include "camera_isp.hpp"
void camera_color_pipeline_v5(hls::stream<RawPixel>& input, hls::stream<RgbPixel>& output,
    int rows, int cols, bool bpc_enable, unsigned short black_level,
    unsigned short blue_gain, unsigned short green_gain, unsigned short red_gain,
    bool median_enable, int roi_x, int roi_y, int roi_width, int roi_height,
    int out_rows, int out_cols, volatile unsigned long long& intensity_sum,
    volatile unsigned int& clipped_pixels, volatile unsigned int& dark_pixels,
    bool ccm_enable, bool gamma_enable, bool awb_enable,
    int matrix[3][3], int offsets[3], unsigned char gamma[768],
    volatile unsigned long long& red_sum, volatile unsigned long long& green_sum,
    volatile unsigned long long& blue_sum, volatile unsigned int& valid_pixels,
    volatile unsigned int& applied_red, volatile unsigned int& applied_green, volatile unsigned int& applied_blue,
    volatile unsigned int& frame_sequence,
    bool bilateral_enable,unsigned short sigma_color,unsigned short sigma_space_q8,unsigned short sharpen_strength,
    bool lsc_enable,unsigned short lsc_gains[4][7][9]);
