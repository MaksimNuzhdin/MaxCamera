#include "local_contrast.hpp"
#include "clahe_bounded.hpp"
using Gray=xf::cv::Mat<XF_8UC1,MAX_ROWS,MAX_COLS,XF_NPPC1,2>;
using Engine=xf::cv::bounded_clahe::CLAHEImpl<XF_8UC1,MAX_ROWS,MAX_COLS,XF_NPPC1,8,4,4,4,4,2,2>;
using Lut=ap_uint<Engine::HIST_COUNTER_BITS>[4][4][2][256];
using Clips=ap_uint<Engine::CLIP_COUNTER_BITS>[4][4];
static const unsigned reciprocal[256]={0,65536,32768,21845,16384,13107,10923,9362,8192,7282,6554,5958,5461,5041,4681,4369,4096,3855,3641,3449,3277,3121,2979,2849,2731,2621,2521,2427,2341,2260,2185,2114,2048,1986,1928,1872,1820,1771,1725,1680,1638,1598,1560,1524,1489,1456,1425,1394,1365,1337,1311,1285,1260,1237,1214,1192,1170,1150,1130,1111,1092,1074,1057,1040,1024,1008,993,978,964,950,936,923,910,898,886,874,862,851,840,830,819,809,799,790,780,771,762,753,745,736,728,720,712,705,697,690,683,676,669,662,655,649,643,636,630,624,618,612,607,601,596,590,585,580,575,570,565,560,555,551,546,542,537,533,529,524,520,516,512,508,504,500,496,493,489,485,482,478,475,471,468,465,462,458,455,452,449,446,443,440,437,434,431,428,426,423,420,417,415,412,410,407,405,402,400,397,395,392,390,388,386,383,381,379,377,374,372,370,368,366,364,362,360,358,356,354,352,350,349,347,345,343,341,340,338,336,334,333,331,329,328,326,324,323,321,320,318,317,315,314,312,311,309,308,306,305,303,302,301,299,298,297,295,294,293,291,290,289,287,286,285,284,282,281,280,279,278,277,275,274,273,272,271,270,269,267,266,265,264,263,262,261,260,259,258,257};

static void split(hls::stream<RgbPixel>& input,Gray& value,hls::stream<ap_uint<24>>& rgb) {
    for(int i=0;i<value.rows*value.cols;++i) {
#pragma HLS PIPELINE II=1
        ap_uint<24> p=input.read().data;
        unsigned r=p.range(7,0),g=p.range(15,8),b=p.range(23,16);
        unsigned v=r>g?r:g;v=v>b?v:b;value.write(i,v);rgb.write(p);
    }
}
static void join(hls::stream<ap_uint<24>>& rgb,Gray& mapped,hls::stream<RgbPixel>& output,bool active) {
    for(int y=0;y<mapped.rows;++y)for(int x=0;x<mapped.cols;++x) {
#pragma HLS PIPELINE II=1
        ap_uint<24> original=rgb.read(),result=original;
        unsigned r=original.range(7,0),g=original.range(15,8),b=original.range(23,16);
        unsigned v=r>g?r:g;v=v>b?v:b;
        ap_uint<25> gain=ap_uint<25>(mapped.read(y*mapped.cols+x))*reciprocal[v];
        if(active)for(int c=0;c<3;++c) {
#pragma HLS UNROLL
            ap_uint<8> channel=original.range(c*8+7,c*8);
            ap_uint<34> product=channel*gain;
            unsigned scaled=(product+32768)>>16;
            result.range(c*8+7,c*8)=scaled>255?255:scaled;
        }
        RgbPixel p;p.data=result;p.keep=7;p.strb=7;p.user=!x&&!y;
        p.last=x==mapped.cols-1;p.id=0;p.dest=0;output.write(p);
    }
}
static void frame(hls::stream<RgbPixel>& input,hls::stream<RgbPixel>& output,
    int rows,int cols,unsigned clip,Lut& write_lut,Lut& read_lut,Clips& counts,bool active) {
#pragma HLS INLINE off
#pragma HLS DATAFLOW
    Gray value(rows,cols),mapped(rows,cols);
    hls::stream<ap_uint<24>> rgb;
#pragma HLS STREAM variable=rgb depth=128
    Engine engine;
    split(input,value,rgb);
    engine.process(mapped,value,write_lut,read_lut,counts,rows,cols,clip,4,4);
    join(rgb,mapped,output,active);
}
void camera_local_contrast(hls::stream<RgbPixel>& input,hls::stream<RgbPixel>& output,
    int rows,int cols,bool enable,unsigned char clip,unsigned epoch) {
#pragma HLS INTERFACE axis port=input
#pragma HLS INTERFACE axis port=output
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=enable bundle=control
#pragma HLS INTERFACE s_axilite port=clip bundle=control
#pragma HLS INTERFACE s_axilite port=epoch bundle=control
    static Lut lut0,lut1;
#pragma HLS ARRAY_PARTITION variable=lut0 complete dim=3
#pragma HLS ARRAY_PARTITION variable=lut1 complete dim=3
#pragma HLS BIND_STORAGE variable=lut0 type=ram_t2p impl=bram
#pragma HLS BIND_STORAGE variable=lut1 type=ram_t2p impl=bram
    static Clips counts;
    static bool valid=false,bank=false;
    static unsigned previous_epoch=0,previous_clip=0;
    static int previous_rows=0,previous_cols=0;
#pragma HLS RESET variable=valid
#pragma HLS RESET variable=bank
#pragma HLS RESET variable=previous_epoch
#pragma HLS RESET variable=previous_clip
#pragma HLS RESET variable=previous_rows
#pragma HLS RESET variable=previous_cols
    if(rows<8 || rows>MAX_ROWS || cols<8 || cols>MAX_COLS)return;
    if(!enable) {
        valid=false;
        ap_uint<23> pixels=ap_uint<11>(rows)*ap_uint<12>(cols);
        for(int i=0;i<pixels;++i) {
#pragma HLS PIPELINE II=1
            output.write(input.read());
        }
        return;
    }
    if(rows<32 || cols<32 || rows%4 || cols%8 || clip<1 || clip>8)return;
    if(epoch!=previous_epoch || clip!=previous_clip || rows!=previous_rows || cols!=previous_cols)valid=false;
    if(!valid) {
        bank=false;
        for(int y=0;y<4;++y)for(int x=0;x<4;++x)for(int v=0;v<256;++v) {
#pragma HLS PIPELINE II=1
            for(int c=0;c<2;++c) {
#pragma HLS UNROLL
                lut0[y][x][c][v]=v;lut1[y][x][c][v]=v;
            }
        }
    }
    if(bank)frame(input,output,rows,cols,clip,lut0,lut1,counts,valid);
    else frame(input,output,rows,cols,clip,lut1,lut0,counts,valid);
    bank=!bank;valid=true;previous_epoch=epoch;previous_clip=clip;previous_rows=rows;previous_cols=cols;
}
