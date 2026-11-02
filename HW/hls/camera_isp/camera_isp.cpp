#include "camera_isp.hpp"
#include "common/xf_common.hpp"
#include "imgproc/xf_bpc.hpp"
#include "imgproc/xf_demosaicing.hpp"
#include "imgproc/xf_median_blur.hpp"

using RawMat = xf::cv::Mat<XF_10UC1, MAX_ROWS, MAX_COLS, XF_NPPC1, 2>;
using ColorMat = xf::cv::Mat<XF_10UC3, MAX_ROWS, MAX_COLS, XF_NPPC1, 2>;
using RgbMat = xf::cv::Mat<XF_8UC3, MAX_ROWS, MAX_COLS, XF_NPPC1, 2>;
using AiMat = xf::cv::Mat<XF_8UC3, AI_ROWS, AI_COLS, XF_NPPC1, 2>;

static void read_raw(hls::stream<RawPixel>& input, RawMat& image,
                     unsigned short black) {
    for (int i = 0; i < image.rows * image.cols; ++i) {
#pragma HLS PIPELINE II=1
        ap_uint<10> value = input.read().data.range(9, 0);
        image.write(i, value > black ? ap_uint<10>(value - black) : ap_uint<10>(0));
    }
}

static void correct_bad_pixels(RawMat& input, RawMat& output, bool enable) {
    if (enable) {
        // The AMD 5x5 kernel compares eight neighbors of the same CFA phase.
        xf::cv::badpixelcorrection<XF_10UC1, MAX_ROWS, MAX_COLS,
            XF_NPPC1, XF_BORDER_CONSTANT, 0, 1, 2, 2>(input, output);
    } else {
        for (int i = 0; i < input.rows * input.cols; ++i) {
#pragma HLS PIPELINE II=1
            output.write(i, input.read(i));
        }
    }
}

static void write_rgb(ColorMat& image, hls::stream<RgbPixel>& output,
                      unsigned short bg, unsigned short gg, unsigned short rg) {
    for (int y = 0; y < image.rows; ++y) {
        for (int x = 0; x < image.cols; ++x) {
#pragma HLS PIPELINE II=1
            // AMD demosaic packs B, G, R from least to most significant bits.
            ap_uint<30> pixel = image.read(y * image.cols + x);
            ap_uint<26> b = pixel.range(9, 0) * ap_uint<16>(bg);
            ap_uint<26> g = pixel.range(19, 10) * ap_uint<16>(gg);
            ap_uint<26> r = pixel.range(29, 20) * ap_uint<16>(rg);
            // Q8.8 gains, then round 10-bit channels to saturated RGB8.
            ap_uint<18> bv = (b + 512) >> 10;
            ap_uint<18> gv = (g + 512) >> 10;
            ap_uint<18> rv = (r + 512) >> 10;
            RgbPixel word;
            word.data.range(7, 0) = rv > 255 ? 255 : rv.to_uint();
            word.data.range(15, 8) = gv > 255 ? 255 : gv.to_uint();
            word.data.range(23, 16) = bv > 255 ? 255 : bv.to_uint();
            word.keep = 7; word.strb = 7;
            word.user = (x == 0 && y == 0); word.last = (x == image.cols - 1);
            word.id = 0; word.dest = 0;
            output.write(word);
        }
    }
}

static void convert_rgb(ColorMat& image, RgbMat& output,
                      unsigned short bg, unsigned short gg, unsigned short rg) {
    for (int y = 0; y < image.rows; ++y) {
        for (int x = 0; x < image.cols; ++x) {
#pragma HLS PIPELINE II=1
            // AMD demosaic packs B, G, R from least to most significant bits.
            ap_uint<30> pixel = image.read(y * image.cols + x);
            ap_uint<26> b = pixel.range(9, 0) * ap_uint<16>(bg);
            ap_uint<26> g = pixel.range(19, 10) * ap_uint<16>(gg);
            ap_uint<26> r = pixel.range(29, 20) * ap_uint<16>(rg);
            // Q8.8 gains, then round 10-bit channels to saturated RGB8.
            ap_uint<18> bv = (b + 512) >> 10;
            ap_uint<18> gv = (g + 512) >> 10;
            ap_uint<18> rv = (r + 512) >> 10;
            ap_uint<24> word;
            word.range(7, 0) = rv > 255 ? 255 : rv.to_uint();
            word.range(15, 8) = gv > 255 ? 255 : gv.to_uint();
            word.range(23, 16) = bv > 255 ? 255 : bv.to_uint();
            output.write(y * image.cols + x, word);
        }
    }
}

static void write_bayer(RawMat& image,hls::stream<RawPixel>& output,
                        unsigned short bg,unsigned short gg,unsigned short rg) {
    for(int y=0;y<image.rows;++y) {
        for(int x=0;x<image.cols;++x) {
#pragma HLS PIPELINE II=1
            ap_uint<10> pixel=image.read(y*image.cols+x);
            ap_uint<16> gain=(y&1)?((x&1)?rg:gg):((x&1)?gg:bg);
            ap_uint<26> scaled=pixel*gain+128;
            ap_uint<18> value=scaled>>8;
            RawPixel word;
            word.data=value>1023?1023:value.to_uint();
            word.keep=3;word.strb=3;word.user=(x==0 && y==0);
            word.last=(x==image.cols-1);word.id=0;word.dest=0;
            output.write(word);
        }
    }
}

static void raw_correct_frame(hls::stream<RawPixel>& input,
                        hls::stream<RawPixel>& output,
                        int rows,int cols,bool bpc_enable,
                        unsigned short black_level,
                        unsigned short blue_gain,unsigned short green_gain,
                        unsigned short red_gain) {
#pragma HLS INLINE off
#pragma HLS DATAFLOW
    RawMat raw(rows,cols),corrected(rows,cols);
    read_raw(input,raw,black_level);
    correct_bad_pixels(raw,corrected,bpc_enable);
    write_bayer(corrected,output,blue_gain,green_gain,red_gain);
}

void camera_raw_correct(hls::stream<RawPixel>& input,
                        hls::stream<RawPixel>& output,
                        int rows,int cols,bool bpc_enable,
                        unsigned short black_level,
                        unsigned short blue_gain,unsigned short green_gain,
                        unsigned short red_gain) {
#pragma HLS INTERFACE axis port=input
#pragma HLS INTERFACE axis port=output
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=bpc_enable bundle=control
#pragma HLS INTERFACE s_axilite port=black_level bundle=control
#pragma HLS INTERFACE s_axilite port=blue_gain bundle=control
#pragma HLS INTERFACE s_axilite port=green_gain bundle=control
#pragma HLS INTERFACE s_axilite port=red_gain bundle=control
    if(rows<8 || rows>MAX_ROWS || cols<16 || cols>MAX_COLS ||
       (rows&1) || (cols&15) || black_level>1023)return;
    raw_correct_frame(input,output,rows,cols,bpc_enable,black_level,blue_gain,green_gain,red_gain);
}

static void raw_isp_frame(hls::stream<RawPixel>& input,
                    hls::stream<RgbPixel>& output,
                    int rows, int cols, bool bpc_enable,
                    unsigned short black_level,
                    unsigned short blue_gain, unsigned short green_gain,
                    unsigned short red_gain) {
#pragma HLS INLINE off
#pragma HLS DATAFLOW
    RawMat raw(rows, cols), corrected(rows, cols);
    ColorMat rgb(rows, cols);
    unsigned short pattern = XF_BAYER_BG;
    read_raw(input, raw, black_level);
    correct_bad_pixels(raw, corrected, bpc_enable);
    xf::cv::demosaicing<XF_10UC1, XF_10UC3, MAX_ROWS, MAX_COLS,
                       XF_NPPC1, false, 2, 2>(corrected, rgb, pattern);
    write_rgb(rgb, output, blue_gain, green_gain, red_gain);
}

