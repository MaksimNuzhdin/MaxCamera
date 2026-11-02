#pragma once
#include "camera_isp.hpp"

// Pass-through RGB8 analytics for the full sensor or ROI. Disabled/smaller-than-32 outputs pass with invalid metrics. Scores are diagnostics,
// not learned classifications. Motion compares 32x32 luma-bin means; focus is
// mean absolute 4-neighbor Laplacian over the interior. Both are Q16 DN.
void camera_scene_metrics_v8(hls::stream<RgbPixel>& input,hls::stream<RgbPixel>& output,
                          int rows,int cols,bool enable,
                          volatile unsigned int& focus_q16,
                          volatile unsigned int& motion_q16,
                          volatile unsigned int& motion_valid,
                          volatile unsigned int& frame_sequence,
                          volatile unsigned int& intensity_sum,
                          volatile unsigned int& clipped_pixels,
                          volatile unsigned int& dark_pixels);
