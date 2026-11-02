#include "probe.hpp"
#include "imgproc/xf_gtm.hpp"
using Wide=xf::cv::Mat<XF_16UC3,MAX_ROWS,MAX_COLS,XF_NPPC1,2>;
using Narrow=xf::cv::Mat<XF_8UC3,MAX_ROWS,MAX_COLS,XF_NPPC1,2>;
static void unpack(hls::stream<ToneInput>& input,Wide& image) {
    for(int i=0;i<image.rows*image.cols;++i) {
#pragma HLS PIPELINE II=1
        image.write(i,input.read().data);
    }
}
static void pack(Narrow& image,hls::stream<RgbPixel>& output) {
    for(int y=0;y<image.rows;++y)for(int x=0;x<image.cols;++x) {
#pragma HLS PIPELINE II=1
        RgbPixel p;p.data=image.read(y*image.cols+x);p.keep=7;p.strb=7;
        p.user=!x&&!y;p.last=x==image.cols-1;p.id=0;p.dest=0;output.write(p);
    }
}
static void transform(Wide& source,Narrow& destination,unsigned c1,unsigned c2,
                      unsigned& mean,unsigned& maximum,unsigned& minimum) {
    // Fixed preceding-frame statistics isolate the native core resource cost.
    // A production design must define temporal history and degenerate ranges.
    ap_ufixed<16,4> mean_write=0,max_write=0,min_write=15;
    ap_ufixed<16,4> mean_read=2,max_read=4,min_read=0;
    xf::cv::gtm<XF_16UC3,XF_8UC3,XF_16UC1,XF_8UC1,MAX_ROWS,MAX_COLS,XF_NPPC1,2,2>(
        source,destination,mean_write,mean_read,max_write,max_read,min_write,min_read,c1,c2);
    mean=mean_write.range(15,0);maximum=max_write.range(15,0);minimum=min_write.range(15,0);
}
void camera_gtm_probe(hls::stream<ToneInput>& input,hls::stream<RgbPixel>& output,
                      int rows,int cols,unsigned c1,unsigned c2,
                      unsigned& mean,unsigned& maximum,unsigned& minimum) {
#pragma HLS INTERFACE axis port=input
#pragma HLS INTERFACE axis port=output
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=c1 bundle=control
#pragma HLS INTERFACE s_axilite port=c2 bundle=control
#pragma HLS INTERFACE s_axilite port=mean bundle=control
#pragma HLS INTERFACE s_axilite port=maximum bundle=control
#pragma HLS INTERFACE s_axilite port=minimum bundle=control
#pragma HLS DATAFLOW
    Wide source(rows,cols);Narrow destination(rows,cols);
    unpack(input,source);transform(source,destination,c1,c2,mean,maximum,minimum);pack(destination,output);
}
