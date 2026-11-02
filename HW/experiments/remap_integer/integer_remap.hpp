#pragma once
#include <ap_axi_sdata.h>
#include <hls_stream.h>
using IntegerRemapPixel = ap_axiu<24,1,1,1>;
// Map word: bit31 valid; bits0..9 source x; bits10..18 source y.
// Bits19..30 must be zero. Invalid entries must be exactly zero (black).
enum IntegerRemapStatus { REMAP_GEOMETRY=1, REMAP_MAP=2, REMAP_FRAMING=4 };
void integer_remap(hls::stream<IntegerRemapPixel>& input,
                   hls::stream<IntegerRemapPixel>& output,
                   const ap_uint<32>* maps, int rows, int cols, bool enabled,
                   unsigned& status);
