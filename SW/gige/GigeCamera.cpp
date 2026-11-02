#include "GigeCamera.h"
#include "GigeTiming.h"
#include "GigeSensorFeatures.h"
#include "GigeFpgaIsp.h"
#include "GigeRemapControl.h"
#include "GigeRemapReader.h"
#include "GigeCameraDiagnostics.h"
#include "GigeVisionBoard.h"
#include "AxiIicPolled.h"
#include "AxiGpioSccb.h"
#include "GigeSccbAccess.h"
#include "Ov5640.h"
#include "sleep.h"
#include "xil_cache.h"
#include "xil_io.h"
#include "xil_printf.h"
#include "xcsi_hw.h"
#include "xparameters.h"
#include "xv_csc_l2.h"
#include "xv_frmbufwr_l2.h"
#ifndef MAX_CAMERA_GIGE_RAW10_HW
#define MAX_CAMERA_GIGE_RAW10_HW 0
#endif
#ifndef MAX_CAMERA_GIGE_FPGA_ISP
#define MAX_CAMERA_GIGE_FPGA_ISP 0
#endif
static GigeFpgaIsp fpga_isp;
static GigeRemapControl remap;
static GigeRemapReader remap_reader;
static uint32_t remap_banks[2][GIGE_REMAP_MAX_WORDS] __attribute__((aligned(64)));
static bool remap_initialized;
static uint64_t remap_geometry=1;
static uint32_t fpga_read(void *,uint32_t address) {return Xil_In32(address);}
static void fpga_write(void *,uint32_t address,uint32_t value) {Xil_Out32(address,value);}
/* Maximum buffers are allocated once. Only active dimensions are touched. */
static uint8_t raw[6][(GIGE_UNIT_WIDTH_MAX*3U+16U)*GIGE_UNIT_HEIGHT_MAX] __attribute__((aligned(64)));
static uint8_t images[4][GIGE_UNIT_WIDTH_MAX*GIGE_UNIT_HEIGHT_MAX*3U] __attribute__((aligned(64)));
static uint32_t width=GIGE_UNIT_WIDTH,height=GIGE_UNIT_HEIGHT,format=GIGE_UNIT_PIXEL_FORMAT;
#if MAX_CAMERA_GIGE_GPIO_SCCB
static AXI_GPIO_SCCB bus;
static int bus_init() {return AxiGpioSccb_Init(&bus,0xa00d0000U,GIGE_UNIT_SENSOR_I2C_HZ);}
#define camera_read AxiGpioSccb_Read
#define camera_write AxiGpioSccb_Write
#else
static AXI_IIC_POLLED bus;
static int bus_init() {return AxiIicPolled_Init(&bus,0xa0020000U,GIGE_UNIT_SENSOR_I2C_HZ,GIGE_UNIT_SENSOR_SDA_HOLD);}
#define camera_read AxiIicPolled_Read
#define camera_write AxiIicPolled_Write
#endif
static OV5640 sensor;
static GigeSensorTelemetry telemetry;
static GigeSccbAccess sensor_access(telemetry);
#if MAX_CAMERA_GIGE_GPIO_SCCB
static SccbAsync telemetry_bus;
static void telemetry_release(void *,unsigned mask) {Xil_Out32(bus.base+4U,mask&3U);}
static unsigned telemetry_levels(void *) {return Xil_In32(bus.base)&3U;}
static void telemetry_publish(void *,uint16_t reg,uint8_t value)
{GigeSensorFeatures_Observe(reg,&value,1);}
#endif
int GigeCamera_SensorAvailable(void)
{
    GigeSccbGuard guard(sensor_access,GigeVisionBoard_NowUs());
    return guard?1:0;
}
static void poll_telemetry(bool allowed,bool acquisition_active)
{
    sensor_access.poll(GigeVisionBoard_NowUs(),allowed,acquisition_active);
    if(telemetry.last_error) {
        GigeSensorFeatures_TelemetryError(telemetry.error_register,telemetry.last_error);
        telemetry.last_error=0;
    }
}
void GigeCamera_ServiceTelemetry(void)
{
    poll_telemetry(false,false);
    if(remap_initialized)GigeRemapControl_Poll(&remap,GigeCamera_Busy());
}
void GigeCamera_RemapOwner(int present)
{
    if(!present && remap_initialized &&
       (remap.map.state==GIGE_REMAP_UPLOAD || remap.map.state==GIGE_REMAP_VALIDATE))
        GigeRemapControl_Invalidate(&remap);
}
static void remap_geometry_changed(void)
{
    GigeRemapControl_Invalidate(&remap);
    /* No previous map survives a change, including generation wrap. */
    if(++remap_geometry==0)remap_geometry=1;
}

