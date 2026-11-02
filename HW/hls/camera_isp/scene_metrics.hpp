#pragma once
#include "camera_isp.hpp"

// Pass-through RGB8 analytics for a 32..640 pixel ROI. Scores are diagnostics,
// not learned classifications. Motion compares 32x32 luma-bin means; focus is
// mean absolute 4-neighbor Laplacian over the interior. Both are Q16 DN.
void camera_scene_metrics(hls::stream<RgbPixel>& input,hls::stream<RgbPixel>& output,
                          int rows,int cols,bool clear_history,
                          volatile unsigned int& focus_q16,
                          volatile unsigned int& motion_q16,
                          volatile unsigned int& motion_valid,
                          volatile unsigned int& frame_sequence);
