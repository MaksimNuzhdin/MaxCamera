#include "camera_isp.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

static std::vector<unsigned> raw_frame(bool bpc, int fault, unsigned black = 0,
                                      int rows = 16, int cols = 24) {
    hls::stream<RawPixel> input;
    hls::stream<RgbPixel> output;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            // BGGR: B=128, G=384, R=768. Distinct channels detect swaps.
            unsigned value = (y & 1) ? ((x & 1) ? 768 : 384) : ((x & 1) ? 384 : 128);
            if (fault && y == 8 && x == 10) value = fault > 0 ? 1023 : 0;
            RawPixel p;
            p.data = value + black; p.keep = 3; p.strb = 3;
            p.user = (y == 0 && x == 0); p.last = (x == cols - 1);
            p.id = 0; p.dest = 0; input.write(p);
        }
    }
    camera_raw_isp(input, output, rows, cols, bpc, black, 256, 256, 256);
    std::vector<unsigned> result;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            assert(!output.empty());
            RgbPixel p = output.read();
            assert(p.user == (y == 0 && x == 0));
            assert(p.last == (x == cols - 1));
            assert(p.keep == 7 && p.strb == 7);
            result.push_back(p.data.to_uint());
            if (!fault && y >= 4 && y < rows - 4 && x >= 4 && x < cols - 4) {
                assert(p.data.range(7, 0) == 192);
                assert(p.data.range(15, 8) == 96);
                assert(p.data.range(23, 16) == 32);
            }
        }
    }
    assert(input.empty() && output.empty());
    return result;
}

static void resize_case(int rows, int cols, int rx, int ry, int rw, int rh,
                        int oh, int ow, bool constant) {
    hls::stream<RgbPixel> input, output;
    std::vector<unsigned> source(rows * cols * 3);
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            RgbPixel p; p.data = 0;
            for (int c = 0; c < 3; ++c) {
                unsigned value = constant ? 128 : (x * 37 + y * 53 + c * 71) % 256;
                source[(y * cols + x) * 3 + c] = value;
                p.data.range(c * 8 + 7, c * 8) = value;
            }
            p.keep = 7; p.strb = 7; p.user = (x == 0 && y == 0);
            p.last = (x == cols - 1); p.id = 0; p.dest = 0; input.write(p);
        }
    }
    unsigned long long sum = 0;
    unsigned high = 0, low = 0;
    camera_ai_resize(input, output, rows, cols, rx, ry, rw, rh, oh, ow, sum, high, low);
    unsigned long long actual_sum = 0;
    unsigned actual_high = 0, actual_low = 0;
    for (int y = 0; y < oh; ++y) {
        for (int x = 0; x < ow; ++x) {
            assert(!output.empty());
            RgbPixel p = output.read();
            assert(p.user == (x == 0 && y == 0)); assert(p.last == (x == ow - 1));
            unsigned channels[3];
            for (int c = 0; c < 3; ++c) {
                channels[c] = p.data.range(c * 8 + 7, c * 8);
                // Independent floating-point integration of source pixel areas.
                double left = double(x) * rw / ow, right = double(x + 1) * rw / ow;
                double top = double(y) * rh / oh, bottom = double(y + 1) * rh / oh;
                double integral = 0;
                for (int sy = int(top); sy < int(std::ceil(bottom)); ++sy) {
                    for (int sx = int(left); sx < int(std::ceil(right)); ++sx) {
                        double weight = (std::min(right, double(sx + 1)) - std::max(left, double(sx))) *
                                        (std::min(bottom, double(sy + 1)) - std::max(top, double(sy)));
                        integral += weight * source[((sy + ry) * cols + sx + rx) * 3 + c];
                    }
                }
                int expected = int(std::round(integral / ((right - left) * (bottom - top))));
                if (std::abs(int(channels[c]) - expected) > 1)
                    std::cerr << "Resize mismatch: source=" << rw << 'x' << rh
                              << " output=" << ow << 'x' << oh << " pixel=" << x << ',' << y
                              << " channel=" << c << " actual=" << channels[c]
                              << " expected=" << expected << '\n';
                assert(std::abs(int(channels[c]) - expected) <= 1);
            }
            unsigned luma = (77 * channels[0] + 150 * channels[1] + 29 * channels[2] + 128) >> 8;
            actual_sum += luma;
            actual_high += (channels[0] >= 250 || channels[1] >= 250 || channels[2] >= 250);
            actual_low += (luma <= 5);
        }
    }
    assert(sum == actual_sum && high == actual_high && low == actual_low);
    assert(input.empty() && output.empty());
}