#if !MAX_CAMERA_GIGE_CSI_BYTES
static XV_Csc_l2 csc;
#endif
static XV_FrmbufWr_l2 writer;
static const uint32_t rx=XPAR_MIPI_CSI2_RX_SUBSYST_1_BASEADDR,csc_base=0xa00a0000U,dma=0xa0080000U;
static int ready,requested,streaming,inflight,fault;
static bool dma_completed_pending;
static unsigned capture_index,image_index;
static GigeVision *transport;
static GigePreset recovery_preset;
static int recovery_valid;
unsigned g_camera_recoveries;
static uint32_t error_union;
static uint32_t frame_rate_enabled,frame_rate_requested;
static uint32_t compression;
static uint64_t deadline,frame_started,frame_timestamp;
static uint32_t pipeline_completed;
static uint32_t frame_errors;
unsigned g_camera_completed,g_camera_dropped,g_camera_csi_errors;
GigeCameraFaultEvidence g_camera_fault_evidence;
static uint32_t rd(uint32_t b,uint32_t o) {return Xil_In32(b+o);}
static void wr(uint32_t b,uint32_t o,uint32_t v) {Xil_Out32(b+o,v);}
static void delay(void *,uint32_t us) {usleep(us);(void)GigeVisionBoard_NowUs();}
static int read_sensor(void *u,uint8_t a,uint16_t r,uint8_t *p,uint16_t n) {int e=camera_read(u,a,r,p,n);if(!e)GigeSensorFeatures_Observe(r,p,n);else xil_printf("SCCB rd reg=%04x e=%d phase=%d irq=%08x status=%08x\r\n",r,e,bus.phase,bus.interrupts,bus.status);return e;}
static int write_sensor(void *u,uint8_t a,uint16_t r,const uint8_t *p,uint16_t n) {int e=camera_write(u,a,r,p,n);if(!e)GigeSensorFeatures_Observe(r,p,n);else xil_printf("SCCB wr reg=%04x e=%d phase=%d irq=%08x status=%08x\r\n",r,e,bus.phase,bus.interrupts,bus.status);return e;}
static bool receiver_initialized;
static bool color_selected(uint32_t f) {return !compression && GigeFpgaIsp_ColorSelected(&fpga_isp,f);}
static bool sensor_raw(uint32_t f) {return GigeImage_Raw(f) || color_selected(f);}
#if MAX_CAMERA_GIGE_CSI_BYTES
static OV5640_FORMAT sensor_format(uint32_t f)
{
    switch(f) {
    case GIGE_PIXEL_MONO8:return OV5640_Y8;
    case GIGE_PIXEL_BG8:return OV5640_RAW8;
    case GIGE_PIXEL_BG10:case GIGE_PIXEL_BG10P:return OV5640_RAW10;
    case GIGE_PIXEL_RGB565:return OV5640_RGB565;
    case GIGE_PIXEL_RGB555:return OV5640_RGB555;
    case GIGE_PIXEL_RGB444:return OV5640_RGB444;
    case GIGE_PIXEL_YUV420:return OV5640_YUV420;
    case GIGE_PIXEL_YUV444:return OV5640_YUV444;
    case GIGE_PIXEL_RGB8:case GIGE_PIXEL_BGR8:return OV5640_YUYV;
    case GIGE_PIXEL_UYVY8:return OV5640_UYVY;
    case GIGE_PIXEL_YVYU8:return OV5640_YVYU;
    case GIGE_PIXEL_VYUY8:return OV5640_VYUY;
    default:return OV5640_YUYV;
    }
}
static uint32_t input_bytes(uint32_t w,uint32_t f)
{
    if(f==GIGE_PIXEL_RGB8 || f==GIGE_PIXEL_BGR8)return w*(color_selected(f)?3:2);
#if MAX_CAMERA_GIGE_RAW10_HW
    return GigeImage_RowBytes(w,f);
#else
    return f==GIGE_PIXEL_BG10?w*5/4:GigeImage_RowBytes(w,f);
#endif
}
#endif
static void geometry_base(OV5640_CONFIG *c,uint32_t w,uint32_t h,uint32_t f)
{
#if MAX_CAMERA_GIGE_CSI_BYTES
    uint32_t output_width=w,output_height=h;
    bool color=color_selected(f);
    if(color) {
        GigeFpgaGeometry g;
        if(!GigeFpgaIsp_Geometry(&fpga_isp,h,w,&g)){w=g.cols;h=g.rows;}
    }
    c->format=compression?OV5640_JPEG:color?OV5640_RAW10:sensor_format(f);
#if MAX_CAMERA_GIGE_RAW10_HW
    // VCO remains800MHz. RAW10 can use the sensor's faster /1 system
    // divider now that unpacking no longer blocks capture on the R5.
    c->clock.sysdiv=(!compression && (f==GIGE_PIXEL_BG10 || f==GIGE_PIXEL_BG10P || color))?1:2;
#endif
    if(!compression && sensor_raw(f)) {
        c->window.width=w;c->window.height=h;
        c->window.x_start=c->window.y_start=0;c->window.x_end=w+31;c->window.y_end=h+7;
        c->window.x_offset=16;c->window.y_offset=4;
        c->window.x_increment=c->window.y_increment=0x11;
        c->window.hts=2844;c->window.vts=1968;
#if MAX_CAMERA_GIGE_RAW10_HW
        if(f==GIGE_PIXEL_BG10) {
            /* LE16 expands the wire payload by60%. At full array, VTS2425
             * is11.599FPS: the qualified 1GbE limit with control-service
             * margin. Smaller crops retain the sensor's14.293FPS cadence. */
            uint32_t lines=(uint32_t)(((uint64_t)w*h*2425U+GIGE_UNIT_WIDTH_MAX*GIGE_UNIT_HEIGHT_MAX-1U)/(GIGE_UNIT_WIDTH_MAX*GIGE_UNIT_HEIGHT_MAX));
            if(lines>c->window.vts)c->window.vts=(uint16_t)lines;
        }
#endif
        if(color) {
            // Bound the selected FPGA output payload to 100 MB/s, leaving Ethernet service margin.
            uint64_t numerator=(uint64_t)GigeImage_RowBytes(output_width,f)*output_height*Ov5640_ConfigSystemClockHz(c);
            uint64_t denominator=(uint64_t)c->window.hts*100000000U;
            uint32_t lines=(uint32_t)((numerator+denominator-1)/denominator);
            if(lines>c->window.vts)c->window.vts=(uint16_t)lines;
        }
        /* Fixed BayerBG phase: raw crop has no ISP scaling/orientation. */
        c->mirror=c->flip=0;
        if(!MAX_CAMERA_GIGE_RAW10_HW && f!=GIGE_PIXEL_BG8 && w*h>640U*480U) {
            uint32_t period=(w*h+3U)/4U+30000U;
            c->window.vts=(uint16_t)((period*40U+c->window.hts-1)/c->window.hts);
            if(c->window.vts<1968)c->window.vts=1968;
        }
        return;
    }
    if(sensor.config.format==OV5640_RAW8 || sensor.config.format==OV5640_RAW10) {
        c->mirror=GIGE_UNIT_SENSOR_MIRROR;c->flip=GIGE_UNIT_SENSOR_FLIP;
    }
#endif
    bool bin=w<=1280 && h<=960;
    c->window.width=w;c->window.height=h;
    c->window.x_start=0;c->window.y_start=bin?4:0;
    c->window.x_end=2623;c->window.y_end=1951;
    c->window.x_offset=16;c->window.y_offset=bin?6:4;
    c->window.x_increment=c->window.y_increment=bin?0x31:0x11;
    c->window.hts=bin?1600:2844;

    /* Leave CPU conversion/cache work enough time before the next sensor frame. */
    c->window.vts=bin?(w*h<=640U*480U?1042:2084):3375;
    /* Fixed-canvas JPEG needs encoder time. At1042 lines it only delivers
     * every other sensor frame; start at2084 so requested FPS tracks output. */
    if(compression && c->window.vts<2084)c->window.vts=2084;
    if(f==GIGE_PIXEL_RGB8 || f==GIGE_PIXEL_BGR8 || f==GIGE_PIXEL_RGB565 || (f==GIGE_PIXEL_YUV444 && w*h>640U*480U)) {
        /* R5 software conversion must finish before the next capture fills.
         * Budget 250ns/pixel plus 30ms for cache and control servicing. */
        uint32_t period_us=(w*h+3U)/4U+30000U;
        uint64_t denom=(uint64_t)c->window.hts*1000000U;
        uint32_t lines=(uint32_t)(((uint64_t)period_us*Ov5640_ConfigSystemClockHz(c)+denom-1)/denom);
        if(lines>c->window.vts)c->window.vts=lines;
    }
}
static uint32_t effective_clock(const OV5640_CONFIG *c) {return Ov5640_ConfigSystemClockHz(c);}
static uint32_t effective_rate(const OV5640_CONFIG *c) {return Ov5640_FrameRateMilliHz(c);}
static uint32_t maximum_rate(uint32_t w,uint32_t h,uint32_t f)
{
    OV5640_CONFIG c=sensor.config;geometry_base(&c,w,h,f);
    return effective_rate(&c);
}
static uint32_t minimum_rate(void)
{
    uint32_t minimum=GigeTiming_MinRate(effective_clock(&sensor.config),sensor.config.window.hts);
    return minimum?minimum:200;
}
static void geometry(OV5640_CONFIG *c,uint32_t w,uint32_t h,uint32_t f)
{
    geometry_base(c,w,h,f);
    if(frame_rate_enabled && frame_rate_requested) {
        uint32_t lines=GigeTiming_Vts(effective_clock(c),c->window.hts,c->window.vts,frame_rate_requested);
        if(lines)c->window.vts=(uint16_t)lines;
    }
}
static uint32_t writer_ppc(void)
{return (fpga_isp.capabilities&GIGE_FPGA_WRITER_PPC4_CAPABILITY)?4U:2U;}
static uint32_t stride(uint32_t w) {
#if MAX_CAMERA_GIGE_CSI_BYTES
    const uint32_t alignment=writer_ppc()*8U;
    return (input_bytes(w,format)+alignment-1U)&~(alignment-1U);
#else
    return (w*2U+15U)&~15U;
#endif
}
static int pipeline_size(uint32_t w,uint32_t h)
{
#if MAX_CAMERA_GIGE_CSI_BYTES
    XVidC_VideoStream stream={};stream.ColorFormatId=XVIDC_CSF_YONLY;
    const uint32_t ppc=writer_ppc();
    stream.ColorDepth=XVIDC_BPC_8;stream.PixPerClk=(XVidC_PixelsPerClock)ppc;stream.FrameRate=XVIDC_FR_30HZ;
    /* The paired v13 adapter pads only the final word; GVSP keeps true row bytes. */
    stream.Timing.HActive=ppc==4U?(input_bytes(w,format)+3U)&~3U:input_bytes(w,format);stream.Timing.VActive=h;
    return XVFrmbufWr_SetMemFormat(&writer,stride(w),XVIDC_CSF_MEM_Y8,&stream);
#else
    XV_CscSetActiveSize(&csc,w,h);
    XVidC_VideoWindow window={0,0,w,h};
    if(XV_CscSetDemoWindow(&csc,&window))return -13;
    XVidC_VideoStream stream={};stream.ColorFormatId=XVIDC_CSF_YCRCB_422;
    stream.ColorDepth=XVIDC_BPC_8;stream.PixPerClk=XVIDC_PPC_2;stream.FrameRate=XVIDC_FR_30HZ;
    stream.Timing.HActive=w;stream.Timing.VActive=h;
    return XVFrmbufWr_SetMemFormat(&writer,stride(w),XVIDC_CSF_MEM_YUYV8,&stream);
#endif
}
static int disable_receiver(void)
{
    wr(rx,XCSI_GIER_OFFSET,0);wr(rx,XCSI_IER_OFFSET,0);
    wr(rx,XCSI_CCR_OFFSET,0);
    for(unsigned wait=0;wait<10000;++wait) {
        if(!(rd(rx,XCSI_CSR_OFFSET)&XCSI_CSR_RIPCD_MASK))return 0;
        usleep(10);
    }
    return -9;
}
static int setup_video(uint32_t w,uint32_t h,bool restart_receiver=false)
{
    /* Never reset an AXI read master with outstanding transactions. A stuck
     * enabled invocation needs a system reset, not a video-only recovery. */
    if(!GigeRemapReader_CanReset(&remap_reader))return GIGE_STATUS_BUSY;
    GigeRemapMap_ReaderQuiesced(&remap.map);
    GigeFpgaIsp_Disable(&fpga_isp);
    /* Preserve CSI/D-PHY sequencing; new images separate reset from LED3. */
    const bool configure_receiver=restart_receiver || !receiver_initialized;
    if(configure_receiver && disable_receiver())return -9;
    /* Legacy reset uses LED channel1 bit3. Signature0A moves reset to
     * channel2 bit0, so LED3 may blink without resetting the video path. */
#if MAX_CAMERA_GIGE_RAW10_HW
    // Channel2 belongs exclusively to the hardware RAW10 repacker.
    wr(0xa00d0000U,8,format==GIGE_PIXEL_BG10P?1:format==GIGE_PIXEL_BG10?2:0);
#endif
    uint32_t reset_address=GIGE_UNIT_VIDEO_RESET_GPIO+(fpga_isp.independent_stream_led?8U:0U);
    uint32_t reset_mask=fpga_isp.independent_stream_led?1U:GIGE_UNIT_VIDEO_RESET_MASK;
    uint32_t reset_value=Xil_In32(reset_address);
    Xil_Out32(reset_address,reset_value&~reset_mask);
    usleep(10);
    Xil_Out32(reset_address,reset_value|reset_mask);
    usleep(1000);
    if(GigeRemapReader_ResetObserved(&remap_reader))return -18;
    if(color_selected(format)) {
        GigeFpgaGeometry g;
        if(GigeFpgaIsp_Geometry(&fpga_isp,h,w,&g) ||
           GigeFpgaIsp_ApplyOutput(&fpga_isp,&g,format))return -16;
    } else if(GigeFpgaIsp_Apply(&fpga_isp,h,w,format==GIGE_PIXEL_BG10 || format==GIGE_PIXEL_BG10P))return -16;
    if(remap_reader.present && color_selected(format)) {
        const uint32_t *map=nullptr;uint64_t generation=0;
        if(remap.enabled) {
            if(remap.geometry!=remap_geometry ||
               GigeRemapMap_Prepare(&remap.map,h,w,&map,&generation))return -18;
            /* Dedicated static buffers reside in the linked application DDR.
             * The BSP cache clean completes before the PL receives the pointer. */
            Xil_DCacheFlushRange((INTPTR)map,h*w*sizeof(*map));
            __asm__ volatile("dsb sy" ::: "memory");
        }
        if(GigeRemapReader_Configure(&remap_reader,h,w,(uint64_t)(UINTPTR)map,remap.enabled))return -18;
        /* Protect the bank before any invocation can expose it to the PL. */
        if(remap.enabled && GigeRemapMap_ReaderStarted(&remap.map,generation))return -18;
    } else if(remap.enabled)return -18;
    /* This implementation never calls XVFrmbufWr_Stop: its flush bit cannot
     * be cleared without a hardware reset on the installed IP. */
    if(rd(dma,0)&0x20) {xil_printf("GIGE CAMERA writer flushed: program FPGA before boot\r\n");return -8;}
    if(configure_receiver) {
        wr(rx,XCSI_GIER_OFFSET,0);wr(rx,XCSI_IER_OFFSET,0);
        wr(rx,XCSI_CCR_OFFSET,XCSI_CCR_SOFTRESET_MASK);
        unsigned wait;
        for(wait=0;wait<10000;++wait) {if(!(rd(rx,XCSI_CSR_OFFSET)&XCSI_CSR_RIPCD_MASK))break;usleep(10);}
        if(wait==10000)return -10;
        wr(rx,XCSI_CCR_OFFSET,0);wr(rx,XCSI_ISR_OFFSET,rd(rx,XCSI_ISR_OFFSET));
        wr(rx,XCSI_CCR_OFFSET,XCSI_CCR_COREENB_MASK);
        receiver_initialized=true;
    }
#if !MAX_CAMERA_GIGE_CSI_BYTES
    XV_csc_Config *cfg=XV_csc_LookupConfig(0);
    if(!cfg || XV_csc_CfgInitialize(&csc.Csc,cfg,csc_base))return -11;
    /* The private full-resolution FPGA variant has larger HLS limits. Keep
     * BSP tables unchanged and use local copies matching that image. */
    csc.Csc.Config.MaxWidth=GIGE_UNIT_WIDTH_MAX;
    csc.Csc.Config.MaxHeight=GIGE_UNIT_HEIGHT_MAX;
    XV_CscSetPowerOnDefaultState(&csc);
    if(XV_CscSetColorspace(&csc,XVIDC_CSF_YCRCB_422,XVIDC_CSF_YCRCB_422,XVIDC_BT_601,XVIDC_BT_601,XVIDC_CR_16_235))return -12;
#endif
    if(XVFrmbufWr_Initialize(&writer,dma))return -13;
#if MAX_CAMERA_GIGE_CSI_BYTES
    writer.FrmbufWr.Config.Y8En=1;
    writer.FrmbufWr.Config.PixPerClk=writer_ppc();
    writer.FrmbufWr.Config.MaxWidth=8192;
#else
    writer.FrmbufWr.Config.MaxWidth=GIGE_UNIT_WIDTH_MAX;
#endif
    writer.FrmbufWr.Config.MaxHeight=GIGE_UNIT_HEIGHT_MAX;
    if(pipeline_size(w,h) || XVFrmbufWr_SetBufferAddr(&writer,(UINTPTR)raw))return -14;
    wr(dma,4,0);wr(dma,8,1);wr(dma,0xc,rd(dma,0xc));
    XV_frmbufwr_DisableAutoRestart(&writer.FrmbufWr);
    remap.hardware_status=0;
    capture_index=0;inflight=0;streaming=0;dma_completed_pending=false;
    return 0;
}
static int arm(void)
{
    dma_completed_pending=false;
    if(transport)GigeVision_ForgetBuffer(transport,raw[capture_index]);
    wr(dma,0xc,rd(dma,0xc)); /* toggle-on-write interrupt status */
    XVFrmbufWr_SetBufferAddr(&writer,(UINTPTR)raw[capture_index]);
    XVFrmbufWr_Start(&writer);inflight=1;frame_errors=0;
    frame_started=GigeVisionBoard_NowUs();
    /* Reset changes the device epoch immediately, never a frame already armed.
     * DMA timeout and telemetry keep using absolute frame_started instead. */
    frame_timestamp=GigeVision_TimestampAt(transport,frame_started);
    uint32_t rate=effective_rate(&sensor.config);
    uint64_t timeout=rate?3000000000ULL/rate+500000U:2000000U;
    deadline=frame_started+(timeout>2000000U?timeout:2000000U);
    if(remap_reader.present && color_selected(format) && GigeRemapReader_Start(&remap_reader)) {
        fault=1;requested=0;
        if(transport){transport->active=0;transport->frame=0;}
        return -18;
    }
    return 0;
}
int GigeCamera_Init(void)
{
    if(!GigeRemapReader_CanReset(&remap_reader))return GIGE_STATUS_BUSY;
    GigeSccbGuard guard(sensor_access,GigeVisionBoard_NowUs());
    if(!guard)return GIGE_STATUS_BUSY;
    /* Old cache belongs to the previous sensor lifecycle. The guard guarantees
     * cancellation has completed before GPIO initialization or sensor reset. */
    telemetry={};
    const GigeFpgaIO fpga_io={nullptr,fpga_read,fpga_write};
    if(GigeFpgaIsp_Init(&fpga_isp,&fpga_io,MAX_CAMERA_GIGE_FPGA_ISP))return -16;
    if(!remap_initialized) {
        if(GigeRemapControl_Init(&remap,remap_banks[0],remap_banks[1],GIGE_REMAP_MAX_WORDS))return -18;
        remap_initialized=true;
    } else {
        GigeRemapMap_ReaderQuiesced(&remap.map);
        remap_geometry_changed();
    }
    GigeRemapReader_Init(&remap_reader,&fpga_io,(fpga_isp.capabilities&GIGE_FPGA_REMAP_CAPABILITY)!=0);
    OV5640_CONFIG config;Ov5640_DefaultConfig(&config);
    config.clock.xclk_hz=12000000;config.clock.multiplier=200;
    config.clock.mipi_div=1;config.continuous_clock=1;config.format=OV5640_UYVY;config.window.vts=1042;
    config.mirror=GIGE_UNIT_SENSOR_MIRROR;
    config.flip=GIGE_UNIT_SENSOR_FLIP;
    geometry(&config,width,height,format);
    int result=-1;
    for(unsigned trial=0;trial<3;++trial) {
        result=bus_init();
        OV5640_IO io={&bus,read_sensor,write_sensor,delay};
        if(!result)result=Ov5640_Bind(&sensor,&io,0x3c);
        GigeSensorFeatures_Bind(&sensor);
        if(!result)result=Ov5640_InitWithClock(&sensor,config.clock.xclk_hz);
        if(!result)result=Ov5640_Configure(&sensor,&config);
        xil_printf("GIGE CAMERA init attempt=%d result=%d bus=%d reg=%04x\r\n",trial+1,result,sensor.transport_error,sensor.error_register);
        if(!result)break;
    }
    if(result)return result;
    if((result=Ov5640_SetExposureLimits(&sensor,config.window.vts-4,config.window.vts-4,512,0)) ||
       (result=Ov5640_SetAutoExposure(&sensor,1,1)) ||
       (result=Ov5640_SetTestPattern(&sensor,0,0,0,0,0,0)))return result;
    if((result=setup_video(width,height)))return result;
    Xil_DCacheFlushRange((INTPTR)raw,sizeof(raw));
    if((result=GigeSensorFeatures_Prepare()))return result;
#if MAX_CAMERA_GIGE_GPIO_SCCB
    const SccbAsyncIo telemetry_io={nullptr,telemetry_release,telemetry_levels};
    /* 100 ms includes main-loop scheduling latency; board qualification is
     * still required. Keep the existing 1 ms clock-stretch bound. */
    if(SccbAsync_Init(&telemetry_bus,&telemetry_io,GIGE_UNIT_SENSOR_I2C_HZ,bus.timeout_us,100000) ||
       GigeSensorTelemetry_Init(&telemetry,&telemetry_bus,0x3c,100000,nullptr,telemetry_publish))return -17;
#endif
    ready=1;xil_printf("GIGE CAMERA ready %dx%d PFNC=%08x, continuous sensor clock\r\n",width,height,format);return 0;
}
int GigeCamera_Acquisition(void *user,int enable)
{
    if(user)transport=static_cast<GigeVision*>(user);
    if(enable) {
        GigeSccbGuard guard(sensor_access,GigeVisionBoard_NowUs());
        if(!guard)return GIGE_STATUS_BUSY;
        if(!transport || !GigeCamera_Ready())return -1;
        if(remap.map.state==GIGE_REMAP_UPLOAD || remap.map.state==GIGE_REMAP_VALIDATE)
            return GIGE_STATUS_BUSY;
        if(color_selected(format)) {
            GigeFpgaGeometry g;
            if(GigeFpgaIsp_Geometry(&fpga_isp,height,width,&g))return -16;
        }
        if(GigeCamera_FeatureWrite(GIGE_REG_PIXEL_FORMAT,format))return -16;
        recovery_valid=GigeCamera_Export(&recovery_preset)==0;
        requested=1;
        if(!streaming) {
            /* A gated sensor can leave a partial AXIS line at stop. Start
             * every acquisition with empty video FIFOs and a fresh writer. */
            int reset=setup_video(width,height,true);
            if(reset){requested=0;return reset;}
            GigeCameraEvidence_Begin(&g_camera_fault_evidence,width,height,format);
            pipeline_completed=0;
            wr(rx,XCSI_ISR_OFFSET,rd(rx,XCSI_ISR_OFFSET));
            if(arm())return -18;
#if !MAX_CAMERA_GIGE_CSI_BYTES
            XV_CscStart(&csc);
#endif
            int e=Ov5640_Stream(&sensor,1);
            if(e) {requested=0;fault=1;return e;}
            streaming=1;
        }
    } else {
        requested=0; /* drain current frame asynchronously; no flush */
        (void)GigeCamera_SensorAvailable(); /* request telemetry cancellation */
    }
    return 0;
}
void GigeCamera_Poll(GigeVision *g)
{
    transport=g;
    if(sensor.state==OV5640_FAULT) {fault=1;requested=0;streaming=0;inflight=0;g->active=0;g->frame=0;}
    if(!ready || !streaming || fault)return;
    uint32_t irq=rd(rx,XCSI_ISR_OFFSET);wr(rx,XCSI_ISR_OFFSET,irq);
    uint32_t errors=irq&(XCSI_INTR_PROT_MASK|XCSI_INTR_PKTLVL_MASK|XCSI_INTR_DPHY_MASK|
                         XCSI_ISR_SLBF_MASK|XCSI_ISR_WC_MASK|XCSI_ISR_ILC_MASK);
    if(errors) {frame_errors|=errors;error_union|=errors;++g_camera_csi_errors;}
    for(unsigned k=0;k<64 && (rd(rx,XCSI_CSR_OFFSET)&XCSI_CSR_SPFIFONE_MASK);++k)(void)rd(rx,XCSI_SPKTR_OFFSET);
    uint32_t control=rd(dma,0);
    if(errors && !g_camera_fault_evidence.current.first_error.valid)
        GigeCameraEvidence_Record(&g_camera_fault_evidence.current.first_error,
            GigeVisionBoard_NowUs(),frame_started,irq,errors,control,frame_errors,
            g_camera_completed,capture_index);
    const bool remap_running=remap_reader.present && color_selected(format);
    uint32_t remap_status=0;
    /* AP_DONE is clear-on-read. Retain either completion source while waiting
     * for the reader, even if no IRQ is latched. The original deadline runs. */
    if(inflight && ((rd(dma,0xc)&1U) || (control&2U)))dma_completed_pending=true;
    if(inflight && dma_completed_pending &&
       (!remap_running || GigeRemapReader_Completed(&remap_reader,&remap_status)==1)) {
        if(remap_running) {
            remap.hardware_status=remap_status;
            if(remap_status) {
                /* Preserve the first error before recovery clears live status. */
                xil_printf("GIGE REMAP rejected status=%08x size=%ux%u crc=%08x frame=%u\r\n",
                    (unsigned)remap_status,(unsigned)width,(unsigned)height,
                    (unsigned)remap.map.expected_crc,(unsigned)g_camera_completed);
                /* Drop this frame, drain the source and require recovery. */
                ++g_camera_dropped;requested=0;g->active=0;g->frame=0;
            }
        }
        wr(dma,0xc,rd(dma,0xc));inflight=0;dma_completed_pending=false;++g_camera_completed;++pipeline_completed;
        GigeFpgaIsp_CaptureStatistics(&fpga_isp);
        unsigned completed=capture_index;
        uint64_t timestamp=frame_timestamp;
        uint32_t completed_errors=frame_errors;
        /* A submitted DMA buffer remains pinned through its GVSP trailer.
         * Six buffers allow capture to continue while the previous image
         * is transmitted without ever overwriting the sender's source. */
        if(requested) {
            for(unsigned k=1;k<=6;++k) {
                unsigned next=(completed+k)%6;
                if(next!=completed && g->frame!=raw[next]) {capture_index=next;break;}
            }
            if(arm())return;
        }
        if(requested && !completed_errors && !GigeVision_FrameBusy(g)) {
#if MAX_CAMERA_GIGE_RAW10_HW
            if(format==GIGE_PIXEL_BG10 || format==GIGE_PIXEL_BG10P || color_selected(format)) {
                /* R5 has32KiB D-cache. Set/way clean+invalidate preserves
                 * dirty CPU data and costs O(cache), not O(10MiB image).
                 * DMA raw buffers are read-only to the CPU after init. */
                Xil_DCacheFlush();
            } else
#endif
            Xil_DCacheInvalidateRange((INTPTR)raw[completed],stride(width)*height);
#if MAX_CAMERA_GIGE_RAW10_HW
            if(!compression && (format==GIGE_PIXEL_BG10 || format==GIGE_PIXEL_BG10P || color_selected(format))) {
                GigeVision_SubmitFrameStrided(g,raw[completed],GigeImage_RowBytes(width,format),stride(width),timestamp);
            } else {
#endif
#if MAX_CAMERA_GIGE_CSI_BYTES
            uint8_t *image=images[image_index++%4];GigeVision_ForgetBuffer(g,image);
            GigeImage_FromCsi(image,raw[completed],width,height,stride(width),format);
#else
            uint8_t *image=images[image_index++%4];GigeVision_ForgetBuffer(g,image);
            for(uint32_t y=0;y<height;++y)
                GigeImage_Convert(image+y*width*GigeImage_BytesPerPixel(format),
                                  raw[completed]+y*stride(width),width,format);
#endif
            uint32_t bytes=compression?GigeImage_JpegBytes(image,width*height*2):GigeImage_Payload(width,height,format);
            if(bytes)GigeVision_SubmitFrameSized(g,image,bytes,timestamp);else ++g_camera_dropped;
#if MAX_CAMERA_GIGE_RAW10_HW
            }
#endif
        } else if(requested)++g_camera_dropped;
    }
    if(inflight && GigeVisionBoard_NowUs()>deadline) {
        GigeCameraEvidence_Record(&g_camera_fault_evidence.current.timeout,
            GigeVisionBoard_NowUs(),frame_started,irq,errors,control,frame_errors,
            g_camera_completed,capture_index);
        /* Never flush a stalled writer and then pretend it can be restarted. */
        fault=1;requested=0;streaming=0;inflight=0;g->active=0;g->frame=0;
        int csi_bytes=0;
#if MAX_CAMERA_GIGE_CSI_BYTES
        csi_bytes=1;
#endif
        GigeCameraTimeoutState state=GigeCamera_ReadTimeoutState(rd,rx,csc_base,csi_bytes);
        xil_printf("GIGE RX at timeout ccr=%08x csr=%08x clk=%08x lane0=%08x vc0=%08x type=%08x csc_mapped=%d csc=%08x\r\n",state.ccr,state.csr,state.clock,state.lane0,state.vc0,state.type,state.legacy_csc_present,state.legacy_csc);
        GigeSccbGuard guard(sensor_access,GigeVisionBoard_NowUs());
        if(guard)(void)Ov5640_Stream(&sensor,0);
        /* Otherwise recovery waits for cancellation before stopping/resetting. */
        xil_printf("GIGE CAMERA FAULT writer timeout ctrl=%08x irq=%08x; recovery scheduled\r\n",control,irq);
        /* Emit retained evidence only after declaring the fault, never on the
         * high-rate error path. The poll IRQ above may already be zero. */
        const GigeCameraFaultObservation &first=g_camera_fault_evidence.current.first_error;
        xil_printf("GIGE FAULT FIRST valid=%d irq=%08x errors=%08x ctrl=%08x frame_errors=%08x completed=%u buffer=%u poll_us=%08x%08x arm_us=%08x%08x\r\n",
            first.valid,first.irq,first.error_mask,first.dma_control,first.frame_error_union,
            first.completed_count,first.capture_index,(uint32_t)(first.poll_us>>32),(uint32_t)first.poll_us,
            (uint32_t)(first.dma_arm_us>>32),(uint32_t)first.dma_arm_us);
        return;
    }
    if(!inflight) {
        if(requested){if(arm())return;}
        else {
            // A cropped output can finish before the remaining sensor rows.
            // Drain that input frame before gating CSI, preserving its statistics.
            if(fpga_isp.color_running && (fpga_isp.capabilities&64U) &&
               fpga_isp.latest.sequence<pipeline_completed) {
                if(GigeVisionBoard_NowUs()<deadline)return;
                fault=1;
            }
            GigeSccbGuard guard(sensor_access,GigeVisionBoard_NowUs());
            if(!guard)return;
            int e=Ov5640_Stream(&sensor,0);streaming=0;
            if(GigeRemapReader_CanReset(&remap_reader))GigeRemapMap_ReaderQuiesced(&remap.map);
            xil_printf("GIGE CAMERA drained stop=%d frames=%d dropped=%d csi_errors=%d mask=%08x\r\n",e,g_camera_completed,g_camera_dropped,g_camera_csi_errors,error_union);
            if(e || remap.hardware_status)fault=1;
            /* CSC stays auto-restart: avoids stopping halfway through an AXIS
             * frame held by backpressure between single-shot DMA captures. */
        }
    }
}

