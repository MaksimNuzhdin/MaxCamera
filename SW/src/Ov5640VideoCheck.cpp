#include "Ov5640VideoCheck.h"
#if MAX_CAMERA_VIDEO_CHECK
#include "AxiIicPolled.h"
#include "AxiGpioSccb.h"
#include "Ov5640.h"
#include "sleep.h"
#include "xil_cache.h"
#include "xil_io.h"
#include "xil_printf.h"

#include "xcsi_hw.h"
#include "xparameters.h"
#include "xv_csc_l2.h"
#include "xv_frmbufwr_l2.h"
#include <string.h>
#ifndef MAX_CAMERA_VIDEO_CSI_BYTES
#define MAX_CAMERA_VIDEO_CSI_BYTES 0
#endif
#ifndef MAX_CAMERA_VIDEO_RGB555
#define MAX_CAMERA_VIDEO_RGB555 0
#endif
#ifndef MAX_CAMERA_VIDEO_RGB444
#define MAX_CAMERA_VIDEO_RGB444 0
#endif
#ifndef MAX_CAMERA_VIDEO_YUV444
#define MAX_CAMERA_VIDEO_YUV444 0
#endif
#ifndef MAX_CAMERA_VIDEO_YUV420
#define MAX_CAMERA_VIDEO_YUV420 0
#endif
#ifndef MAX_CAMERA_VIDEO_JPEG
#define MAX_CAMERA_VIDEO_JPEG 0
#endif
#ifndef MAX_CAMERA_VIDEO_YUV422_BYTES
#define MAX_CAMERA_VIDEO_YUV422_BYTES 0
#endif
#ifndef MAX_CAMERA_VIDEO_IIC_HZ
#define MAX_CAMERA_VIDEO_IIC_HZ 50000
#endif
#ifndef MAX_CAMERA_VIDEO_PREP_TESTS
#define MAX_CAMERA_VIDEO_PREP_TESTS 1
#endif
#ifndef MAX_CAMERA_VIDEO_FULL_RAW
#define MAX_CAMERA_VIDEO_FULL_RAW 0
#endif
#ifndef MAX_CAMERA_VIDEO_RGB565
#define MAX_CAMERA_VIDEO_RGB565 0
#endif
#ifndef MAX_CAMERA_VIDEO_RGB888
#define MAX_CAMERA_VIDEO_RGB888 0
#endif
#ifndef MAX_CAMERA_VIDEO_RGB888_CSC
#define MAX_CAMERA_VIDEO_RGB888_CSC 0
#endif
#ifndef MAX_CAMERA_VIDEO_RAW10
#define MAX_CAMERA_VIDEO_RAW10 0
#endif
/* CAM0/J23 is physically connected to CSI-2 receiver 1, not receiver 0.
 * A linker-owned, cache-line-aligned buffer is the only DMA destination. */
