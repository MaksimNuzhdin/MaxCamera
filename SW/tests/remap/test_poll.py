"""Run the actual production camera poll function with deterministic MMIO/time.

This tests software scheduling/failure policy, not FPGA or sensor timing.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
source = (Path(sys.argv[1]) if len(sys.argv)>1 else root/'gige/GigeCamera.cpp').read_text()
body = source[source.index('void GigeCamera_Poll('):source.index('\nint GigeCamera_IndependentStreamLed')]
fixture = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include "GigeRemapControl.h"
#include "GigeRemapReader.h"
#include "GigeCameraDiagnostics.h"
#define MAX_CAMERA_GIGE_RAW10_HW 1
#define MAX_CAMERA_GIGE_CSI_BYTES 1
enum {OV5640_FAULT=9};
enum {XCSI_ISR_OFFSET=1,XCSI_CSR_OFFSET=2,XCSI_SPKTR_OFFSET=3};
enum {XCSI_INTR_PROT_MASK=1,XCSI_INTR_PKTLVL_MASK=2,XCSI_INTR_DPHY_MASK=4,
      XCSI_ISR_SLBF_MASK=8,XCSI_ISR_WC_MASK=16,XCSI_ISR_ILC_MASK=32,XCSI_CSR_SPFIFONE_MASK=1};
enum {GIGE_PIXEL_BG10=1,GIGE_PIXEL_BG10P=2};
struct GigeVision {int active;const uint8_t *frame;};
struct {int state;} sensor;
static GigeVision *transport;
static GigeRemapReader remap_reader;
static GigeRemapControl remap;
static GigeFpgaIsp fpga_isp;
GigeCameraFaultEvidence g_camera_fault_evidence;
static int ready,streaming,fault,requested,inflight,compression,sensor_access;
static bool dma_completed_pending;
static uint32_t error_union,frame_errors,pipeline_completed,width=32,height=16,format=3;
static unsigned g_camera_completed,g_camera_dropped,g_camera_csi_errors,capture_index,image_index;
static uint64_t frame_started,frame_timestamp,deadline,now;
static uint8_t raw[6][4096],images[4][4096];
static const uint32_t rx=100,dma=200,csc_base=300;
static uint32_t dma_irq,dma_control,reader_ctrl,reader_status;
static unsigned submitted,armed,stopped,cleaned,remap_error_logs;
static bool color_selected(uint32_t){return true;}
static uint64_t GigeVisionBoard_NowUs(){return now;}
static uint32_t rd(uint32_t base,uint32_t offset){
 if(base==dma){if(offset==0xc)return dma_irq;uint32_t value=dma_control;dma_control=0;return value;}
 return 0;
}
static void wr(uint32_t base,uint32_t offset,uint32_t value){if(base==dma && offset==0xc)dma_irq^=value;}
static uint32_t rr(void*,uint32_t a){return a==GIGE_REMAP_READER_BASE?reader_ctrl:reader_status;}
static void rw(void*,uint32_t,uint32_t){}
static int arm(){++armed;inflight=1;dma_completed_pending=false;frame_errors=0;deadline=now+1000;return 0;}
void GigeFpgaIsp_CaptureStatistics(GigeFpgaIsp*){}
static bool GigeVision_FrameBusy(GigeVision*){return false;}
static void GigeVision_SubmitFrameStrided(GigeVision*,const uint8_t*,uint32_t,uint32_t,uint64_t){++submitted;}
static void GigeVision_ForgetBuffer(GigeVision*,const uint8_t*){}
static void GigeVision_SubmitFrameSized(GigeVision*,const uint8_t*,uint32_t,uint64_t){++submitted;}
static uint32_t GigeImage_RowBytes(uint32_t w,uint32_t){return w*3;}
static uint32_t stride(uint32_t w){return w*3;}
static uint32_t GigeImage_JpegBytes(const uint8_t*,uint32_t n){return n;}
static uint32_t GigeImage_Payload(uint32_t w,uint32_t h,uint32_t){return w*h*3;}
static void GigeImage_FromCsi(uint8_t*,const uint8_t*,uint32_t,uint32_t,uint32_t,uint32_t){}
static void Xil_DCacheFlush(){++cleaned;}
using INTPTR=intptr_t;
static void Xil_DCacheInvalidateRange(INTPTR,uint32_t){++cleaned;}
struct GigeSccbGuard {GigeSccbGuard(int,uint64_t){} explicit operator bool()const{return true;}};
static int Ov5640_Stream(void*,int value){assert(value==0);++stopped;return 0;}
template<typename... T>static void xil_printf(const char* format,T...){
 if(std::strstr(format,"GIGE REMAP rejected"))++remap_error_logs;
}
'''
tests = r'''
static void reset_case(GigeVision &g){
 g={1,nullptr};sensor.state=0;ready=streaming=requested=inflight=1;fault=compression=0;dma_completed_pending=false;
 frame_errors=error_union=pipeline_completed=0;capture_index=image_index=0;
 g_camera_completed=g_camera_dropped=g_camera_csi_errors=0;
 submitted=armed=stopped=cleaned=remap_error_logs=0;now=100;deadline=1000;dma_irq=1;dma_control=0;
 frame_started=10;frame_timestamp=42;fpga_isp={};remap={};
 GigeFpgaIO io={nullptr,rr,rw};GigeRemapReader_Init(&remap_reader,&io,1);
 remap_reader.configured=remap_reader.map_mode=remap_reader.last_start_used_map=1;
 remap.map.reader_may_access=1;reader_ctrl=0;reader_status=0;
 g_camera_fault_evidence={};
}
int main(){GigeVision g;
 reset_case(g);GigeCamera_Poll(&g);
 assert(inflight && dma_irq==1 && !submitted && !armed && !cleaned && !g_camera_completed);
 assert(remap.map.reader_may_access && !GigeRemapReader_CanReset(&remap_reader));
 now=1001;GigeCamera_Poll(&g);
 assert(fault && !g.active && !submitted && !armed && stopped==1);
 assert(g_camera_fault_evidence.current.timeout.valid && remap.map.reader_may_access);
 reset_case(g);dma_irq=0;dma_control=2;GigeCamera_Poll(&g);
 assert(!submitted && !armed && inflight);
 reader_ctrl=4;GigeCamera_Poll(&g);
 assert(submitted==1 && armed==1 && !fault); // Preserve clear-on-read DMA completion while remap drains.
 reset_case(g);reader_ctrl=4;GigeCamera_Poll(&g);
 assert(submitted==1 && armed==1 && cleaned==1 && g_camera_completed==1 && !fault && !remap_error_logs);
 reset_case(g);requested=0;reader_ctrl=4;GigeCamera_Poll(&g);
 assert(!submitted && !armed && stopped==1 && !streaming && !remap.map.reader_may_access);
 reset_case(g);reader_ctrl=4;reader_status=4;GigeCamera_Poll(&g);
 assert(!submitted && !armed && stopped==1 && fault && !g.active && !streaming);
 assert(g_camera_dropped==1 && remap.hardware_status==4 && !remap.map.reader_may_access && remap_error_logs==1);
 GigeCamera_Poll(&g);assert(remap_error_logs==1); // No repeated UART work after fault.
 reset_case(g);requested=0;reader_ctrl=4;reader_status=2;GigeCamera_Poll(&g);
 assert(!submitted && !armed && fault && remap.hardware_status==2);
 reset_case(g);remap_reader.present=0;GigeCamera_Poll(&g);
 assert(submitted==1 && armed==1 && !fault); // Legacy pipeline never waits on absent MMIO.
}
'''
with tempfile.TemporaryDirectory(prefix='remap-poll-') as name:
    p = Path(name);(p/'test.cpp').write_text(fixture+body+tests)
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
        '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-I'+str(root/'gige'),
        str(p/'test.cpp'), str(root/'gige/GigeRemapReader.c'), str(root/'gige/GigeRemapMap.c'),
        '-o', str(p/'test')], check=True)
    subprocess.run([str(p/'test')], check=True, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))
print('PASS actual camera poll: delayed reader, original timeout, no early publication/rearm, status rejection including stop, lease drain, legacy bypass')
