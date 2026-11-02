#pragma once
#include "camera_isp.hpp"

// BGGR phase order: B, G on blue row, G on red row, R. A 7x9 grid per phase.
// Gains are unsigned Q12 in [0,16384]; unity is 4096. No optical calibration
// is implied by the unity defaults or synthetic tests of this standalone IP.
void camera_raw_lsc(hls::stream<RawPixel>& input,hls::stream<RawPixel>& output,
                    int rows,int cols,bool enable,unsigned short gains[4][7][9]);