void camera_raw_isp(hls::stream<RawPixel>& input,
                    hls::stream<RgbPixel>& output,
                    int rows, int cols, bool bpc_enable,
                    unsigned short black_level,
                    unsigned short blue_gain, unsigned short green_gain,
                    unsigned short red_gain) {
#pragma HLS INTERFACE axis port=input
#pragma HLS INTERFACE axis port=output
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=bpc_enable bundle=control
#pragma HLS INTERFACE s_axilite port=black_level bundle=control
#pragma HLS INTERFACE s_axilite port=blue_gain bundle=control
#pragma HLS INTERFACE s_axilite port=green_gain bundle=control
#pragma HLS INTERFACE s_axilite port=red_gain bundle=control
    if (rows < 8 || rows > MAX_ROWS || cols < 8 || cols > MAX_COLS ||
        (rows & 1) || (cols & 1) || black_level > 1023) return;
    raw_isp_frame(input,output,rows,cols,bpc_enable,black_level,blue_gain,green_gain,red_gain);
}

static void crop_rgb(hls::stream<RgbPixel>& input, RgbMat& roi,
                     int rows, int cols, int rx, int ry) {
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
#pragma HLS PIPELINE II=1
            RgbPixel pixel = input.read();
            if (y >= ry && y < ry + roi.rows && x >= rx && x < rx + roi.cols)
                roi.write((y - ry) * roi.cols + x - rx, pixel.data);
        }
    }
}

template<class Image>
static void write_ai(Image& image, hls::stream<RgbPixel>& output,
                     unsigned long long& sum, unsigned int& clipped,
                     unsigned int& dark) {
    ap_uint<32> total = 0;
    ap_uint<23> high = 0, low = 0;
    for (int y = 0; y < image.rows; ++y) {
        for (int x = 0; x < image.cols; ++x) {
#pragma HLS PIPELINE II=1
            ap_uint<24> p = image.read(y * image.cols + x);
            unsigned r = p.range(7, 0), g = p.range(15, 8), b = p.range(23, 16);
            unsigned luma = (77 * r + 150 * g + 29 * b + 128) >> 8;
            total += luma;
            high += (r >= 250 || g >= 250 || b >= 250);
            low += (luma <= 5);
            RgbPixel word;
            word.data = p; word.keep = 7; word.strb = 7;
            word.user = (x == 0 && y == 0); word.last = (x == image.cols - 1);
            word.id = 0; word.dest = 0;
            output.write(word);
        }
    }
    sum = total; clipped = high; dark = low;
}

template<int OutputCols, class Image>
static void area_downscale(RgbMat& input, Image& output) {
    // Integer overlap coordinates avoid cumulative Q16.16 boundary drift.
    // Every output footprint has area input.cols * input.rows in this grid.
    ap_uint<96> vertical[OutputCols];
#pragma HLS BIND_STORAGE variable=vertical type=ram_t2p impl=bram
    unsigned area = input.rows * input.cols;
    ap_uint<48> numerator = (ap_uint<48>(1) << 40) + area - 1;
    ap_uint<41> reciprocal = numerator / area;
    unsigned y_phase = 0;
    int out_y = 0;
    for (int y = 0; y < input.rows; ++y) {
        unsigned remaining_y = input.rows - y_phase;
        unsigned yl = remaining_y < unsigned(output.rows) ? remaining_y : unsigned(output.rows);
        unsigned yr = output.rows - yl;
        bool emit_row = y_phase + output.rows >= unsigned(input.rows);
        unsigned x_phase = 0;
        int out_x = 0;
        ap_uint<32> horizontal[3] = {0, 0, 0};
#pragma HLS ARRAY_PARTITION variable=horizontal complete
        for (int x = 0; x < input.cols; ++x) {
#pragma HLS PIPELINE II=1
#pragma HLS DEPENDENCE variable=vertical inter false
            // Each output column is accessed once per source row. The inner
            // pipeline drains before the next row can revisit that address.
            ap_uint<24> pixel = input.read(y * input.cols + x);
            unsigned remaining_x = input.cols - x_phase;
            unsigned xl = remaining_x < unsigned(output.cols) ? remaining_x : unsigned(output.cols);
            unsigned xr = output.cols - xl;
            bool emit_col = x_phase + output.cols >= unsigned(input.cols);
            ap_uint<96> previous = 0, next = 0;
            if (emit_col && y != 0) previous = vertical[out_x];
            ap_uint<24> result = 0;
            for (int c = 0; c < 3; ++c) {
#pragma HLS UNROLL
                ap_uint<8> value = pixel.range(8 * c + 7, 8 * c);
                ap_uint<32> hsum = horizontal[c] + value * xl;
                if (emit_col) {
                    ap_uint<32> old = previous.range(32 * c + 31, 32 * c);
                    ap_uint<32> total = old + hsum * yl;
                    next.range(32 * c + 31, 32 * c) = emit_row ? ap_uint<32>(hsum * yr) : total;
                    ap_uint<72> rounded = total * reciprocal + (ap_uint<72>(1) << 39);
                    result.range(8 * c + 7, 8 * c) = rounded >> 40;
                    horizontal[c] = value * xr;
                } else horizontal[c] = hsum;
            }
            if (emit_col) {
                vertical[out_x] = next;
                if (emit_row) output.write(out_y * output.cols + out_x, result);
                ++out_x;
                x_phase = x_phase + output.cols - input.cols;
            } else x_phase += output.cols;
        }
        if (emit_row) {
            ++out_y;
            y_phase = y_phase + output.rows - input.rows;
        } else y_phase += output.rows;
    }
}

static void ai_resize_frame(hls::stream<RgbPixel>& input,
                      hls::stream<RgbPixel>& output,
                      int rows, int cols, int roi_x, int roi_y,
                      int roi_width, int roi_height,
                      int out_rows, int out_cols,
                      unsigned long long& intensity_sum,
                      unsigned int& clipped_pixels,
                      unsigned int& dark_pixels) {
#pragma HLS INLINE off
#pragma HLS DATAFLOW
    RgbMat roi(roi_height, roi_width);
    AiMat resized(out_rows, out_cols);
    crop_rgb(input, roi, rows, cols, roi_x, roi_y);
    area_downscale<AI_COLS>(roi, resized);
    write_ai(resized, output, intensity_sum, clipped_pixels, dark_pixels);
}

void camera_ai_resize(hls::stream<RgbPixel>& input,
                      hls::stream<RgbPixel>& output,
                      int rows, int cols, int roi_x, int roi_y,
                      int roi_width, int roi_height,
                      int out_rows, int out_cols,
                      unsigned long long& intensity_sum,
                      unsigned int& clipped_pixels,
                      unsigned int& dark_pixels) {
#pragma HLS INTERFACE axis port=input
#pragma HLS INTERFACE axis port=output
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=roi_x bundle=control
#pragma HLS INTERFACE s_axilite port=roi_y bundle=control
#pragma HLS INTERFACE s_axilite port=roi_width bundle=control
#pragma HLS INTERFACE s_axilite port=roi_height bundle=control
#pragma HLS INTERFACE s_axilite port=out_rows bundle=control
#pragma HLS INTERFACE s_axilite port=out_cols bundle=control
#pragma HLS INTERFACE s_axilite port=intensity_sum bundle=control
#pragma HLS INTERFACE s_axilite port=clipped_pixels bundle=control
#pragma HLS INTERFACE s_axilite port=dark_pixels bundle=control
    intensity_sum = 0; clipped_pixels = 0; dark_pixels = 0;
    if (rows < 1 || rows > MAX_ROWS || cols < 1 || cols > MAX_COLS ||
        roi_x < 0 || roi_y < 0 || roi_x >= cols || roi_y >= rows ||
        roi_width < 1 || roi_width > cols - roi_x ||
        roi_height < 1 || roi_height > rows - roi_y ||
        out_rows < 1 || out_rows > AI_ROWS || out_rows > roi_height ||
        out_cols < 1 || out_cols > AI_COLS || out_cols > roi_width) return;
    ai_resize_frame(input,output,rows,cols,roi_x,roi_y,roi_width,roi_height,out_rows,out_cols,intensity_sum,clipped_pixels,dark_pixels);
}