static constexpr unsigned video_width=MAX_CAMERA_VIDEO_FULL_RAW ? 2592 : 640;
static constexpr unsigned video_height=MAX_CAMERA_VIDEO_FULL_RAW ? 1944 : 480;
#if MAX_CAMERA_VIDEO_RGB888 || MAX_CAMERA_VIDEO_YUV444
static constexpr unsigned video_stride=1920;
#elif MAX_CAMERA_VIDEO_YUV420
static constexpr unsigned video_stride=960;
#elif MAX_CAMERA_VIDEO_RGB565 || MAX_CAMERA_VIDEO_RGB555 || MAX_CAMERA_VIDEO_RGB444 || MAX_CAMERA_VIDEO_JPEG || MAX_CAMERA_VIDEO_YUV422_BYTES
static constexpr unsigned video_stride=1280;
#elif MAX_CAMERA_VIDEO_RAW10
static constexpr unsigned video_stride=MAX_CAMERA_VIDEO_CSI_BYTES ? ((video_width*5/4+15)&~15U) : 864;
#elif MAX_CAMERA_VIDEO_Y8
static constexpr unsigned video_stride=video_width;
#else
static constexpr unsigned video_stride=1280;
#endif
extern "C" {
unsigned char g_video_frame[video_stride*video_height] __attribute__((aligned(64)));
/* JTAG-readable evidence when UART is unavailable. stage: 1 init, 2 receiver,
 * 3 capture, 4 complete, 0xffffffff failed. Values are published before stage. */
volatile struct {
    uint32_t stage, bytes, frames, dma_done, errors, elapsed_us, coherent;
    uint32_t changed, hash, receiver_type, formatter, mux;
    int32_t result;
    uint32_t sensor_error_register, sensor_state, receiver_line_info;
} g_video_result __attribute__((aligned(64)));
}
static void publish_stage(uint32_t stage)
{
    g_video_result.stage=stage;
    Xil_DCacheFlushRange((INTPTR)&g_video_result,sizeof(g_video_result));
}
#if MAX_CAMERA_VIDEO_GPIO_SCCB
static AXI_GPIO_SCCB bus;
static int transport_init() {return AxiGpioSccb_Init(&bus,0xa00d0000U,MAX_CAMERA_VIDEO_IIC_HZ);}
#define transport_read AxiGpioSccb_Read
#define transport_write AxiGpioSccb_Write
#else
static AXI_IIC_POLLED bus;
static int transport_init() {return AxiIicPolled_Init(&bus,0xa0020000U,MAX_CAMERA_VIDEO_IIC_HZ,200);}
#define transport_read AxiIicPolled_Read
#define transport_write AxiIicPolled_Write
#endif
static OV5640 sensor;
#if !MAX_CAMERA_VIDEO_Y8 || MAX_CAMERA_VIDEO_RGB888_CSC
static XV_Csc_l2 csc;
#endif
static XV_FrmbufWr_l2 writer;
static const uint32_t rx=XPAR_MIPI_CSI2_RX_SUBSYST_1_BASEADDR, csc_base=0xa00a0000U, dma=0xa0080000U;
static void delay(void *,uint32_t us) {usleep(us);}
static int sensor_read(void *u,uint8_t addr,uint16_t reg,uint8_t *data,uint16_t count)
{
    int e=transport_read(u,addr,reg,data,count);
    if(e) xil_printf("VIDEO I2C READ reg=%04x status=%d sr=%08x irq=%08x phase=%d\r\n",reg,e,bus.status,bus.interrupts,bus.phase);
    return e;
}
static int sensor_write(void *u,uint8_t addr,uint16_t reg,const uint8_t *data,uint16_t count)
{
    int e=transport_write(u,addr,reg,data,count);
    if(e) xil_printf("VIDEO I2C WRITE reg=%04x value=%02x status=%d sr=%08x irq=%08x phase=%d\r\n",reg,data[0],e,bus.status,bus.interrupts,bus.phase);
#if MAX_CAMERA_VIDEO_CAMERA_POWER
    /* Conservative transition settling for the isolated PWUP experiment;
     * no command is replayed and the driver still verifies state and ID. */
    if(reg==0x3008)usleep(100000);
#endif
    return e;
}
static uint32_t read(uint32_t base,uint32_t off) {return Xil_In32(base+off);}
static void write(uint32_t base,uint32_t off,uint32_t value) {Xil_Out32(base+off,value);}
static void snapshot(const char *tag)
{
    xil_printf("VIDEO %s CCR=%08x PCR=%08x CSR=%08x ISR=%08x CLK=%08x L0=%08x L1=%08x VC0=%08x DT=%08x CSC=%08x DMA=%08x\r\n",tag,
        read(rx,0),read(rx,4),read(rx,0x10),read(rx,0x24),read(rx,0x3c),read(rx,0x40),read(rx,0x44),read(rx,0x60),read(rx,0x64),read(csc_base,0),read(dma,0));
}
static int sensor_status(const char *tag)
{
    const uint16_t regs[]={0x3008,0x3034,0x3035,0x3036,0x3037,0x3108,0x3103,0x3800,0x3801,0x3802,0x3803,0x3804,0x3805,0x3806,0x3807,0x3808,0x3809,0x380a,0x380b,0x380c,0x380d,0x380e,0x380f,0x3814,0x3815,0x3820,0x3821,0x3400,0x3401,0x3402,0x3403,0x3404,0x3405,0x3406,0x3500,0x3501,0x3502,0x350a,0x350b,0x350c,0x350d,0x3503,0x4837,0x4300,0x501f,0x5000,0x5001,0x503d,0x4814,0x4800};
    for(unsigned i=0;i<sizeof(regs)/sizeof(regs[0]);++i) {
        uint8_t value=0;int e=Ov5640_Read(&sensor,regs[i],&value,1);
        xil_printf("VIDEO SENSOR %s %04x=%02x status=%d\r\n",tag,regs[i],value,e);
        if(e)return e;
    }
    return 0;
}
static uint32_t csi_errors(uint32_t irq)
{
    return irq&(XCSI_INTR_PROT_MASK|XCSI_INTR_PKTLVL_MASK|XCSI_INTR_DPHY_MASK|
                XCSI_ISR_SLBF_MASK|XCSI_ISR_WC_MASK|XCSI_ISR_ILC_MASK);
}
static int prepare_receiver()
{
#if MAX_CAMERA_VIDEO_RESET
    /* Matching source-level CSI-byte reset image only; GPIO3 is reserved by
     * status LED mode. The earlier post-route reset ECO is ineffective. */
    uint32_t leds=read(0xa0010000U,0);
    write(0xa0010000U,0,leds&~8U);usleep(10);
    write(0xa0010000U,0,leds|8U);usleep(1000);
#endif
    if(read(dma,0)&0x20) {
        xil_printf("VIDEO frame writer is flushed: full FPGA reset required\r\n");
        return -8;
    }
    write(rx,XCSI_GIER_OFFSET,0);
    write(rx,XCSI_IER_OFFSET,0);
    write(rx,XCSI_CCR_OFFSET,XCSI_CCR_SOFTRESET_MASK);
    unsigned wait;
    for(wait=0;wait<10000;++wait) {
        if(!(read(rx,XCSI_CSR_OFFSET)&XCSI_CSR_RIPCD_MASK))break;
        usleep(10);
    }
    if(wait==10000) {xil_printf("VIDEO receiver reset timeout\r\n");return -1;}
    write(rx,XCSI_CCR_OFFSET,0);
    write(rx,XCSI_ISR_OFFSET,read(rx,XCSI_ISR_OFFSET));
    write(rx,XCSI_CCR_OFFSET,XCSI_CCR_COREENB_MASK);
#if !MAX_CAMERA_VIDEO_Y8 || MAX_CAMERA_VIDEO_RGB888_CSC
    XV_csc_Config *cfg=XV_csc_LookupConfig(0);
    if(!cfg || XV_csc_CfgInitialize(&csc.Csc,cfg,csc_base)) return -2;
    XV_CscSetPowerOnDefaultState(&csc);
    XV_CscSetActiveSize(&csc,640,480);
#if MAX_CAMERA_VIDEO_RGB888_CSC
    if(XV_CscSetColorspace(&csc,XVIDC_CSF_YCRCB_444,XVIDC_CSF_RGB,
                           XVIDC_BT_601,XVIDC_BT_601,XVIDC_CR_0_255))return -3;
#else
    if(XV_CscSetColorspace(&csc,XVIDC_CSF_YCRCB_422,XVIDC_CSF_YCRCB_422,
                           XVIDC_BT_601,XVIDC_BT_601,XVIDC_CR_16_235))return -3;
#endif
    XVidC_VideoWindow window={0,0,640,480};
    if(XV_CscSetDemoWindow(&csc,&window))return -4;
#endif
    if(XVFrmbufWr_Initialize(&writer,dma)) return -5;
#if MAX_CAMERA_VIDEO_FULL_RAW
    /* Matching full-width byte-writer image only; keep BSP tables unchanged. */
    writer.FrmbufWr.Config.MaxWidth=8192;
    writer.FrmbufWr.Config.MaxHeight=1944;
#endif
    XVidC_VideoStream stream={};
#if MAX_CAMERA_VIDEO_Y8
    /* Only for the dedicated native-y8 image. Do not alter shared BSP tables. */
#if MAX_CAMERA_VIDEO_CSI_BYTES
    writer.FrmbufWr.Config.Y8En=1;
#elif MAX_CAMERA_VIDEO_RGB888
    writer.FrmbufWr.Config.RGB8En=1;
#elif MAX_CAMERA_VIDEO_RAW10
    writer.FrmbufWr.Config.Y10En=1;
    writer.FrmbufWr.Config.MaxDataWidth=10;
#else
    writer.FrmbufWr.Config.Y8En=1;
#endif
    /* BSP calls Y8 "YONLY" while this IP's HLS MEMORY2LIVE table maps it
     * to YUV444 lanes. Use the driver's metadata; hardware remap stays 444. */
    stream.ColorFormatId=MAX_CAMERA_VIDEO_RGB888 && !MAX_CAMERA_VIDEO_CSI_BYTES ? XVIDC_CSF_RGB : XVIDC_CSF_YONLY;
#else
    stream.ColorFormatId=XVIDC_CSF_YCRCB_422;
#endif
#if MAX_CAMERA_VIDEO_RAW10 && !MAX_CAMERA_VIDEO_CSI_BYTES
    stream.ColorDepth=XVIDC_BPC_10;
#else
    stream.ColorDepth=XVIDC_BPC_8;
#endif
    stream.PixPerClk=XVIDC_PPC_2;
    stream.FrameRate=XVIDC_FR_30HZ;
    stream.Timing.HActive=MAX_CAMERA_VIDEO_CSI_BYTES ? (MAX_CAMERA_VIDEO_RAW10 ? video_width*5/4 : video_stride) : video_width;
    stream.Timing.VActive=video_height;
#if MAX_CAMERA_VIDEO_CSI_BYTES
    if(XVFrmbufWr_SetMemFormat(&writer,video_stride,XVIDC_CSF_MEM_Y8,&stream)) return -6;
#elif MAX_CAMERA_VIDEO_RGB888
    if(XVFrmbufWr_SetMemFormat(&writer,1920,XVIDC_CSF_MEM_RGB8,&stream)) return -6;
#elif MAX_CAMERA_VIDEO_RAW10
    if(XVFrmbufWr_SetMemFormat(&writer,864,XVIDC_CSF_MEM_Y10,&stream)) return -6;
#elif MAX_CAMERA_VIDEO_Y8
    if(XVFrmbufWr_SetMemFormat(&writer,640,XVIDC_CSF_MEM_Y8,&stream)) return -6;
#else
    if(XVFrmbufWr_SetMemFormat(&writer,1280,XVIDC_CSF_MEM_YUYV8,&stream)) return -6;
#endif
    if(XVFrmbufWr_SetBufferAddr(&writer,(UINTPTR)g_video_frame))return -7;
    return 0;
}
static void capture(const char *tag)
{
    memset(g_video_frame,0xa5,sizeof(g_video_frame));
    Xil_DCacheFlushRange((INTPTR)g_video_frame,sizeof(g_video_frame));
    write(rx,XCSI_ISR_OFFSET,read(rx,XCSI_ISR_OFFSET));
    /* HLS interrupt status is toggle-on-write; poll done with GIE disabled. */
    write(dma,4,0);write(dma,8,1);write(dma,0xc,read(dma,0xc));
    XV_frmbufwr_EnableAutoRestart(&writer.FrmbufWr);
    XVFrmbufWr_Start(&writer);
#if !MAX_CAMERA_VIDEO_Y8 || MAX_CAMERA_VIDEO_RGB888_CSC
    XV_CscStart(&csc);
#endif
    int start=Ov5640_Stream(&sensor,1);
    xil_printf("VIDEO %s stream_start=%d bus=%d reg=%04x frame=%08x bytes=%d\r\n",tag,start,sensor.transport_error,sensor.error_register,(unsigned)(UINTPTR)g_video_frame,(unsigned)sizeof(g_video_frame));
    unsigned frames=0,done=0,shorts=0;uint32_t irqs=0;
    uint32_t error_polls=0,first_error_us=0xffffffffU;
    /* AXI timer0 is already free-running at 200MHz; usleep uses a PMU
     * counter that may be reset by each delay and is not a capture clock. */
    uint32_t previous=read(XPAR_XTMRCTR_0_BASEADDR,8);
    uint64_t elapsed_ticks=0;
#ifndef MAX_CAMERA_VIDEO_SECONDS
#define MAX_CAMERA_VIDEO_SECONDS 10
#endif
    static_assert(MAX_CAMERA_VIDEO_SECONDS >= 1 && MAX_CAMERA_VIDEO_SECONDS <= 1800,
                  "Capture duration must fit the result microsecond counter");
    snapshot("START");
    if(!start) for(unsigned polls=0;polls<MAX_CAMERA_VIDEO_SECONDS*5000U;++polls) {
        uint32_t now=read(XPAR_XTMRCTR_0_BASEADDR,8);
        elapsed_ticks += (uint32_t)(now-previous);
        previous=now;
        if(elapsed_ticks>=uint64_t(MAX_CAMERA_VIDEO_SECONDS)*XPAR_XTMRCTR_0_CLOCK_FREQUENCY)break;
        uint32_t irq=read(rx,XCSI_ISR_OFFSET);
        if(csi_errors(irq)) {
            if(!error_polls)first_error_us=(unsigned)(elapsed_ticks*1000000ULL/XPAR_XTMRCTR_0_CLOCK_FREQUENCY);
            ++error_polls;
        }
        irqs|=irq;
        if(irq&XCSI_ISR_FR_MASK)++frames;
        write(rx,XCSI_ISR_OFFSET,irq);
        for(unsigned n=0;n<64 && (read(rx,XCSI_CSR_OFFSET)&XCSI_CSR_SPFIFONE_MASK);++n) { (void)read(rx,XCSI_SPKTR_OFFSET);++shorts; }
        uint32_t di=read(dma,0xc);
        if(di&1)++done;
        write(dma,0xc,di);
        usleep(1000);
    }
    elapsed_ticks += (uint32_t)(read(XPAR_XTMRCTR_0_BASEADDR,8)-previous);
    unsigned elapsed_us=(unsigned)(elapsed_ticks*1000000ULL/XPAR_XTMRCTR_0_CLOCK_FREQUENCY);
    g_video_result.frames=frames;g_video_result.dma_done=done;
    g_video_result.errors=csi_errors(irqs);
    g_video_result.elapsed_us=elapsed_us;
    g_video_result.receiver_type=read(rx,0x64);
    g_video_result.receiver_line_info=read(rx,0x60);
    xil_printf("VIDEO measured_us=%d dma_fps_milli=%d timer_hz=%d\r\n",elapsed_us,elapsed_us ? (unsigned)((uint64_t)done*1000000000ULL/elapsed_us) : 0,XPAR_XTMRCTR_0_CLOCK_FREQUENCY);
    xil_printf("VIDEO error_polls=%d first_error_us=%u\r\n",error_polls,first_error_us);
    snapshot("RUNNING");
    /* Finish the current DDR frame while the sensor is still sending.
     * Stopping the sensor first and flushing midway can leave a torn image. */
    XV_frmbufwr_DisableAutoRestart(&writer.FrmbufWr);
    unsigned drain;
    for(drain=0;drain<20000;++drain) {
        uint32_t irq=read(rx,XCSI_ISR_OFFSET);
        irqs|=irq;
        write(rx,XCSI_ISR_OFFSET,irq);
        if(XV_frmbufwr_IsIdle(&writer.FrmbufWr))break;
        usleep(100);
    }
    xil_printf("VIDEO coherent_frame=%d drain_polls=%d\r\n",drain<20000,drain);
    g_video_result.coherent=drain<20000;
    /* Include CSI faults during completion of the final DDR frame. */
    g_video_result.errors=csi_errors(irqs);
    int stop=Ov5640_Stream(&sensor,0);
    int dma_stop=XVFrmbufWr_Stop(&writer);
#if !MAX_CAMERA_VIDEO_Y8 || MAX_CAMERA_VIDEO_RGB888_CSC
    XV_CscStop(&csc);
#endif
    Xil_DCacheInvalidateRange((INTPTR)g_video_frame,sizeof(g_video_frame));
    unsigned changed=0;uint32_t hash=2166136261U;
    for(unsigned i=0;i<sizeof(g_video_frame);++i) {if(g_video_frame[i]!=0xa5)++changed;hash=(hash^g_video_frame[i])*16777619U;}
    g_video_result.changed=changed;g_video_result.hash=hash;
    g_video_result.result=start?start:stop?stop:dma_stop?dma_stop:
        (!done || !g_video_result.coherent || g_video_result.errors || !changed)?-20:0;
#if MAX_CAMERA_VIDEO_Y8
    if(!g_video_result.result && (g_video_result.receiver_type&0x3f)!=((MAX_CAMERA_VIDEO_JPEG || MAX_CAMERA_VIDEO_YUV422_BYTES) ? 0x1e : MAX_CAMERA_VIDEO_YUV420 ? 0x1a : MAX_CAMERA_VIDEO_RGB444 ? 0x2a : MAX_CAMERA_VIDEO_RGB555 ? 0x2a : MAX_CAMERA_VIDEO_RGB565 ? 0x22 : (MAX_CAMERA_VIDEO_RGB888 || MAX_CAMERA_VIDEO_YUV444) ? 0x24 : MAX_CAMERA_VIDEO_RAW10 ? 0x2b : 0x2a))
        g_video_result.result=-22;
#endif
    xil_printf("VIDEO %s END frame_events=%d dma_done=%d shorts=%d irq_union=%08x changed=%d hash=%08x stop=%d dma_stop=%d\r\n",tag,frames,done,shorts,irqs,changed,hash,stop,dma_stop);
    snapshot("STOP");
}
void Ov5640VideoCheck_Run(void)
{
    g_video_result.bytes=sizeof(g_video_frame);publish_stage(1);
    xil_printf("VIDEO CHECK BEGIN CAM0 -> CSI1, %dx%d; explicit bringup attempts, no write retries\r\n",video_width,video_height);
    xil_printf("VIDEO SCCB clock_hz=%d hold_cycles=200\r\n",MAX_CAMERA_VIDEO_IIC_HZ);
#if MAX_CAMERA_VIDEO_GPIO_SCCB
    xil_printf("VIDEO SCCB GPIO OPEN-DRAIN base=A00D0000; AXI-IIC0 bypassed\r\n");
#endif
    OV5640_CONFIG config;Ov5640_DefaultConfig(&config);
#if MAX_CAMERA_VIDEO_FAST_LINK
    config.clock.mipi_div=1;
    config.continuous_clock=1;
    config.format=OV5640_UYVY;
#if MAX_CAMERA_VIDEO_CLOCK12
    config.clock.xclk_hz=12000000;
    config.clock.multiplier=200;
    config.window.vts=1042; /* 50MHz / (1600 * 1042) = 29.990fps */
#endif
#endif
#if MAX_CAMERA_VIDEO_Y8
#if MAX_CAMERA_VIDEO_RAW8 || MAX_CAMERA_VIDEO_RAW10
#if MAX_CAMERA_VIDEO_RAW10
    Ov5640_Raw10VgaConfig(&config);
#else
    Ov5640_Raw8VgaConfig(&config);
#endif
    xil_printf("VIDEO NATIVE RAW%d: unbinned VGA ROI, dedicated receiver/writer required\r\n",MAX_CAMERA_VIDEO_RAW10 ? 10 : 8);
#if MAX_CAMERA_VIDEO_FULL_RAW
    if(MAX_CAMERA_VIDEO_RAW10)Ov5640_Raw10FullConfig(&config);
    else Ov5640_Raw8FullConfig(&config);
    xil_printf("VIDEO FULL SENSOR RAW:2592x1944 stride=%d\r\n",video_stride);
#endif
#elif MAX_CAMERA_VIDEO_YUV422_BYTES
    config.format=(OV5640_FORMAT)MAX_CAMERA_VIDEO_YUV422_ORDER;
    xil_printf("VIDEO NATIVE YUV422 order=%d\r\n",MAX_CAMERA_VIDEO_YUV422_ORDER);
#elif MAX_CAMERA_VIDEO_JPEG
    config.format=OV5640_JPEG;
    xil_printf("VIDEO JPEG mode2:640x480 image in1280x480 fixed byte canvas\r\n");
#elif MAX_CAMERA_VIDEO_YUV420
    config.format=OV5640_YUV420;
    xil_printf("VIDEO NATIVE YUV420: CSI payload-byte receiver required\r\n");
#elif MAX_CAMERA_VIDEO_YUV444
    config.format=OV5640_YUV444;
    xil_printf("VIDEO NATIVE YUV444: CSI payload-byte receiver required\r\n");
#elif MAX_CAMERA_VIDEO_RGB555 || MAX_CAMERA_VIDEO_RGB444
    config.format=MAX_CAMERA_VIDEO_RGB555 ? OV5640_RGB555 : OV5640_RGB444;
    xil_printf("VIDEO NATIVE PACKED RGB: CSI payload-byte receiver required\r\n");
#elif MAX_CAMERA_VIDEO_RGB565
    config.format=OV5640_RGB565;
    xil_printf("VIDEO NATIVE RGB565: CSI payload-byte receiver required\r\n");
#elif MAX_CAMERA_VIDEO_RGB888
#if MAX_CAMERA_VIDEO_RGB_ROI
    Ov5640_Raw8VgaConfig(&config);
#endif
    config.format=MAX_CAMERA_VIDEO_RGB888_CSC ? OV5640_YUV444 : OV5640_RGB888;
#if MAX_CAMERA_VIDEO_RGB888_CSC
    xil_printf("VIDEO RGB888: full-chroma YUV444 sensor -> hardware CSC -> RGB8 DDR\r\n");
#else
    xil_printf("VIDEO NATIVE RGB888: dedicated RGB888 receiver / RGB8 writer image required\r\n");
#endif
#else
    config.format=OV5640_Y8;
    xil_printf("VIDEO NATIVE Y8: dedicated RAW8 receiver / direct Y8 writer image required\r\n");
#endif
#endif
#if MAX_CAMERA_RGB_SLOW_CLOCK && MAX_CAMERA_VIDEO_RGB888
    config.clock.mipi_div=2;
#endif
    if(MAX_CAMERA_VIDEO_CSI_BYTES) xil_printf("VIDEO CSI PAYLOAD BYTES writer_width_bytes=%d\r\n",video_stride);
    xil_printf("VIDEO link divider=%d continuous=%d xclk_assumed=%d multiplier=%d VTS=%d\r\n",config.clock.mipi_div,config.continuous_clock,config.clock.xclk_hz,config.clock.multiplier,config.window.vts);
    xil_printf("VIDEO nominal_fps_milli=%d HTS=%d\r\n",Ov5640_FrameRateMilliHz(&config),config.window.hts);
    int result=-1;
    /* A failed prepare is retried only through a complete sensor Init. No
     * individual failed SCCB operation is replayed or treated as successful. */
    unsigned passed_preps=0,failed_preps=0;
    const unsigned prep_limit=MAX_CAMERA_VIDEO_PREP_TESTS>1 ? MAX_CAMERA_VIDEO_PREP_TESTS : 6;
    for(unsigned trial=0;trial<prep_limit;++trial) {
        if(trial)usleep(25000); /* Recovery between whole preparations only. */
#if MAX_CAMERA_VIDEO_CAMERA_POWER
        /* Matching csi-bytes-camera-power image only. AN5641 CAM_PWUP is
         * not documented as RESETB; test its effect without assuming one. */
        write(0xa00c0000U,4,0);
        write(0xa00c0000U,0,0);usleep(10000);
        if(!trial) {
            uint8_t low_id[2]={0,0};
            int low=transport_init();
            if(!low)low=transport_read(&bus,0x3c,0x300a,low_id,2);
            xil_printf("VIDEO CAM_PWUP LOW probe_status=%d chip_id=%04x\r\n",low,(low_id[0]<<8)|low_id[1]);
        }
        write(0xa00c0000U,0,1);usleep(100000);
        xil_printf("VIDEO CAM_PWUP cycle trial=%d low_us=10000 settle_us=100000 readback=%d\r\n",trial+1,read(0xa00c0000U,0)&1U);
#endif
        result=transport_init();
        usleep(1000); /* Allow reset of the AXI-IIC clock domain to settle. */
        OV5640_IO io={&bus,sensor_read,sensor_write,delay};
        if(!result)result=Ov5640_Bind(&sensor,&io,0x3c);
        if(!result)result=Ov5640_InitWithClock(&sensor,config.clock.xclk_hz);
        if(!result)result=Ov5640_Configure(&sensor,&config);
#if MAX_CAMERA_VIDEO_JPEG
        if(!result)result=Ov5640_SetJpegQuality(&sensor,16);
        if(!result)result=Ov5640_SetJpegFixedFrame(&sensor,1280,480,0);
#endif
#if MAX_CAMERA_VIDEO_RGB888 && MAX_CAMERA_VIDEO_RGB_ORDER != 3
        OV5640_PATCH order={0x4300,0x0f,MAX_CAMERA_VIDEO_RGB_ORDER};
        if(!result)result=Ov5640_ApplyFeature(&sensor,OV5640_FEATURE_FORMAT,&order,1);
#endif
#if MAX_CAMERA_VIDEO_MINIMAL_ISP
        if(!result)result=Ov5640_SetIsp(&sensor,0x01,0x20,0);
#endif
#if MAX_CAMERA_VIDEO_MINIMAL_ISP || MAX_CAMERA_VIDEO_MANUAL_WB || MAX_CAMERA_VIDEO_BARS
        if(!result)result=Ov5640_SetWhiteBalance(&sensor,0,1024,1024,1024);
#endif
        if(!result)result=Ov5640_SetExposureLimits(&sensor,config.window.vts-4,config.window.vts-4,512,0);
#if MAX_CAMERA_VIDEO_BARS
        /* Gain-dependent automatic UV reduction otherwise desaturates bars
         * in a dark real scene. Keep the oracle stimulus deterministic. */
        if(!result)result=Ov5640_SetExposure(&sensor,256,16);
#else
        if(!result)result=Ov5640_SetAutoExposure(&sensor,1,1);
#endif
        uint8_t formatter=0,mux=0;
        if(!result)result=Ov5640_Read(&sensor,0x4300,&formatter,1);
        if(!result)result=Ov5640_Read(&sensor,0x501f,&mux,1);
        if(!result)result=Ov5640_SetTestPattern(&sensor,MAX_CAMERA_VIDEO_BARS,0,0,0,0,0);
        if(!result)result=sensor_status("PRE");
        g_video_result.formatter=formatter;g_video_result.mux=mux;
        xil_printf("VIDEO bringup trial=%d status=%d bus=%d reg=%04x state=%d\r\n",trial+1,result,sensor.transport_error,sensor.error_register,sensor.state);
        g_video_result.sensor_error_register=sensor.error_register;
        g_video_result.sensor_state=sensor.state;
        if(result)++failed_preps;
        else {
            ++passed_preps;
            if(MAX_CAMERA_VIDEO_PREP_TESTS==1)break;
        }
    }
    xil_printf("VIDEO PREP SUMMARY passed=%d failed=%d strict_tests=%d\r\n",passed_preps,failed_preps,MAX_CAMERA_VIDEO_PREP_TESTS);
    if(MAX_CAMERA_VIDEO_PREP_TESTS>1 && failed_preps)result=-23;
    if(result) {g_video_result.result=result;publish_stage(0xffffffffU);return;}
    publish_stage(2);
    result=prepare_receiver();
    xil_printf("VIDEO receiver prepare=%d\r\n",result);
    if(result) {g_video_result.result=result;publish_stage(0xffffffffU);return;}
    snapshot("BEFORE");
    publish_stage(3);
    g_video_result.result=-21; /* A failed pattern setup must not look successful. */
#if MAX_CAMERA_VIDEO_BARS
    capture("BARS");
#else
    capture("LIVE");
#endif
#if MAX_CAMERA_VIDEO_JPEG
    uint32_t jpeg_length=0;uint8_t overflow=0;
    int jpeg_status=Ov5640_GetJpegStatus(&sensor,&jpeg_length,&overflow);
    xil_printf("VIDEO JPEG length=%d overflow=%d status=%d\r\n",jpeg_length,overflow,jpeg_status);
#endif
    /* DMA flush is held until hardware reset on this IP version. A later
     * diagnostic execution needs a qualified GPIO reset image or FPGA reload. */
    /* Allow the MIPI stop transition to settle before touching ISP controls.
     * Report cleanup independently: the capture evidence is already complete. */
    usleep(10000);
    int cleanup=sensor.state==OV5640_CONFIGURED ?
        Ov5640_SetTestPattern(&sensor,0,0,0,0,0,0) : OV5640_E_STATE;
    xil_printf("VIDEO cleanup=%d bus=%d reg=%04x state=%d\r\n",cleanup,
        sensor.transport_error,sensor.error_register,sensor.state);
    int post=sensor_status("POST");
    xil_printf("VIDEO post_status=%d verified_power_nacks=%d\r\n",post,sensor.verified_power_nacks);
    write(rx,XCSI_CCR_OFFSET,0);
    publish_stage(g_video_result.result?0xffffffffU:4);
    xil_printf("VIDEO CHECK END output gated, receiver disabled\r\n");
}
#else
void Ov5640VideoCheck_Run(void) {}
#endif
