#pragma once
#include "camera_isp.hpp"
void camera_raw_correct_v5(hls::stream<RawPixel>& input,hls::stream<RawPixel>& output,
    int rows,int cols,bool bpc_enable,unsigned short black_level,
    unsigned short blue_gain,unsigned short green_gain,unsigned short red_gain,
    bool lsc_enable,unsigned short lsc_gains[4][7][9]);
