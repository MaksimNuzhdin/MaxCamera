#pragma once
#include "camera_isp.hpp"
// Adaptive RGB8 display mapping, not HDR radiance reconstruction. The previous
// frame's input luma drives a shared per-channel curve. First/reset frame bypasses.
void camera_adaptive_tone(hls::stream<RgbPixel>& input,hls::stream<RgbPixel>& output,
    int rows,int cols,bool enable,unsigned char target,unsigned epoch,
    unsigned& applied_gain_q8,unsigned& previous_mean);
