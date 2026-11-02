#pragma once
#include "camera_isp.hpp"
enum { OUTPUT_RGB,OUTPUT_YUYV,OUTPUT_UYVY,OUTPUT_YVYU,OUTPUT_VYUY,
       OUTPUT_UYV444,OUTPUT_YUV420 };
// RGB bytes enter R,G,B. Packed output uses contiguous TKEEP=3 or 7.
// YUV422 averages horizontal chroma pairs; legacy YUV420 averages 2x2 chroma.
void camera_rgb_output(hls::stream<RgbPixel>& input,hls::stream<RgbPixel>& output,
                        int rows,int cols,unsigned format,bool full_range);
