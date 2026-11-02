#include "rgb_output.hpp"
#include "common/xf_common.hpp"
#include "imgproc/xf_cvt_color.hpp"
using OutputRgb=xf::cv::Mat<XF_8UC3,MAX_ROWS,MAX_COLS,XF_NPPC1,2>;
using OutputPlane=xf::cv::Mat<XF_8UC1,MAX_ROWS,MAX_COLS,XF_NPPC1,2>;

static void read_rgb(hls::stream<RgbPixel>& input,OutputRgb& image) {
    for(int i=0;i<image.rows*image.cols;++i) {
#pragma HLS PIPELINE II=1
        image.write(i,input.read().data);
    }
}
static unsigned clip(int value) {return value<0?0:value>255?255:unsigned(value);}
static void planes(OutputRgb& rgb,OutputPlane& y,OutputPlane& u,OutputPlane& v,bool full) {
    if(!full) {
        xf::cv::rgb2yuv4<XF_8UC3,XF_8UC1,MAX_ROWS,MAX_COLS,XF_NPPC1,2,2,2,2>(rgb,y,u,v);
    } else {
        for(int i=0;i<rgb.rows*rgb.cols;++i) {
#pragma HLS PIPELINE II=1
            ap_uint<24> p=rgb.read(i);
            int r=p.range(7,0),g=p.range(15,8),b=p.range(23,16);
            y.write(i,clip((19595*r+38470*g+7471*b+32768)>>16));
            u.write(i,clip((-11059*r-21709*g+32768*b+8421376)>>16));
            v.write(i,clip((32768*r-27439*g-5329*b+8421376)>>16));
        }
    }
}
static RgbPixel word(unsigned data,unsigned keep,bool sof,bool last) {
#pragma HLS INLINE
    RgbPixel p;p.data=data;p.keep=keep;p.strb=keep;p.user=sof;p.last=last;p.id=0;p.dest=0;return p;
}
static void write_444(OutputPlane& y,OutputPlane& u,OutputPlane& v,hls::stream<RgbPixel>& output) {
    for(int row=0;row<y.rows;++row)for(int col=0;col<y.cols;++col) {
#pragma HLS PIPELINE II=1
        int i=row*y.cols+col;
        unsigned yy=y.read(i),uu=u.read(i),vv=v.read(i);
        output.write(word(uu|(yy<<8)|(vv<<16),7,!row&&!col,col==y.cols-1));
    }
}
static void write_422(OutputPlane& y,OutputPlane& u,OutputPlane& v,hls::stream<RgbPixel>& output,unsigned format) {
    for(int row=0;row<y.rows;++row)for(int col=0;col<y.cols;col+=2) {
#pragma HLS PIPELINE II=2
        int i=row*y.cols+col;
        unsigned y0=y.read(i),u0=u.read(i),v0=v.read(i);
        unsigned y1=y.read(i+1),u1=u.read(i+1),v1=v.read(i+1);
        unsigned uu=(u0+u1+1)>>1,vv=(v0+v1+1)>>1;
        bool reversed=format==OUTPUT_YVYU || format==OUTPUT_VYUY;
        unsigned c0=reversed?vv:uu,c1=reversed?uu:vv;
        bool chroma_first=format==OUTPUT_UYVY || format==OUTPUT_VYUY;
        unsigned a=chroma_first?c0|(y0<<8):y0|(c0<<8);
        unsigned b=chroma_first?c1|(y1<<8):y1|(c1<<8);
        output.write(word(a,3,!row&&!col,false));
        output.write(word(b,3,false,col==y.cols-2));
    }
}
static void write_420(OutputPlane& y,OutputPlane& u,OutputPlane& v,hls::stream<RgbPixel>& output) {
    ap_uint<8> even_y[MAX_COLS],odd_y[MAX_COLS],odd_v[MAX_COLS/2];
    ap_uint<18> even_uv[MAX_COLS/2];
#pragma HLS ARRAY_PARTITION variable=even_y cyclic factor=2
#pragma HLS ARRAY_PARTITION variable=odd_y cyclic factor=2
#pragma HLS BIND_STORAGE variable=even_y type=ram_2p impl=bram
#pragma HLS BIND_STORAGE variable=odd_y type=ram_2p impl=bram
#pragma HLS BIND_STORAGE variable=even_uv type=ram_2p impl=bram
#pragma HLS BIND_STORAGE variable=odd_v type=ram_2p impl=bram
    for(int row=0;row<y.rows;++row) {
        for(int col=0;col<y.cols;col+=2) {
#pragma HLS PIPELINE II=2
            int i=row*y.cols+col;
            unsigned y0=y.read(i),u0=u.read(i),v0=v.read(i);
            unsigned y1=y.read(i+1),u1=u.read(i+1),v1=v.read(i+1);
            if(!(row&1)) {
                even_y[col]=y0;even_y[col+1]=y1;
                even_uv[col/2]=(u0+u1)|((v0+v1)<<9);
            } else {
                ap_uint<18> previous=even_uv[col/2];
                unsigned uu=(unsigned(previous.range(8,0))+u0+u1+2)>>2;
                unsigned vv=(unsigned(previous.range(17,9))+v0+v1+2)>>2;
                odd_y[col]=y0;odd_y[col+1]=y1;odd_v[col/2]=vv;
                unsigned a=even_y[col],b=even_y[col+1];
                output.write(word(uu|(a<<8)|(b<<16),7,row==1&&!col,col==y.cols-2));
            }
        }
        if(row&1)for(int col=0;col<y.cols;col+=2) {
#pragma HLS PIPELINE II=1
            unsigned a=odd_y[col],b=odd_y[col+1],vv=odd_v[col/2];
            output.write(word(vv|(a<<8)|(b<<16),7,false,col==y.cols-2));
        }
    }
}
static void write_planes(OutputPlane& y,OutputPlane& u,OutputPlane& v,
                          hls::stream<RgbPixel>& output,unsigned format) {
#pragma HLS INLINE off
    if(format==OUTPUT_UYV444)write_444(y,u,v,output);
    else if(format==OUTPUT_YUV420)write_420(y,u,v,output);
    else write_422(y,u,v,output,format);
}
static void converted(hls::stream<RgbPixel>& input,hls::stream<RgbPixel>& output,
                       int rows,int cols,unsigned format,bool full) {
#pragma HLS INLINE off
#pragma HLS DATAFLOW
    OutputRgb rgb(rows,cols);OutputPlane y(rows,cols),u(rows,cols),v(rows,cols);
    read_rgb(input,rgb);planes(rgb,y,u,v,full);
    write_planes(y,u,v,output,format);
}
void camera_rgb_output(hls::stream<RgbPixel>& input,hls::stream<RgbPixel>& output,
                        int rows,int cols,unsigned format,bool full_range) {
#pragma HLS INTERFACE axis port=input
#pragma HLS INTERFACE axis port=output
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=format bundle=control
#pragma HLS INTERFACE s_axilite port=full_range bundle=control
    if(rows<1 || rows>MAX_ROWS || cols<1 || cols>MAX_COLS || format>OUTPUT_YUV420 ||
       (format>=OUTPUT_YUYV && format<=OUTPUT_VYUY && (cols&1)) ||
       (format==OUTPUT_YUV420 && ((cols&1)||(rows&1))))return;
    if(format==OUTPUT_RGB) {
        for(int y=0;y<rows;++y)for(int x=0;x<cols;++x) {
#pragma HLS PIPELINE II=1
            output.write(word(input.read().data,7,!x&&!y,x==cols-1));
        }
    } else converted(input,output,rows,cols,format,full_range);
}