static void median_rgb(RgbMat& input, RgbMat& output, bool enable) {
    if (enable) {
        xf::cv::medianBlur<3, XF_BORDER_REPLICATE, XF_8UC3, MAX_ROWS,
                           MAX_COLS, XF_NPPC1, 0, 2, 2>(input, output);
    } else {
        for (int i = 0; i < input.rows * input.cols; ++i) {
#pragma HLS PIPELINE II=1
            output.write(i, input.read(i));
        }
    }
}

static void stream_rgb(RgbMat& image, hls::stream<RgbPixel>& output) {
    for (int y = 0; y < image.rows; ++y) {
        for (int x = 0; x < image.cols; ++x) {
#pragma HLS PIPELINE II=1
            RgbPixel word;
            word.data = image.read(y * image.cols + x);
            word.keep = 7; word.strb = 7; word.user = (x == 0 && y == 0);
            word.last = (x == image.cols - 1); word.id = 0; word.dest = 0;
            output.write(word);
        }
    }
}

static void rgb_denoise_frame(hls::stream<RgbPixel>& input,
                        hls::stream<RgbPixel>& output,
                        int rows, int cols, bool enable) {
#pragma HLS INLINE off
#pragma HLS DATAFLOW
    RgbMat image(rows, cols), filtered(rows, cols);
    crop_rgb(input, image, rows, cols, 0, 0);
    median_rgb(image, filtered, enable);
    stream_rgb(filtered, output);
}

void camera_rgb_denoise(hls::stream<RgbPixel>& input,
                        hls::stream<RgbPixel>& output,
                        int rows, int cols, bool enable) {
#pragma HLS INTERFACE axis port=input
#pragma HLS INTERFACE axis port=output
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=enable bundle=control
    if (rows < 3 || rows > MAX_ROWS || cols < 3 || cols > MAX_COLS) return;
    rgb_denoise_frame(input,output,rows,cols,enable);
}

static void crop_mat(RgbMat& input, RgbMat& roi, int rx, int ry) {
    for (int y = 0; y < input.rows; ++y) {
        for (int x = 0; x < input.cols; ++x) {
#pragma HLS PIPELINE II=1
            ap_uint<24> pixel = input.read(y * input.cols + x);
            if (y >= ry && y < ry + roi.rows && x >= rx && x < rx + roi.cols)
                roi.write((y - ry) * roi.cols + x - rx, pixel);
        }
    }
}

// Keep guards outside the DATAFLOW region so all streaming processes run concurrently.
static void color_pipeline_frame(hls::stream<RawPixel>& input,
                                hls::stream<RgbPixel>& output,
                                int rows, int cols, bool bpc_enable,
                                unsigned short black_level,
                                unsigned short blue_gain, unsigned short green_gain,
                                unsigned short red_gain, bool median_enable,
                                int roi_x, int roi_y, int roi_width, int roi_height,
                                int out_rows, int out_cols,
                                unsigned long long& intensity_sum,
                                unsigned int& clipped_pixels, unsigned int& dark_pixels) {
#pragma HLS INLINE off
#pragma HLS DATAFLOW
    RawMat raw(rows, cols), corrected(rows, cols);
    ColorMat demosaiced(rows, cols);
    RgbMat rgb(rows, cols), filtered(rows, cols);
    RgbMat roi(roi_height, roi_width), resized(out_rows, out_cols);
    unsigned short pattern = XF_BAYER_BG;
    read_raw(input, raw, black_level);
    correct_bad_pixels(raw, corrected, bpc_enable);
    xf::cv::demosaicing<XF_10UC1, XF_10UC3, MAX_ROWS, MAX_COLS,
                       XF_NPPC1, false, 2, 2>(corrected, demosaiced, pattern);
    convert_rgb(demosaiced, rgb, blue_gain, green_gain, red_gain);
    median_rgb(rgb, filtered, median_enable);
    crop_mat(filtered, roi, roi_x, roi_y);
    area_downscale<MAX_COLS>(roi, resized);
    write_ai(resized, output, intensity_sum, clipped_pixels, dark_pixels);
}

void camera_color_pipeline(hls::stream<RawPixel>& input,
                           hls::stream<RgbPixel>& output,
                           int rows, int cols, bool bpc_enable,
                           unsigned short black_level,
                           unsigned short blue_gain, unsigned short green_gain,
                           unsigned short red_gain, bool median_enable,
                           int roi_x, int roi_y, int roi_width, int roi_height,
                           int out_rows, int out_cols,
                           unsigned long long& intensity_sum,
                           unsigned int& clipped_pixels, unsigned int& dark_pixels) {
#pragma HLS INTERFACE axis port=input
#pragma HLS INTERFACE axis port=output
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=bpc_enable bundle=control
#pragma HLS INTERFACE s_axilite port=black_level bundle=control
#pragma HLS INTERFACE s_axilite port=blue_gain bundle=control
#pragma HLS INTERFACE s_axilite port=green_gain bundle=control
#pragma HLS INTERFACE s_axilite port=red_gain bundle=control
#pragma HLS INTERFACE s_axilite port=median_enable bundle=control
#pragma HLS INTERFACE s_axilite port=roi_x bundle=control
#pragma HLS INTERFACE s_axilite port=roi_y bundle=control
#pragma HLS INTERFACE s_axilite port=roi_width bundle=control
#pragma HLS INTERFACE s_axilite port=roi_height bundle=control
#pragma HLS INTERFACE s_axilite port=out_rows bundle=control
#pragma HLS INTERFACE s_axilite port=out_cols bundle=control
#pragma HLS INTERFACE s_axilite port=intensity_sum bundle=control
#pragma HLS INTERFACE s_axilite port=clipped_pixels bundle=control
#pragma HLS INTERFACE s_axilite port=dark_pixels bundle=control
    intensity_sum = 0; clipped_pixels = 0; dark_pixels = 0;
    if (rows < 8 || rows > MAX_ROWS || cols < 16 || cols > MAX_COLS ||
        (rows & 1) || (cols & 15) || black_level > 1023 ||
        roi_x < 0 || roi_y < 0 || roi_x >= cols || roi_y >= rows ||
        roi_width < 1 || roi_width > cols - roi_x ||
        roi_height < 1 || roi_height > rows - roi_y ||
        out_rows < 1 || out_rows > roi_height ||
        out_cols < 1 || out_cols > roi_width) return;
    color_pipeline_frame(input, output, rows, cols, bpc_enable, black_level,
                         blue_gain, green_gain, red_gain, median_enable,
                         roi_x, roi_y, roi_width, roi_height, out_rows, out_cols,
                         intensity_sum, clipped_pixels, dark_pixels);
}


#include "color_adjust_internal.hpp"
#include "color_pipeline_v3.hpp"