static void denoise_case(bool enable) {
    constexpr int rows = 9, cols = 13;
    std::vector<unsigned> source(rows * cols * 3);
    hls::stream<RgbPixel> input, output;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            RgbPixel p; p.data = 0;
            for (int c = 0; c < 3; ++c) {
                unsigned value = (y * 53 + x * 37 + c * 71) % 256;
                source[(y * cols + x) * 3 + c] = value;
                p.data.range(c * 8 + 7, c * 8) = value;
            }
            p.keep = 7; p.strb = 7; p.user = (x == 0 && y == 0);
            p.last = (x == cols - 1); p.id = 0; p.dest = 0; input.write(p);
        }
    }
    camera_rgb_denoise(input, output, rows, cols, enable);
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            assert(!output.empty());
            RgbPixel p = output.read();
            assert(p.user == (x == 0 && y == 0)); assert(p.last == (x == cols - 1));
            for (int c = 0; c < 3; ++c) {
                unsigned neighborhood[9]; int n = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int sy = std::max(0, std::min(rows - 1, y + dy));
                        int sx = std::max(0, std::min(cols - 1, x + dx));
                        neighborhood[n++] = source[(sy * cols + sx) * 3 + c];
                    }
                }
                std::sort(neighborhood, neighborhood + 9);
                unsigned expected = enable ? neighborhood[4] : source[(y * cols + x) * 3 + c];
                assert(p.data.range(c * 8 + 7, c * 8) == expected);
            }
        }
    }
    assert(input.empty() && output.empty());
}

static void raw_correct_case(bool bpc) {
    constexpr int rows=12,cols=32;
    hls::stream<RawPixel> input,output;
    unsigned source[rows][cols];
    for(int y=0;y<rows;++y)for(int x=0;x<cols;++x) {
        unsigned value=(y*53+x*37)%1024;
        if(y==6 && x==12)value=1023;
        source[y][x]=value>31?value-31:0;
        RawPixel p;p.data=value;p.keep=3;p.strb=3;p.user=!y&&!x;
        p.last=x==cols-1;p.id=0;p.dest=0;input.write(p);
    }
    camera_raw_correct(input,output,rows,cols,bpc,31,128,256,512);
    for(int y=0;y<rows;++y)for(int x=0;x<cols;++x) {
        unsigned value=source[y][x];
        if(bpc) {
            unsigned low=1023,high=0;
            for(int dy=-2;dy<=2;dy+=2)for(int dx=-2;dx<=2;dx+=2) {
                if(!dy && !dx)continue;
                unsigned neighbor=(y+dy<0 || y+dy>=rows || x+dx<0 || x+dx>=cols)?0:source[y+dy][x+dx];
                low=std::min(low,neighbor);high=std::max(high,neighbor);
            }
            value=std::max(low,std::min(high,value));
        }
        unsigned gain=(y&1)?((x&1)?512:256):((x&1)?256:128);
        unsigned expected=std::min(1023U,(value*gain+128)>>8);
        assert(!output.empty());RawPixel p=output.read();
        assert(p.data==expected && p.keep==3 && p.strb==3);
        assert(p.user==(!y&&!x) && p.last==(x==cols-1));
    }
    assert(input.empty() && output.empty());
}