int GigeCamera_IndependentStreamLed(void) {return fpga_isp.independent_stream_led!=0;}
int GigeCamera_Ready(void) {return ready && !fault && sensor.state!=OV5640_FAULT;}
int GigeCamera_Busy(void) {return streaming || inflight;}
static int black_live_allowed(void)
{
    return GigeFpgaIsp_BlackLiveAllowed(&fpga_isp,GigeCamera_Ready(),requested,
        streaming,sensor.state==OV5640_STREAMING,sensor.config.format==OV5640_RAW10);
}
uint16_t GigeCamera_FeatureRead(uint32_t a,uint32_t *v)
{
    if(GigeRemapControl_Address(a))return GigeRemapControl_Read(&remap,a,v,remap_reader.present);
    if(a==GIGE_F_FPGA_BLACK_LIVE_ACTIVE) {
        if(!v)return GIGE_STATUS_INVALID_ADDRESS;
        *v=black_live_allowed();return 0;
    }
    if(GigeFpgaIsp_Address(a))return GigeFpgaIsp_Read(&fpga_isp,a,v,
        format==GIGE_PIXEL_BG10 || format==GIGE_PIXEL_BG10P || color_selected(format),GigeCamera_Busy());
    if(a==GIGE_F_COMPRESSION){*v=compression;return 0;}
    if(a==GIGE_F_JPEG_QUALITY){GigeSccbGuard guard(sensor_access,GigeVisionBoard_NowUs());if(!guard)return GIGE_STATUS_BUSY;uint8_t q=0;int e=Ov5640_Read(&sensor,0x4407,&q,1);*v=q&63;return e?GIGE_STATUS_BUSY:0;}
    if(a==GIGE_F_NATIVE_FORMATS){*v=MAX_CAMERA_GIGE_CSI_BYTES;return 0;}
    if(a==GIGE_F_WIDTH_MAX){*v=GigeImage_WidthMax(format);return 0;}
    if(a==GIGE_F_HEIGHT_MAX){*v=GigeImage_HeightMax(format);return 0;}
    if(a==GIGE_F_WIDTH_INC){*v=color_selected(format) && !fpga_isp.config.value[GIGE_ISP_SCALER]?16:GigeImage_WidthIncrement(format);return 0;}
    if(a==GIGE_F_RAW_ACTIVE){*v=GigeImage_Raw(format);return 0;}
    if(a==GIGE_F_SENSOR_RAW_ACTIVE){*v=sensor_raw(format);return 0;}
    if(a==GIGE_F_FRAME_RATE_ENABLE){*v=frame_rate_enabled;return 0;}
    if(a==GIGE_F_FRAME_RATE_MIN){*v=minimum_rate();return 0;}
    if(a==GIGE_F_FRAME_RATE_MAX){*v=maximum_rate(width,height,format);return 0;}
    if(a==GIGE_F_RESULTING_FPS){*v=effective_rate(&sensor.config);return 0;}
    if(a==GIGE_F_SENSOR_FPS){uint32_t max=maximum_rate(width,height,format),min=minimum_rate();*v=frame_rate_requested?frame_rate_requested:max;if(*v>max)*v=max;if(*v<min)*v=min;return 0;}
    if(a==GIGE_F_REG_VALUE) {
        GigeSccbGuard guard(sensor_access,GigeVisionBoard_NowUs());
        if(!guard)return GIGE_STATUS_BUSY;
        return GigeSensorFeatures_Read(a,v);
    }
    return GigeSensorFeatures_Read(a,v);
}
uint16_t GigeCamera_FeatureWrite(uint32_t a,uint32_t v)
{
    if(!GigeCamera_Ready())return GIGE_STATUS_BUSY;
    if(GigeRemapControl_Address(a))return GigeRemapControl_Write(&remap,a,v,remap_reader.present,
        GigeCamera_Busy() || !GigeRemapReader_CanReset(&remap_reader),
        width,height,remap_geometry,color_selected(format));
    if(a==GIGE_F_SENSOR_RAW_ACTIVE)return GIGE_STATUS_WRITE_PROTECT;
    if(a==GIGE_F_FPGA_BLACK && GigeCamera_Busy())
        return GigeFpgaIsp_WriteBlackLive(&fpga_isp,v,black_live_allowed());
    if(GigeFpgaIsp_Address(a)) {
        const bool changes_geometry=a==GIGE_F_FPGA_ENABLE || a==GIGE_F_FPGA_SCALER ||
            a==GIGE_F_FPGA_ROI_ENABLE || a==GIGE_F_FPGA_INTERPOLATION ||
            (a>=GIGE_F_FPGA_INPUT_WIDTH && a<=GIGE_F_FPGA_ROI_HEIGHT);
        uint32_t old=0;
        if(changes_geometry)(void)GigeCamera_FeatureRead(a,&old);
        uint16_t e=GigeFpgaIsp_Write(&fpga_isp,a,v,GigeCamera_Busy());
        if(!e && changes_geometry && old!=v)remap_geometry_changed();
        return e;
    }
    GigeSccbGuard guard(sensor_access,GigeVisionBoard_NowUs());
    if(!guard)return GIGE_STATUS_BUSY;
    if(sensor_raw(format) && (a==GIGE_F_MIRROR || a==GIGE_F_FLIP)) {
        /* Writing the already-fixed value is harmless during preset load. */
        return v==0?0:GIGE_STATUS_WRITE_PROTECT;
    }
    if(a==GIGE_F_JPEG_QUALITY) {
        if(GigeCamera_Busy())return GIGE_STATUS_BUSY;
        return Ov5640_SetJpegQuality(&sensor,(uint8_t)v)?GIGE_STATUS_BUSY:0;
    }
    if(a==GIGE_F_COMPRESSION) {
        if(!MAX_CAMERA_GIGE_CSI_BYTES)return v?GIGE_STATUS_NOT_IMPLEMENTED:0;
        if(GigeCamera_Busy())return GIGE_STATUS_BUSY;
        if(v>1 || (v && (width!=640 || height!=480 || format!=GIGE_PIXEL_YUYV8)))return GIGE_STATUS_INVALID_PARAM;
        if(v!=compression)remap_geometry_changed();
        uint32_t old=compression;compression=v;
        uint16_t e=GigeCamera_FeatureWrite(GIGE_REG_PIXEL_FORMAT,format);
        if(e)compression=old;
        return e;
    }
    if(a==GIGE_F_FRAME_RATE_ENABLE || a==GIGE_F_SENSOR_FPS) {
        if(GigeCamera_Busy())return GIGE_STATUS_BUSY;
        uint32_t old_enable=frame_rate_enabled,old_request=frame_rate_requested;
        if(a==GIGE_F_FRAME_RATE_ENABLE) {
            if(v>1)return GIGE_STATUS_INVALID_PARAM;
            frame_rate_enabled=v;
            if(!frame_rate_requested)frame_rate_requested=maximum_rate(width,height,format);
        } else {
            if(v<minimum_rate() || v>maximum_rate(width,height,format))return GIGE_STATUS_INVALID_PARAM;
            frame_rate_requested=v;
        }
        uint16_t result=GigeCamera_FeatureWrite(GIGE_REG_PIXEL_FORMAT,format);
        if(result){frame_rate_enabled=old_enable;frame_rate_requested=old_request;}
        return result;
    }
    if(a==GIGE_REG_WIDTH || a==GIGE_REG_HEIGHT || a==GIGE_REG_PIXEL_FORMAT) {
        if(GigeCamera_Busy())return GIGE_STATUS_BUSY;
        uint32_t w=a==GIGE_REG_WIDTH?v:width,h=a==GIGE_REG_HEIGHT?v:height;
        uint32_t f=a==GIGE_REG_PIXEL_FORMAT?v:format;
        if(!GigeImage_Valid(w,h,f) || (compression && (w!=640 || h!=480 || f!=GIGE_PIXEL_YUYV8)))return GIGE_STATUS_INVALID_PARAM;
#if !MAX_CAMERA_GIGE_CSI_BYTES
        if(f!=GIGE_PIXEL_MONO8 && f!=GIGE_PIXEL_RGB8 && f!=GIGE_PIXEL_BGR8 && f!=GIGE_PIXEL_RGB565 && f!=GIGE_PIXEL_UYVY8 && f!=GIGE_PIXEL_YUYV8)return GIGE_STATUS_NOT_IMPLEMENTED;
#endif
        if(color_selected(f)) {
            GigeFpgaGeometry g;
            if(GigeFpgaIsp_Geometry(&fpga_isp,h,w,&g))return GIGE_STATUS_INVALID_PARAM;
        }
        if(w!=width || h!=height || f!=format)remap_geometry_changed();
        OV5640_CONFIG config=sensor.config;
        geometry(&config,w,h,f);
        if(w!=width || h!=height || f!=format || config.format!=sensor.config.format || config.window.vts!=sensor.config.window.vts || config.clock.sysdiv!=sensor.config.clock.sysdiv || config.window.width!=sensor.config.window.width || config.window.height!=sensor.config.window.height) {
            uint32_t ae,ag,exposure,gain,banding;
            GigeSensorFeatures_Read(GIGE_F_BANDING,&banding);
            GigeSensorFeatures_Read(GIGE_F_EXPOSURE_AUTO,&ae);
            GigeSensorFeatures_Read(GIGE_F_GAIN_AUTO,&ag);
            GigeSensorFeatures_Read(GIGE_F_EXPOSURE_LINES,&exposure);
            GigeSensorFeatures_Read(GIGE_F_GAIN,&gain);
            uint32_t old_hts=sensor.config.window.hts;
            uint64_t exposure_denominator=(uint64_t)config.window.hts*Ov5640_ConfigSystemClockHz(&sensor.config);
            exposure=(uint32_t)(((uint64_t)exposure*old_hts*Ov5640_ConfigSystemClockHz(&config)+exposure_denominator/2U)/exposure_denominator);
            if(!exposure)exposure=1;
            int e=Ov5640_Configure(&sensor,&config);
            /* The DPC RAW tap has no valid AWB statistics. Its estimator can
             * converge to zero after JPEG/ISP, blacking out even RAW data. */
            if(!e && sensor_raw(f) && (sensor.config.format==OV5640_RAW8 || sensor.config.format==OV5640_RAW10))
                e=Ov5640_SetWhiteBalance(&sensor,0,1024,1024,1024);
            if(!e) {
                uint8_t actual[8];
                e=Ov5640_Read(&sensor,0x3808,actual,sizeof(actual));
                if(!e) {
                    uint32_t aw=(actual[0]<<8)|actual[1],ah=(actual[2]<<8)|actual[3];
                    uint32_t at=(actual[4]<<8)|actual[5],av=(actual[6]<<8)|actual[7];
                    xil_printf("GIGE SENSOR geometry readback %dx%d HTS=%d VTS=%d\r\n",aw,ah,at,av);
                    if(aw!=config.window.width || ah!=config.window.height || at!=config.window.hts || av!=config.window.vts)e=-15;
                }
            }
            if(!e && compression)e=Ov5640_SetJpegFixedFrame(&sensor,1280,480,0);
            if(!e)e=Ov5640_SetBanding(&sensor,banding);
            /* Seed a bounded integration before waking the new mode. An old
             * long automatic exposure must not survive a low-to-high FPS
             * transition and delay its first frame beyond the new period. */
            if(!e) {
                uint32_t max=config.window.vts-4;
                e=Ov5640_SetExposure(&sensor,exposure>max?max:exposure,
                                    (uint16_t)(gain<16?16:gain>1023?1023:gain));
            }
            if(!e)e=Ov5640_SetAutoExposure(&sensor,ae!=0,ag!=0);
            /* A frame-period change leaves the AXIS geometry unchanged.
             * Keep the drained pipeline alive across that sensor-only update. */
            if(!e && (w!=width || h!=height || f!=format)) {
                uint32_t old_format=format;format=f;e=setup_video(w,h);if(e)format=old_format;
            }
            if(e) {xil_printf("GIGE IMAGE configure fault e=%d sensor=%d bus=%d reg=%04x\r\n",e,sensor.state,sensor.transport_error,sensor.error_register);fault=1;ready=0;return GIGE_STATUS_BUSY;}
            width=w;height=h;
            xil_printf("GIGE IMAGE size=%dx%d hts=%d vts=%d\r\n",width,height,config.window.hts,config.window.vts);
        }
        format=f;return 0;
    }
    if(a>=GIGE_FEATURE_FIRST && a<GIGE_FEATURE_END &&
       gige_feature_descriptors[(a-GIGE_FEATURE_FIRST)/4].stopped && GigeCamera_Busy())return GIGE_STATUS_BUSY;
    /* Arbitrary sensor register access can change geometry outside GenApi. */
    if(a==GIGE_F_REG_VALUE)remap_geometry_changed();
    return GigeSensorFeatures_Write(a,v);
}
int GigeCamera_TelemetryDeferred(void)
{
    /* Preserve maximum LE16 delivery while the software GPIO high-bandwidth
     * experiments remain unqualified. Hardware AE/AGC continues independently. */
    return streaming && ((uint64_t)GigeImage_Payload(width,height,format)*effective_rate(&sensor.config)>105000000000ULL);
}
void GigeCamera_RefreshControls(void)
{
    static uint64_t next_statistics;
    if(fpga_isp.color_running && (fpga_isp.capabilities&64U)) {
        uint64_t statistics_now=GigeVisionBoard_NowUs();
        if(statistics_now>=next_statistics) {
            next_statistics=statistics_now+1000;
            GigeFpgaIsp_CaptureStatistics(&fpga_isp);
        }
    }
    uint64_t now=GigeVisionBoard_NowUs();
    bool allowed=GigeCamera_Ready() && !(streaming && !requested) && !GigeCamera_TelemetryDeferred() &&
        !(transport && GigeVision_FrameBusy(transport)) &&
        !(streaming && inflight && !GigeTiming_CanServiceSensor(
            effective_rate(&sensor.config),now-frame_started));
#if MAX_CAMERA_GIGE_GPIO_SCCB
    poll_telemetry(allowed,streaming && requested); /* Pending work drains even when starts are gated. */
#else
    static uint64_t next;
    if(allowed && now>=next) {next=now+100000U;(void)GigeSensorFeatures_RefreshOne();}
#endif
}

