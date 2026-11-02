#pragma once
#include "camera_isp.hpp"

// Standalone feasibility IP; no camera capability is advertised yet.
// Four-by-four Xilinx CLAHE tiles operate on RGB value (maximum channel).
// A preceding-frame histogram preserves streaming operation. Epoch, dimensions,
// clip changes and disabled operation invalidate history; the next frame bypasses.
void camera_local_contrast(hls::stream<RgbPixel>& input,hls::stream<RgbPixel>& output,
    int rows,int cols,bool enable,unsigned char clip,unsigned epoch);