static void color_pipeline_case(bool median, bool bpc, int rx, int ry,
                                int rw, int rh, int oh, int ow) {
    constexpr int rows = 16, cols = 32;
    hls::stream<RawPixel> input, reference_input;
    hls::stream<RgbPixel> output, rgb, filtered, reference;
    for (int y = 0; y < rows; ++y) for (int x = 0; x < cols; ++x) {
        RawPixel p;
        p.data = (x * 37 + y * 53) % 1024;
        p.keep = 3; p.strb = 3; p.user = !x && !y;
        p.last = x == cols - 1; p.id = 0; p.dest = 0;
        input.write(p); reference_input.write(p);
    }
    unsigned long long sum, expected_sum;
    unsigned high, low, expected_high, expected_low;
    camera_color_pipeline(input, output, rows, cols, bpc, 31, 128, 256, 512,
                          median, rx, ry, rw, rh, oh, ow, sum, high, low);
    // Composition reference detects ordering, crop coordinate and frame errors.
    // The component tests above also use independent numeric references.
    camera_raw_isp(reference_input, rgb, rows, cols, bpc, 31, 128, 256, 512);
    camera_rgb_denoise(rgb, filtered, rows, cols, median);
    camera_ai_resize(filtered, reference, rows, cols, rx, ry, rw, rh, oh, ow,
                     expected_sum, expected_high, expected_low);
    for (int y = 0; y < oh; ++y) for (int x = 0; x < ow; ++x) {
        assert(!output.empty() && !reference.empty());
        RgbPixel actual = output.read(), expected = reference.read();
        assert(actual.data == expected.data);
        assert(actual.user == (!x && !y) && actual.last == (x == ow - 1));
        assert(actual.keep == 7 && actual.strb == 7);
    }
    assert(sum == expected_sum && high == expected_high && low == expected_low);
    assert(input.empty() && output.empty() && reference.empty() &&
           reference_input.empty() && rgb.empty() && filtered.empty());
}

int main() {
    color_pipeline_case(false, false, 0, 0, 32, 16, 16, 32);
    color_pipeline_case(true, true, 3, 5, 21, 9, 4, 7);
    color_pipeline_case(true, false, 0, 0, 32, 16, 1, 1);
    raw_correct_case(false);raw_correct_case(true);
    auto clean = raw_frame(true, 0);
    assert(clean == raw_frame(true, 1));
    assert(clean == raw_frame(true, -1));
    assert(clean != raw_frame(false, 1));
    assert(raw_frame(false, 0) == raw_frame(false, 0, 32));
    raw_frame(true, 0, 0, 16, MAX_COLS);
    resize_case(24, 32, 0, 0, 32, 24, 12, 16, false);
    resize_case(24, 32, 3, 5, 21, 15, 7, 9, false);
    resize_case(24, 32, 2, 2, 28, 20, 5, 7, true);
    resize_case(24, 32, 0, 0, 32, 24, 24, 32, false);
    resize_case(8, MAX_COLS, 0, 0, MAX_COLS, 8, 4, AI_COLS, true);
    resize_case(MAX_ROWS, 8, 0, 0, 8, MAX_ROWS, AI_ROWS, 4, true);
    resize_case(8, 16, 0, 0, 16, 8, 1, 1, false);
    resize_case(1, 1, 0, 0, 1, 1, 1, 1, false);
    denoise_case(false);
    denoise_case(true);
    hls::stream<RgbPixel> input, output;
    unsigned long long sum = 99; unsigned high = 99, low = 99;
    camera_ai_resize(input, output, 24, 32, 31, 0, 2, 24, 12, 16, sum, high, low);
    assert(input.empty() && output.empty() && !sum && !high && !low);
    std::cout << "PASS camera ISP: BGGR, hot/dead pixels, BLC, crop, area resize, median, AXIS markers, statistics, bounds\n";
}