static void gain_color_stats(ColorMat& input, ColorImage& output,
    unsigned short bg, unsigned short gg, unsigned short rg,
    unsigned long long& red_sum, unsigned long long& green_sum,
    unsigned long long& blue_sum, unsigned int& valid_pixels) {
    ap_uint<40> sums[3]={0,0,0};
#pragma HLS ARRAY_PARTITION variable=sums complete
    ap_uint<23> count=0;
    for(int i=0;i<input.rows*input.cols;++i) {
#pragma HLS PIPELINE II=1
        ap_uint<30> p=input.read(i),result;
        ap_uint<10> r=p.range(29,20),g=p.range(19,10),b=p.range(9,0);
        if(r>=8 && g>=8 && b>=8 && r<1000 && g<1000 && b<1000) {
            sums[0]+=r;sums[1]+=g;sums[2]+=b;++count;
        }
        ap_uint<10> channels[3]={r,g,b};
        ap_uint<16> gains[3]={rg,gg,bg};
#pragma HLS ARRAY_PARTITION variable=channels complete
#pragma HLS ARRAY_PARTITION variable=gains complete
        for(int c=0;c<3;++c) {
#pragma HLS UNROLL
            ap_uint<27> scaled=channels[c]*gains[c]+128;
            ap_uint<19> value=scaled>>8;
            result.range(c*10+9,c*10)=value>1023?1023:value.to_uint();
        }
        output.write(i,result);
    }
    red_sum=sums[0];green_sum=sums[1];blue_sum=sums[2];valid_pixels=count;
}

static void color_frame_v3(hls::stream<RawPixel>& input,hls::stream<RgbPixel>& output,
    int rows,int cols,bool bpc_enable,unsigned short black_level,
    unsigned short bg,unsigned short gg,unsigned short rg,bool median_enable,
    int rx,int ry,int rw,int rh,int oh,int ow,bool ccm_enable,bool gamma_enable,
    int matrix[3][3],int offsets[3],unsigned char gamma[768],
    unsigned long long& sum,unsigned int& clipped,unsigned int& dark,
    unsigned long long& rs,unsigned long long& gs,unsigned long long& bs,unsigned int& count) {
#pragma HLS INLINE off
#pragma HLS DATAFLOW
    RawMat raw(rows,cols),corrected(rows,cols);
    ColorMat demosaiced(rows,cols);
    ColorImage balanced(rows,cols),colored(rows,cols);
    ByteImage quantized(rows,cols),mapped(rows,cols);
    RgbMat filtered(rows,cols),roi(rh,rw),resized(oh,ow);
    unsigned short pattern=XF_BAYER_BG;
    read_raw(input,raw,black_level);
    correct_bad_pixels(raw,corrected,bpc_enable);
    xf::cv::demosaicing<XF_10UC1,XF_10UC3,MAX_ROWS,MAX_COLS,XF_NPPC1,false,2,2>(corrected,demosaiced,pattern);
    gain_color_stats(demosaiced,balanced,bg,gg,rg,rs,gs,bs,count);
    correct_color(balanced,colored,ccm_enable,matrix,offsets);
    quantize_color(colored,quantized);
    apply_gamma(quantized,mapped,gamma_enable,gamma);
    median_rgb(mapped,filtered,median_enable);
    crop_mat(filtered,roi,rx,ry);
    area_downscale<MAX_COLS>(roi,resized);
    write_ai(resized,output,sum,clipped,dark);
}

static unsigned short next_awb_gain(unsigned long long green,unsigned long long channel,unsigned short old) {
    unsigned long long ratio=(green*256+channel/2)/channel;
    unsigned target=ratio<64?64:ratio>1024?1024:unsigned(ratio);
    return (3*unsigned(old)+target+2)/4;
}

void camera_color_pipeline_v3(hls::stream<RawPixel>& input, hls::stream<RgbPixel>& output,
    int rows, int cols, bool bpc_enable, unsigned short black_level,
    unsigned short blue_gain, unsigned short green_gain, unsigned short red_gain,
    bool median_enable, int roi_x, int roi_y, int roi_width, int roi_height,
    int out_rows, int out_cols, volatile unsigned long long& intensity_sum,
    volatile unsigned int& clipped_pixels, volatile unsigned int& dark_pixels,
    bool ccm_enable, bool gamma_enable, bool awb_enable,
    int matrix[3][3], int offsets[3], unsigned char gamma[768],
    volatile unsigned long long& red_sum, volatile unsigned long long& green_sum,
    volatile unsigned long long& blue_sum, volatile unsigned int& valid_pixels,
    volatile unsigned int& applied_red, volatile unsigned int& applied_green, volatile unsigned int& applied_blue,
    volatile unsigned int& frame_sequence) {
#pragma HLS INTERFACE axis port=input
#pragma HLS INTERFACE axis port=output
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=bpc_enable bundle=control
#pragma HLS INTERFACE s_axilite port=black_level bundle=control
#pragma HLS INTERFACE s_axilite port=blue_gain bundle=control
#pragma HLS INTERFACE s_axilite port=green_gain bundle=control
#pragma HLS INTERFACE s_axilite port=red_gain bundle=control
#pragma HLS INTERFACE s_axilite port=median_enable bundle=control
#pragma HLS INTERFACE s_axilite port=roi_x bundle=control
#pragma HLS INTERFACE s_axilite port=roi_y bundle=control
#pragma HLS INTERFACE s_axilite port=roi_width bundle=control
#pragma HLS INTERFACE s_axilite port=roi_height bundle=control
#pragma HLS INTERFACE s_axilite port=out_rows bundle=control
#pragma HLS INTERFACE s_axilite port=out_cols bundle=control
#pragma HLS INTERFACE s_axilite port=intensity_sum bundle=control
#pragma HLS INTERFACE s_axilite port=clipped_pixels bundle=control
#pragma HLS INTERFACE s_axilite port=dark_pixels bundle=control
#pragma HLS INTERFACE s_axilite port=ccm_enable bundle=control
#pragma HLS INTERFACE s_axilite port=gamma_enable bundle=control
#pragma HLS INTERFACE s_axilite port=awb_enable bundle=control
#pragma HLS INTERFACE s_axilite port=matrix bundle=control
#pragma HLS INTERFACE s_axilite port=offsets bundle=control
#pragma HLS INTERFACE s_axilite port=gamma bundle=control
#pragma HLS INTERFACE s_axilite port=red_sum bundle=control
#pragma HLS INTERFACE s_axilite port=green_sum bundle=control
#pragma HLS INTERFACE s_axilite port=blue_sum bundle=control
#pragma HLS INTERFACE s_axilite port=valid_pixels bundle=control
#pragma HLS INTERFACE s_axilite port=applied_red bundle=control
#pragma HLS INTERFACE s_axilite port=applied_green bundle=control
#pragma HLS INTERFACE s_axilite port=applied_blue bundle=control
#pragma HLS INTERFACE s_axilite port=frame_sequence bundle=control
    static unsigned short auto_red=256,auto_blue=256;
    static unsigned sequence=0;
    static bool previous_auto=false;
#pragma HLS RESET variable=auto_red
#pragma HLS RESET variable=auto_blue
#pragma HLS RESET variable=sequence
#pragma HLS RESET variable=previous_auto
    bool valid=rows>=8 && rows<=MAX_ROWS && !(rows&1) && cols>=16 && cols<=MAX_COLS && !(cols&15) &&
        black_level<=1023 && roi_x>=0 && roi_x<cols && roi_y>=0 && roi_y<rows &&
        roi_width>=1 && roi_width<=cols-roi_x && roi_height>=1 && roi_height<=rows-roi_y &&
        out_rows>=1 && out_rows<=roi_height && out_cols>=1 && out_cols<=roi_width;
    if(ccm_enable)for(int i=0;i<3;++i) {
        valid&=offsets[i]>=-8388608 && offsets[i]<=8388607;
        for(int j=0;j<3;++j)valid&=matrix[i][j]>=-8388608 && matrix[i][j]<=8388607;
    }
    if(!valid) {
        intensity_sum=0;red_sum=0;green_sum=0;blue_sum=0;
        clipped_pixels=0;dark_pixels=0;valid_pixels=0;
        applied_red=0;applied_green=0;applied_blue=0;frame_sequence=0;return;
    }
    if(awb_enable && !previous_auto){auto_red=256;auto_blue=256;}
    unsigned short rg=awb_enable?auto_red:red_gain;
    unsigned short gg=awb_enable?256:green_gain;
    unsigned short bg=awb_enable?auto_blue:blue_gain;
    unsigned long long rs,gs,bs,output_sum;unsigned count,output_high,output_low;
    color_frame_v3(input,output,rows,cols,bpc_enable,black_level,bg,gg,rg,median_enable,
        roi_x,roi_y,roi_width,roi_height,out_rows,out_cols,ccm_enable,gamma_enable,matrix,offsets,gamma,
        output_sum,output_high,output_low,rs,gs,bs,count);
    if(awb_enable && count>=64 && rs && gs && bs) {
        auto_red=next_awb_gain(gs,rs,rg);auto_blue=next_awb_gain(gs,bs,bg);
    }
    previous_auto=awb_enable;
    // Volatile publication gives AXI readers an odd/even consistency epoch.
    // The marker brackets every result write; it is stable/even between frames.
    frame_sequence=(++sequence<<1)|1U;
    intensity_sum=output_sum;clipped_pixels=output_high;dark_pixels=output_low;
    red_sum=rs;green_sum=gs;blue_sum=bs;valid_pixels=count;
    applied_red=rg;applied_green=gg;applied_blue=bg;
    frame_sequence=sequence<<1;
}

