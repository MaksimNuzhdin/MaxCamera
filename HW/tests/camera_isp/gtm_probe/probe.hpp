#pragma once
#include "camera_isp.hpp"
using ToneInput=ap_axiu<48,1,1,1>;
// Resource feasibility only. No camera integration or capability is advertised.
void camera_gtm_probe(hls::stream<ToneInput>& input,hls::stream<RgbPixel>& output,
                      int rows,int cols,unsigned c1,unsigned c2,
                      unsigned& mean,unsigned& maximum,unsigned& minimum);