int GigeCamera_Export(GigePreset *p)
{
    GigeSccbGuard guard(sensor_access,GigeVisionBoard_NowUs());
    if(!guard)return -1;
    if(!p || !GigeCamera_Ready() || GigeCamera_Busy())return -1;
    *p={};p->width=width;p->height=height;p->pixel_format=format;p->frame_rate_enable=frame_rate_enabled;
    GigeCamera_FeatureRead(GIGE_F_SENSOR_FPS,&p->frame_rate);
    p->compression=compression;
    p->line_period_ns=(uint32_t)(((uint64_t)sensor.config.window.hts*1000000000U+effective_clock(&sensor.config)/2U)/effective_clock(&sensor.config));
    if(GigeCamera_FeatureRead(GIGE_F_JPEG_QUALITY,&p->jpeg_quality))return -1;
    if(GigeSensorFeatures_Export(p))return -1;
    p->fpga=fpga_isp.config;
    return GigePreset_Validate(p);
}
uint16_t GigeCamera_Import(const GigePreset *p)
{
    GigeSccbGuard guard(sensor_access,GigeVisionBoard_NowUs());
    if(!guard)return GIGE_STATUS_BUSY;
    if(!GigeCamera_Ready() || GigeCamera_Busy())return GIGE_STATUS_BUSY;
    if(GigePreset_Validate(p))return GIGE_STATUS_INVALID_PARAM;
    if(!fpga_isp.capabilities && p->fpga.value[GIGE_ISP_ENABLE])return GIGE_STATUS_NOT_IMPLEMENTED;
    GigeFpgaIsp target_isp=fpga_isp;
    if(GigeFpgaIsp_Import(&target_isp,&p->fpga))return GIGE_STATUS_INVALID_PARAM;
    if(GigeFpgaIsp_ColorSelected(&target_isp,p->pixel_format)) {
        GigeFpgaGeometry g;
        if(GigeFpgaIsp_Geometry(&target_isp,p->height,p->width,&g))return GIGE_STATUS_INVALID_PARAM;
    }
    GigeFpgaConfig previous_fpga=fpga_isp.config;fpga_isp.config=p->fpga;
    OV5640_CONFIG target=sensor.config;uint32_t previous_compression=compression;compression=p->compression;
    geometry_base(&target,p->width,p->height,p->pixel_format);compression=previous_compression;fpga_isp.config=previous_fpga;
    uint32_t min=GigeTiming_MinRate(effective_clock(&target),target.window.hts);
    uint32_t max=effective_rate(&target);
    if(p->frame_rate<min || p->frame_rate>max)return GIGE_STATUS_INVALID_PARAM;
    if(p->frame_rate_enable) {
        uint32_t lines=GigeTiming_Vts(effective_clock(&target),target.window.hts,target.window.vts,p->frame_rate);
        if(!lines)return GIGE_STATUS_INVALID_PARAM;
        target.window.vts=(uint16_t)lines;
    }
    GigePreset adapted=*p;
    uint32_t line_ns=(uint32_t)(((uint64_t)target.window.hts*1000000000U+effective_clock(&target)/2U)/effective_clock(&target));
    GigePreset_AdaptTiming(&adapted,line_ns,target.window.vts-4);p=&adapted;
    if(p->values[GIGE_PRESET_EXPOSURE_LINES]>(uint32_t)target.window.vts-4 ||
       p->values[GIGE_PRESET_MAX_EXPOSURE_50]>(uint32_t)target.window.vts-4 ||
       p->values[GIGE_PRESET_MAX_EXPOSURE_60]>(uint32_t)target.window.vts-4)return GIGE_STATUS_INVALID_PARAM;
    remap_geometry_changed(); /* Maps and enable are never persisted in UserSets. */
    uint16_t e=GigeCamera_FeatureWrite(GIGE_F_COMPRESSION,0);
    if(!e)e=GigeCamera_FeatureWrite(GIGE_F_FRAME_RATE_ENABLE,0);
    /* Move through an unconstrained even-width ISP format so RAW alignment
     * or JPEG's fixed geometry cannot reject an otherwise valid preset. */
    if(!e)e=GigeCamera_FeatureWrite(GIGE_REG_PIXEL_FORMAT,GIGE_PIXEL_YUYV8);
    if(!e)e=GigeCamera_FeatureWrite(GIGE_REG_WIDTH,p->width);
    if(!e)e=GigeCamera_FeatureWrite(GIGE_REG_HEIGHT,p->height);
    if(!e && GigeFpgaIsp_Import(&fpga_isp,&p->fpga))e=GIGE_STATUS_INVALID_PARAM;
    if(!e)e=GigeCamera_FeatureWrite(GIGE_REG_PIXEL_FORMAT,p->pixel_format);
    if(!e)e=GigeCamera_FeatureWrite(GIGE_F_SENSOR_FPS,p->frame_rate);
    if(!e)e=GigeCamera_FeatureWrite(GIGE_F_FRAME_RATE_ENABLE,p->frame_rate_enable);
    if(!e)e=GigeCamera_FeatureWrite(GIGE_F_JPEG_QUALITY,p->jpeg_quality);
    if(!e)e=GigeCamera_FeatureWrite(GIGE_F_COMPRESSION,p->compression);
    if(!e)e=GigeSensorFeatures_Import(p);
    if(!e && GigeFpgaIsp_Import(&fpga_isp,&p->fpga))e=GIGE_STATUS_INVALID_PARAM;
    return e;
}