#include "rgb_detail_internal.hpp"
#include "color_pipeline_v4.hpp"

static void color_frame_v4(hls::stream<RawPixel>& input,hls::stream<RgbPixel>& output,
    int rows,int cols,bool bpc_enable,unsigned short black_level,
    unsigned short bg,unsigned short gg,unsigned short rg,bool median_enable,
    int rx,int ry,int rw,int rh,int oh,int ow,bool ccm_enable,bool gamma_enable,
    int matrix[3][3],int offsets[3],unsigned char gamma[768],
    bool bilateral_enable,unsigned short sigma_color,unsigned short sigma_space_q8,unsigned short sharpen_strength,
    unsigned long long& sum,unsigned int& clipped,unsigned int& dark,
    unsigned long long& rs,unsigned long long& gs,unsigned long long& bs,unsigned int& count) {
#pragma HLS INLINE off
#pragma HLS DATAFLOW
    RawMat raw(rows,cols),corrected(rows,cols);
    ColorMat demosaiced(rows,cols);
    ColorImage balanced(rows,cols),colored(rows,cols);
    ByteImage quantized(rows,cols),mapped(rows,cols);
    RgbMat filtered(rows,cols),denoised(rows,cols),enhanced(rows,cols),roi(rh,rw),resized(oh,ow);
    unsigned short pattern=XF_BAYER_BG;
    read_raw(input,raw,black_level);
    correct_bad_pixels(raw,corrected,bpc_enable);
    xf::cv::demosaicing<XF_10UC1,XF_10UC3,MAX_ROWS,MAX_COLS,XF_NPPC1,false,2,2>(corrected,demosaiced,pattern);
    gain_color_stats(demosaiced,balanced,bg,gg,rg,rs,gs,bs,count);
    correct_color(balanced,colored,ccm_enable,matrix,offsets);
    quantize_color(colored,quantized);
    apply_gamma(quantized,mapped,gamma_enable,gamma);
    median_rgb(mapped,filtered,median_enable);
    bilateral_rgb(filtered,denoised,bilateral_enable,sigma_color,sigma_space_q8);
    sharpen_rgb(denoised,enhanced,sharpen_strength);
    crop_mat(enhanced,roi,rx,ry);
    area_downscale<MAX_COLS>(roi,resized);
    write_ai(resized,output,sum,clipped,dark);
}

void camera_color_pipeline_v4(hls::stream<RawPixel>& input, hls::stream<RgbPixel>& output,
    int rows, int cols, bool bpc_enable, unsigned short black_level,
    unsigned short blue_gain, unsigned short green_gain, unsigned short red_gain,
    bool median_enable, int roi_x, int roi_y, int roi_width, int roi_height,
    int out_rows, int out_cols, volatile unsigned long long& intensity_sum,
    volatile unsigned int& clipped_pixels, volatile unsigned int& dark_pixels,
    bool ccm_enable, bool gamma_enable, bool awb_enable,
    int matrix[3][3], int offsets[3], unsigned char gamma[768],
    volatile unsigned long long& red_sum, volatile unsigned long long& green_sum,
    volatile unsigned long long& blue_sum, volatile unsigned int& valid_pixels,
    volatile unsigned int& applied_red, volatile unsigned int& applied_green, volatile unsigned int& applied_blue,
    volatile unsigned int& frame_sequence,
    bool bilateral_enable,unsigned short sigma_color,unsigned short sigma_space_q8,unsigned short sharpen_strength) {
#pragma HLS INTERFACE axis port=input
#pragma HLS INTERFACE axis port=output
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=bpc_enable bundle=control
#pragma HLS INTERFACE s_axilite port=black_level bundle=control
#pragma HLS INTERFACE s_axilite port=blue_gain bundle=control
#pragma HLS INTERFACE s_axilite port=green_gain bundle=control
#pragma HLS INTERFACE s_axilite port=red_gain bundle=control
#pragma HLS INTERFACE s_axilite port=median_enable bundle=control
#pragma HLS INTERFACE s_axilite port=roi_x bundle=control
#pragma HLS INTERFACE s_axilite port=roi_y bundle=control
#pragma HLS INTERFACE s_axilite port=roi_width bundle=control
#pragma HLS INTERFACE s_axilite port=roi_height bundle=control
#pragma HLS INTERFACE s_axilite port=out_rows bundle=control
#pragma HLS INTERFACE s_axilite port=out_cols bundle=control
#pragma HLS INTERFACE s_axilite port=intensity_sum bundle=control
#pragma HLS INTERFACE s_axilite port=clipped_pixels bundle=control
#pragma HLS INTERFACE s_axilite port=dark_pixels bundle=control
#pragma HLS INTERFACE s_axilite port=ccm_enable bundle=control
#pragma HLS INTERFACE s_axilite port=gamma_enable bundle=control
#pragma HLS INTERFACE s_axilite port=awb_enable bundle=control
#pragma HLS INTERFACE s_axilite port=matrix bundle=control
#pragma HLS INTERFACE s_axilite port=offsets bundle=control
#pragma HLS INTERFACE s_axilite port=gamma bundle=control
#pragma HLS INTERFACE s_axilite port=red_sum bundle=control
#pragma HLS INTERFACE s_axilite port=green_sum bundle=control
#pragma HLS INTERFACE s_axilite port=blue_sum bundle=control
#pragma HLS INTERFACE s_axilite port=valid_pixels bundle=control
#pragma HLS INTERFACE s_axilite port=applied_red bundle=control
#pragma HLS INTERFACE s_axilite port=applied_green bundle=control
#pragma HLS INTERFACE s_axilite port=applied_blue bundle=control
#pragma HLS INTERFACE s_axilite port=frame_sequence bundle=control
#pragma HLS INTERFACE s_axilite port=bilateral_enable bundle=control
#pragma HLS INTERFACE s_axilite port=sigma_color bundle=control
#pragma HLS INTERFACE s_axilite port=sigma_space_q8 bundle=control
#pragma HLS INTERFACE s_axilite port=sharpen_strength bundle=control
    static unsigned short auto_red=256,auto_blue=256;
    static unsigned sequence=0;
    static bool previous_auto=false;
#pragma HLS RESET variable=auto_red
#pragma HLS RESET variable=auto_blue
#pragma HLS RESET variable=sequence
#pragma HLS RESET variable=previous_auto
    bool valid=rows>=8 && rows<=MAX_ROWS && !(rows&1) && cols>=16 && cols<=MAX_COLS && !(cols&15) &&
        black_level<=1023 && roi_x>=0 && roi_x<cols && roi_y>=0 && roi_y<rows &&
        roi_width>=1 && roi_width<=cols-roi_x && roi_height>=1 && roi_height<=rows-roi_y &&
        out_rows>=1 && out_rows<=roi_height && out_cols>=1 && out_cols<=roi_width;
    valid &= sharpen_strength<=256 && (!bilateral_enable || (sigma_color>=1 && sigma_color<=255 && sigma_space_q8>=64 && sigma_space_q8<=1024));
    if(ccm_enable)for(int i=0;i<3;++i) {
        valid&=offsets[i]>=-8388608 && offsets[i]<=8388607;
        for(int j=0;j<3;++j)valid&=matrix[i][j]>=-8388608 && matrix[i][j]<=8388607;
    }
    if(!valid) {
        intensity_sum=0;red_sum=0;green_sum=0;blue_sum=0;
        clipped_pixels=0;dark_pixels=0;valid_pixels=0;
        applied_red=0;applied_green=0;applied_blue=0;frame_sequence=0;return;
    }
    if(awb_enable && !previous_auto){auto_red=256;auto_blue=256;}
    unsigned short rg=awb_enable?auto_red:red_gain;
    unsigned short gg=awb_enable?256:green_gain;
    unsigned short bg=awb_enable?auto_blue:blue_gain;
    unsigned long long rs,gs,bs,output_sum;unsigned count,output_high,output_low;
    color_frame_v4(input,output,rows,cols,bpc_enable,black_level,bg,gg,rg,median_enable,
        roi_x,roi_y,roi_width,roi_height,out_rows,out_cols,ccm_enable,gamma_enable,matrix,offsets,gamma,
        bilateral_enable,sigma_color,sigma_space_q8,sharpen_strength,
        output_sum,output_high,output_low,rs,gs,bs,count);
    if(awb_enable && count>=64 && rs && gs && bs) {
        auto_red=next_awb_gain(gs,rs,rg);auto_blue=next_awb_gain(gs,bs,bg);
    }
    previous_auto=awb_enable;
    // Volatile publication gives AXI readers an odd/even consistency epoch.
    // The marker brackets every result write; it is stable/even between frames.
    frame_sequence=(++sequence<<1)|1U;
    intensity_sum=output_sum;clipped_pixels=output_high;dark_pixels=output_low;
    red_sum=rs;green_sum=gs;blue_sum=bs;valid_pixels=count;
    applied_red=rg;applied_green=gg;applied_blue=bg;
    frame_sequence=sequence<<1;
}

