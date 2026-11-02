#ifndef GIGE_CAMERA_DIAGNOSTICS_H
#define GIGE_CAMERA_DIAGNOSTICS_H
#include <stdint.h>
typedef uint32_t (*GigeCameraDiagnosticRead)(uint32_t base,uint32_t offset);
/* RAM-only evidence from the existing poll loop. Times are software observation
 * and DMA-arm times, never sensor SOF or effective-exposure timestamps.
 * Access only from the same serialized camera/control context. A remote debugger
 * must obtain a quiescent snapshot; this is not an atomic register interface. */
typedef struct {
    uint64_t poll_us,dma_arm_us;
    uint32_t irq,error_mask,dma_control,frame_error_union;
    uint32_t completed_count,capture_index,valid;
} GigeCameraFaultObservation;
typedef struct {
    uint64_t acquisition_generation;
    uint32_t width,height,pixel_format;
    GigeCameraFaultObservation first_error,timeout;
} GigeCameraAcquisitionEvidence;
typedef struct {
    GigeCameraAcquisitionEvidence current,previous_fault;
} GigeCameraFaultEvidence;
static inline void GigeCameraEvidence_Begin(GigeCameraFaultEvidence *e,
    uint32_t width,uint32_t height,uint32_t pixel_format)
{
    GigeCameraFaultObservation empty={0,0,0,0,0,0,0,0,0};
    if(e->current.first_error.valid || e->current.timeout.valid)
        e->previous_fault=e->current;
    ++e->current.acquisition_generation;
    e->current.width=width;e->current.height=height;e->current.pixel_format=pixel_format;
    e->current.first_error=empty;e->current.timeout=empty;
}
static inline void GigeCameraEvidence_Record(GigeCameraFaultObservation *o,
    uint64_t poll_us,uint64_t dma_arm_us,uint32_t irq,uint32_t error_mask,
    uint32_t dma_control,uint32_t frame_error_union,uint32_t completed_count,
    uint32_t capture_index)
{
    if(o->valid)return; /* repeated error polls must not erase the first cause */
    o->poll_us=poll_us;o->dma_arm_us=dma_arm_us;o->irq=irq;o->error_mask=error_mask;
    o->dma_control=dma_control;o->frame_error_union=frame_error_union;
    o->completed_count=completed_count;o->capture_index=capture_index;o->valid=1;
}
#ifdef __cplusplus
extern "C" {
#endif
extern GigeCameraFaultEvidence g_camera_fault_evidence;
#ifdef __cplusplus
}
#endif
typedef struct {
    uint32_t ccr,csr,clock,lane0,vc0,type,legacy_csc;
    int legacy_csc_present;
} GigeCameraTimeoutState;
/* CSI-byte images do not map the legacy video CSC. Diagnostics must never
 * introduce a second fault by touching that absent AXI peripheral. */
static inline GigeCameraTimeoutState GigeCamera_ReadTimeoutState(
    GigeCameraDiagnosticRead read,uint32_t rx,uint32_t legacy_csc,int csi_bytes)
{
    GigeCameraTimeoutState s;
    s.ccr=read(rx,0);s.csr=read(rx,0x10);s.clock=read(rx,0x3c);
    s.lane0=read(rx,0x40);s.vc0=read(rx,0x60);s.type=read(rx,0x64);
    s.legacy_csc_present=!csi_bytes;
    s.legacy_csc=s.legacy_csc_present?read(legacy_csc,0):0;
    return s;
}
#endif