int GigeCamera_NeedsRecovery(void){return fault!=0 || sensor.state==OV5640_FAULT;}
int GigeCamera_Reset(GigeVision *g,int preserve_settings)
{
    if(!GigeRemapReader_CanReset(&remap_reader))return GIGE_STATUS_BUSY;
    GigeSccbGuard guard(sensor_access,GigeVisionBoard_NowUs());
    if(!guard)return GIGE_STATUS_BUSY;
    if(preserve_settings && GigeCamera_Ready() && !GigeCamera_Busy())recovery_valid=GigeCamera_Export(&recovery_preset)==0;
    GigeVision_Release(g);GigeVision_ClearHistory(g);
    requested=streaming=inflight=0;
    (void)Ov5640_Stream(&sensor,0);
    ready=0;fault=0;
    if(!preserve_settings){width=GIGE_UNIT_WIDTH;height=GIGE_UNIT_HEIGHT;format=GIGE_UNIT_PIXEL_FORMAT;compression=0;frame_rate_enabled=frame_rate_requested=0;recovery_valid=0;}
    /* Keep CSI disabled across sensor reset/clock reconfiguration. Enabling it
     * before sensor initialization can leave the receive pipeline backpressured
     * after warm reset. Init performs the full receiver/video setup afterward. */
    int e=disable_receiver();
    receiver_initialized=false;
    if(!e)e=GigeCamera_Init();
    if(!e && recovery_valid)e=GigeCamera_Import(&recovery_preset);
    if(e){fault=1;ready=0;}else {
        g->width=width;g->height=height;g->pixel_format=format;g->compression=compression;
        g->payload_size=GigeImage_Payload(width,height,format);++g_camera_recoveries;
    }
    return e;
}