#include "raw_lsc_internal.hpp"
#include "color_pipeline_v5.hpp"

static void color_frame_v5(hls::stream<RawPixel>& input,hls::stream<RgbPixel>& output,
    int rows,int cols,bool bpc_enable,unsigned short black_level,
    unsigned short bg,unsigned short gg,unsigned short rg,bool median_enable,
    int rx,int ry,int rw,int rh,int oh,int ow,bool ccm_enable,bool gamma_enable,
    int matrix[3][3],int offsets[3],unsigned char gamma[768],
    bool bilateral_enable,unsigned short sigma_color,unsigned short sigma_space_q8,unsigned short sharpen_strength,
    bool lsc_enable,unsigned short lsc_gains[4][7][9],
    unsigned long long& sum,unsigned int& clipped,unsigned int& dark,
    unsigned long long& rs,unsigned long long& gs,unsigned long long& bs,unsigned int& count) {
#pragma HLS INLINE off
#pragma HLS DATAFLOW
    RawMat raw(rows,cols),corrected(rows,cols),shaded(rows,cols);
    ColorMat demosaiced(rows,cols);
    ColorImage balanced(rows,cols),colored(rows,cols);
    ByteImage quantized(rows,cols),mapped(rows,cols);
    RgbMat filtered(rows,cols),denoised(rows,cols),enhanced(rows,cols),roi(rh,rw),resized(oh,ow);
    unsigned short pattern=XF_BAYER_BG;
    read_raw(input,raw,black_level);
    correct_bad_pixels(raw,corrected,bpc_enable);
    lsc_mat_frame(corrected,shaded,lsc_enable,lsc_gains);
    xf::cv::demosaicing<XF_10UC1,XF_10UC3,MAX_ROWS,MAX_COLS,XF_NPPC1,false,2,2>(shaded,demosaiced,pattern);
    gain_color_stats(demosaiced,balanced,bg,gg,rg,rs,gs,bs,count);
    correct_color(balanced,colored,ccm_enable,matrix,offsets);
    quantize_color(colored,quantized);
    apply_gamma(quantized,mapped,gamma_enable,gamma);
    median_rgb(mapped,filtered,median_enable);
    bilateral_rgb(filtered,denoised,bilateral_enable,sigma_color,sigma_space_q8);
    sharpen_rgb(denoised,enhanced,sharpen_strength);
    crop_mat(enhanced,roi,rx,ry);
    area_downscale<MAX_COLS>(roi,resized);
    write_ai(resized,output,sum,clipped,dark);
}

