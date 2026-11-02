#pragma once
#include <ap_axi_sdata.h>
#include <hls_stream.h>

// Pixel interfaces, not CSI byte interfaces. One invocation processes one frame.
using RawPixel = ap_axiu<16, 1, 1, 1>;
using RgbPixel = ap_axiu<24, 1, 1, 1>;
constexpr int MAX_ROWS = 1944;
constexpr int MAX_COLS = 2592;
constexpr int AI_ROWS = 640;
constexpr int AI_COLS = 640;

// Integrated RAW path: retain Bayer samples and CSI packing downstream.
void camera_raw_correct(hls::stream<RawPixel>& input,
                        hls::stream<RawPixel>& output,
                        int rows, int cols, bool bpc_enable,
                        unsigned short black_level,
                        unsigned short blue_gain, unsigned short green_gain,
                        unsigned short red_gain);

void camera_raw_isp(hls::stream<RawPixel>& input,
                    hls::stream<RgbPixel>& output,
                    int rows, int cols, bool bpc_enable,
                    unsigned short black_level,
                    unsigned short blue_gain, unsigned short green_gain,
                    unsigned short red_gain);

// RGB byte order is R in bits 7:0, G in 15:8, B in 23:16.
void camera_ai_resize(hls::stream<RgbPixel>& input,
                      hls::stream<RgbPixel>& output,
                      int rows, int cols, int roi_x, int roi_y,
                      int roi_width, int roi_height,
                      int out_rows, int out_cols,
                      unsigned long long& intensity_sum,
                      unsigned int& clipped_pixels,
                      unsigned int& dark_pixels);

// Apply a 3x3 RGB median after demosaic; never apply this to Bayer mosaics.
void camera_rgb_denoise(hls::stream<RgbPixel>& input,
                        hls::stream<RgbPixel>& output,
                        int rows, int cols, bool enable);

// Combined FPGA RGB path; output dimensions may span the full sensor image.
void camera_color_pipeline(hls::stream<RawPixel>& input,
                           hls::stream<RgbPixel>& output,
                           int rows, int cols, bool bpc_enable,
                           unsigned short black_level,
                           unsigned short blue_gain, unsigned short green_gain,
                           unsigned short red_gain, bool median_enable,
                           int roi_x, int roi_y, int roi_width, int roi_height,
                           int out_rows, int out_cols,
                           unsigned long long& intensity_sum,
                           unsigned int& clipped_pixels, unsigned int& dark_pixels);