void camera_color_pipeline_v5(hls::stream<RawPixel>& input, hls::stream<RgbPixel>& output,
    int rows, int cols, bool bpc_enable, unsigned short black_level,
    unsigned short blue_gain, unsigned short green_gain, unsigned short red_gain,
    bool median_enable, int roi_x, int roi_y, int roi_width, int roi_height,
    int out_rows, int out_cols, volatile unsigned long long& intensity_sum,
    volatile unsigned int& clipped_pixels, volatile unsigned int& dark_pixels,
    bool ccm_enable, bool gamma_enable, bool awb_enable,
    int matrix[3][3], int offsets[3], unsigned char gamma[768],
    volatile unsigned long long& red_sum, volatile unsigned long long& green_sum,
    volatile unsigned long long& blue_sum, volatile unsigned int& valid_pixels,
    volatile unsigned int& applied_red, volatile unsigned int& applied_green, volatile unsigned int& applied_blue,
    volatile unsigned int& frame_sequence,
    bool bilateral_enable,unsigned short sigma_color,unsigned short sigma_space_q8,unsigned short sharpen_strength,
    bool lsc_enable,unsigned short lsc_gains[4][7][9]) {
#pragma HLS INTERFACE axis port=input
#pragma HLS INTERFACE axis port=output
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=bpc_enable bundle=control
#pragma HLS INTERFACE s_axilite port=black_level bundle=control
#pragma HLS INTERFACE s_axilite port=blue_gain bundle=control
#pragma HLS INTERFACE s_axilite port=green_gain bundle=control
#pragma HLS INTERFACE s_axilite port=red_gain bundle=control
#pragma HLS INTERFACE s_axilite port=median_enable bundle=control
#pragma HLS INTERFACE s_axilite port=roi_x bundle=control
#pragma HLS INTERFACE s_axilite port=roi_y bundle=control
#pragma HLS INTERFACE s_axilite port=roi_width bundle=control
#pragma HLS INTERFACE s_axilite port=roi_height bundle=control
#pragma HLS INTERFACE s_axilite port=out_rows bundle=control
#pragma HLS INTERFACE s_axilite port=out_cols bundle=control
#pragma HLS INTERFACE s_axilite port=intensity_sum bundle=control
#pragma HLS INTERFACE s_axilite port=clipped_pixels bundle=control
#pragma HLS INTERFACE s_axilite port=dark_pixels bundle=control
#pragma HLS INTERFACE s_axilite port=ccm_enable bundle=control
#pragma HLS INTERFACE s_axilite port=gamma_enable bundle=control
#pragma HLS INTERFACE s_axilite port=awb_enable bundle=control
#pragma HLS INTERFACE s_axilite port=matrix bundle=control
#pragma HLS INTERFACE s_axilite port=offsets bundle=control
#pragma HLS INTERFACE s_axilite port=gamma bundle=control
#pragma HLS INTERFACE s_axilite port=red_sum bundle=control
#pragma HLS INTERFACE s_axilite port=green_sum bundle=control
#pragma HLS INTERFACE s_axilite port=blue_sum bundle=control
#pragma HLS INTERFACE s_axilite port=valid_pixels bundle=control
#pragma HLS INTERFACE s_axilite port=applied_red bundle=control
#pragma HLS INTERFACE s_axilite port=applied_green bundle=control
#pragma HLS INTERFACE s_axilite port=applied_blue bundle=control
#pragma HLS INTERFACE s_axilite port=frame_sequence bundle=control
#pragma HLS INTERFACE s_axilite port=bilateral_enable bundle=control
#pragma HLS INTERFACE s_axilite port=sigma_color bundle=control
#pragma HLS INTERFACE s_axilite port=sigma_space_q8 bundle=control
#pragma HLS INTERFACE s_axilite port=sharpen_strength bundle=control
#pragma HLS INTERFACE s_axilite port=lsc_enable bundle=control
#pragma HLS INTERFACE s_axilite port=lsc_gains bundle=control
    static unsigned short auto_red=256,auto_blue=256;
    static unsigned sequence=0;
    static bool previous_auto=false;
#pragma HLS RESET variable=auto_red
#pragma HLS RESET variable=auto_blue
#pragma HLS RESET variable=sequence
#pragma HLS RESET variable=previous_auto
    bool valid=rows>=8 && rows<=MAX_ROWS && !(rows&1) && cols>=16 && cols<=MAX_COLS && !(cols&15) &&
        black_level<=1023 && roi_x>=0 && roi_x<cols && roi_y>=0 && roi_y<rows &&
        roi_width>=1 && roi_width<=cols-roi_x && roi_height>=1 && roi_height<=rows-roi_y &&
        out_rows>=1 && out_rows<=roi_height && out_cols>=1 && out_cols<=roi_width;
    valid &= sharpen_strength<=256 && (!bilateral_enable || (sigma_color>=1 && sigma_color<=255 && sigma_space_q8>=64 && sigma_space_q8<=1024));
    if(ccm_enable)for(int i=0;i<3;++i) {
        valid&=offsets[i]>=-8388608 && offsets[i]<=8388607;
        for(int j=0;j<3;++j)valid&=matrix[i][j]>=-8388608 && matrix[i][j]<=8388607;
    }
    if(!valid) {
        intensity_sum=0;red_sum=0;green_sum=0;blue_sum=0;
        clipped_pixels=0;dark_pixels=0;valid_pixels=0;
        applied_red=0;applied_green=0;applied_blue=0;frame_sequence=0;return;
    }
    if(awb_enable && !previous_auto){auto_red=256;auto_blue=256;}
    unsigned short rg=awb_enable?auto_red:red_gain;
    unsigned short gg=awb_enable?256:green_gain;
    unsigned short bg=awb_enable?auto_blue:blue_gain;
    unsigned long long rs,gs,bs,output_sum;unsigned count,output_high,output_low;
    color_frame_v5(input,output,rows,cols,bpc_enable,black_level,bg,gg,rg,median_enable,
        roi_x,roi_y,roi_width,roi_height,out_rows,out_cols,ccm_enable,gamma_enable,matrix,offsets,gamma,
        bilateral_enable,sigma_color,sigma_space_q8,sharpen_strength,
        lsc_enable,lsc_gains,output_sum,output_high,output_low,rs,gs,bs,count);
    if(awb_enable && count>=64 && rs && gs && bs) {
        auto_red=next_awb_gain(gs,rs,rg);auto_blue=next_awb_gain(gs,bs,bg);
    }
    previous_auto=awb_enable;
    // Volatile publication gives AXI readers an odd/even consistency epoch.
    // The marker brackets every result write; it is stable/even between frames.
    frame_sequence=(++sequence<<1)|1U;
    intensity_sum=output_sum;clipped_pixels=output_high;dark_pixels=output_low;
    red_sum=rs;green_sum=gs;blue_sum=bs;valid_pixels=count;
    applied_red=rg;applied_green=gg;applied_blue=bg;
    frame_sequence=sequence<<1;
}

#include "raw_pipeline_v5.hpp"

static void raw_correct_frame_v5(hls::stream<RawPixel>& input,
                        hls::stream<RawPixel>& output,
                        int rows,int cols,bool bpc_enable,
                        unsigned short black_level,
                        unsigned short blue_gain,unsigned short green_gain,
                        unsigned short red_gain,bool lsc_enable,unsigned short lsc_gains[4][7][9]) {
#pragma HLS INLINE off
#pragma HLS DATAFLOW
    RawMat raw(rows,cols),corrected(rows,cols),shaded(rows,cols);
    read_raw(input,raw,black_level);
    correct_bad_pixels(raw,corrected,bpc_enable);
    lsc_mat_frame(corrected,shaded,lsc_enable,lsc_gains);
    write_bayer(shaded,output,blue_gain,green_gain,red_gain);
}
void camera_raw_correct_v5(hls::stream<RawPixel>& input,
                        hls::stream<RawPixel>& output,
                        int rows,int cols,bool bpc_enable,
                        unsigned short black_level,
                        unsigned short blue_gain,unsigned short green_gain,
                        unsigned short red_gain,bool lsc_enable,unsigned short lsc_gains[4][7][9]) {
#pragma HLS INTERFACE axis port=input
#pragma HLS INTERFACE axis port=output
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=bpc_enable bundle=control
#pragma HLS INTERFACE s_axilite port=black_level bundle=control
#pragma HLS INTERFACE s_axilite port=blue_gain bundle=control
#pragma HLS INTERFACE s_axilite port=green_gain bundle=control
#pragma HLS INTERFACE s_axilite port=red_gain bundle=control
#pragma HLS INTERFACE s_axilite port=lsc_enable bundle=control
#pragma HLS INTERFACE s_axilite port=lsc_gains bundle=control
    if(rows<8 || rows>MAX_ROWS || cols<16 || cols>MAX_COLS ||
       (rows&1) || (cols&15) || black_level>1023)return;
    raw_correct_frame_v5(input,output,rows,cols,bpc_enable,black_level,blue_gain,green_gain,red_gain,lsc_enable,lsc_gains);
}


// V9 color input is already corrected by the shared RAW v5 stage.
#include "color_pipeline_v9.hpp"

static void color_frame_v9(hls::stream<RawPixel>& input,hls::stream<RgbPixel>& output,
    int rows,int cols,
    unsigned short bg,unsigned short gg,unsigned short rg,bool median_enable,
    int rx,int ry,int rw,int rh,int oh,int ow,bool ccm_enable,bool gamma_enable,
    int matrix[3][3],int offsets[3],unsigned char gamma[768],
    bool bilateral_enable,unsigned short sigma_color,unsigned short sigma_space_q8,unsigned short sharpen_strength,
    unsigned long long& sum,unsigned int& clipped,unsigned int& dark,
    unsigned long long& rs,unsigned long long& gs,unsigned long long& bs,unsigned int& count) {
#pragma HLS INLINE off
#pragma HLS DATAFLOW
    RawMat raw(rows,cols);
    ColorMat demosaiced(rows,cols);
    ColorImage balanced(rows,cols),colored(rows,cols);
    ByteImage quantized(rows,cols),mapped(rows,cols);
    RgbMat filtered(rows,cols),denoised(rows,cols),enhanced(rows,cols),roi(rh,rw),resized(oh,ow);
    unsigned short pattern=XF_BAYER_BG;
    read_raw(input,raw,0);
    xf::cv::demosaicing<XF_10UC1,XF_10UC3,MAX_ROWS,MAX_COLS,XF_NPPC1,false,2,2>(raw,demosaiced,pattern);
    gain_color_stats(demosaiced,balanced,bg,gg,rg,rs,gs,bs,count);
    correct_color(balanced,colored,ccm_enable,matrix,offsets);
    quantize_color(colored,quantized);
    apply_gamma(quantized,mapped,gamma_enable,gamma);
    median_rgb(mapped,filtered,median_enable);
    bilateral_rgb(filtered,denoised,bilateral_enable,sigma_color,sigma_space_q8);
    sharpen_rgb(denoised,enhanced,sharpen_strength);
    crop_mat(enhanced,roi,rx,ry);
    area_downscale<MAX_COLS>(roi,resized);
    write_ai(resized,output,sum,clipped,dark);
}

void camera_color_pipeline_v9(hls::stream<RawPixel>& input, hls::stream<RgbPixel>& output,
    int rows, int cols,
    unsigned short blue_gain, unsigned short green_gain, unsigned short red_gain,
    bool median_enable, int roi_x, int roi_y, int roi_width, int roi_height,
    int out_rows, int out_cols, volatile unsigned long long& intensity_sum,
    volatile unsigned int& clipped_pixels, volatile unsigned int& dark_pixels,
    bool ccm_enable, bool gamma_enable, bool awb_enable,
    int matrix[3][3], int offsets[3], unsigned char gamma[768],
    volatile unsigned long long& red_sum, volatile unsigned long long& green_sum,
    volatile unsigned long long& blue_sum, volatile unsigned int& valid_pixels,
    volatile unsigned int& applied_red, volatile unsigned int& applied_green, volatile unsigned int& applied_blue,
    volatile unsigned int& frame_sequence,
    bool bilateral_enable,unsigned short sigma_color,unsigned short sigma_space_q8,unsigned short sharpen_strength) {
#pragma HLS INTERFACE axis port=input
#pragma HLS INTERFACE axis port=output
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=blue_gain bundle=control
#pragma HLS INTERFACE s_axilite port=green_gain bundle=control
#pragma HLS INTERFACE s_axilite port=red_gain bundle=control
#pragma HLS INTERFACE s_axilite port=median_enable bundle=control
#pragma HLS INTERFACE s_axilite port=roi_x bundle=control
#pragma HLS INTERFACE s_axilite port=roi_y bundle=control
#pragma HLS INTERFACE s_axilite port=roi_width bundle=control
#pragma HLS INTERFACE s_axilite port=roi_height bundle=control
#pragma HLS INTERFACE s_axilite port=out_rows bundle=control
#pragma HLS INTERFACE s_axilite port=out_cols bundle=control
#pragma HLS INTERFACE s_axilite port=intensity_sum bundle=control
#pragma HLS INTERFACE s_axilite port=clipped_pixels bundle=control
#pragma HLS INTERFACE s_axilite port=dark_pixels bundle=control
#pragma HLS INTERFACE s_axilite port=ccm_enable bundle=control
#pragma HLS INTERFACE s_axilite port=gamma_enable bundle=control
#pragma HLS INTERFACE s_axilite port=awb_enable bundle=control
#pragma HLS INTERFACE s_axilite port=matrix bundle=control
#pragma HLS INTERFACE s_axilite port=offsets bundle=control
#pragma HLS INTERFACE s_axilite port=gamma bundle=control
#pragma HLS INTERFACE s_axilite port=red_sum bundle=control
#pragma HLS INTERFACE s_axilite port=green_sum bundle=control
#pragma HLS INTERFACE s_axilite port=blue_sum bundle=control
#pragma HLS INTERFACE s_axilite port=valid_pixels bundle=control
#pragma HLS INTERFACE s_axilite port=applied_red bundle=control
#pragma HLS INTERFACE s_axilite port=applied_green bundle=control
#pragma HLS INTERFACE s_axilite port=applied_blue bundle=control
#pragma HLS INTERFACE s_axilite port=frame_sequence bundle=control
#pragma HLS INTERFACE s_axilite port=bilateral_enable bundle=control
#pragma HLS INTERFACE s_axilite port=sigma_color bundle=control
#pragma HLS INTERFACE s_axilite port=sigma_space_q8 bundle=control
#pragma HLS INTERFACE s_axilite port=sharpen_strength bundle=control
    static unsigned short auto_red=256,auto_blue=256;
    static unsigned sequence=0;
    static bool previous_auto=false;
#pragma HLS RESET variable=auto_red
#pragma HLS RESET variable=auto_blue
#pragma HLS RESET variable=sequence
#pragma HLS RESET variable=previous_auto
    bool valid=rows>=8 && rows<=MAX_ROWS && !(rows&1) && cols>=16 && cols<=MAX_COLS && !(cols&15) &&
        roi_x>=0 && roi_x<cols && roi_y>=0 && roi_y<rows &&
        roi_width>=1 && roi_width<=cols-roi_x && roi_height>=1 && roi_height<=rows-roi_y &&
        out_rows>=1 && out_rows<=roi_height && out_cols>=1 && out_cols<=roi_width;
    valid &= sharpen_strength<=256 && (!bilateral_enable || (sigma_color>=1 && sigma_color<=255 && sigma_space_q8>=64 && sigma_space_q8<=1024));
    if(ccm_enable)for(int i=0;i<3;++i) {
        valid&=offsets[i]>=-8388608 && offsets[i]<=8388607;
        for(int j=0;j<3;++j)valid&=matrix[i][j]>=-8388608 && matrix[i][j]<=8388607;
    }
    if(!valid) {
        intensity_sum=0;red_sum=0;green_sum=0;blue_sum=0;
        clipped_pixels=0;dark_pixels=0;valid_pixels=0;
        applied_red=0;applied_green=0;applied_blue=0;frame_sequence=0;return;
    }
    if(awb_enable && !previous_auto){auto_red=256;auto_blue=256;}
    unsigned short rg=awb_enable?auto_red:red_gain;
    unsigned short gg=awb_enable?256:green_gain;
    unsigned short bg=awb_enable?auto_blue:blue_gain;
    unsigned long long rs,gs,bs,output_sum;unsigned count,output_high,output_low;
    color_frame_v9(input,output,rows,cols,bg,gg,rg,median_enable,
        roi_x,roi_y,roi_width,roi_height,out_rows,out_cols,ccm_enable,gamma_enable,matrix,offsets,gamma,
        bilateral_enable,sigma_color,sigma_space_q8,sharpen_strength,
        output_sum,output_high,output_low,rs,gs,bs,count);
    if(awb_enable && count>=64 && rs && gs && bs) {
        auto_red=next_awb_gain(gs,rs,rg);auto_blue=next_awb_gain(gs,bs,bg);
    }
    previous_auto=awb_enable;
    // Volatile publication gives AXI readers an odd/even consistency epoch.
    // The marker brackets every result write; it is stable/even between frames.
    frame_sequence=(++sequence<<1)|1U;
    intensity_sum=output_sum;clipped_pixels=output_high;dark_pixels=output_low;
    red_sum=rs;green_sum=gs;blue_sum=bs;valid_pixels=count;
    applied_red=rg;applied_green=gg;applied_blue=bg;
    frame_sequence=sequence<<1;
}

