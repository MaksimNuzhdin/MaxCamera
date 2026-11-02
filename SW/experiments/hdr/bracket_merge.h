#ifndef CAMERA_HDR_BRACKET_MERGE_H
#define CAMERA_HDR_BRACKET_MERGE_H
#include <stddef.h>
#include <stdint.h>
/* Offline integration component. Not linked into camera firmware. Buffers are
 * caller-owned immutable RAW10 samples, identical Bayer phase/geometry. */
typedef struct {
    uint32_t width,height,geometry_generation,configuration_generation,bayer_phase;
    uint64_t frame_id,timestamp_us;
    uint32_t exposure_lines,gain16,line_period_ns;
    uint16_t black,white;
    uint8_t effective_exposure_verified,manual_exposure;
} HdrFrame;
enum { HDR_VALID=0, HDR_BELOW_BLACK=1, HDR_BOTH_SATURATED=2 };
#ifdef __cplusplus
extern "C" {
#endif
int HdrFrame_Validate(const HdrFrame *frame);
int HdrBracket_Validate(const HdrFrame *short_frame,const HdrFrame *long_frame,
                        uint64_t maximum_gap_us);
/* Radiance is black-subtracted DN per exposure line at unity gain, Q16.
 * Prefer the longer exposure below saturation; fall back to short exposure.
 * This performs no motion compensation, denoising, demosaic or tone mapping.
 * Distinct, nonoverlapping input/output arrays are a caller precondition. */
int HdrBracket_Merge(const HdrFrame *short_frame,const HdrFrame *long_frame,
                     const uint16_t *short_raw,const uint16_t *long_raw,
                     size_t samples,uint64_t maximum_gap_us,
                     uint32_t *radiance_q16,uint8_t *flags);
typedef enum { HDR_MERGE_VALIDATE, HDR_MERGE_PIXELS, HDR_MERGE_DONE,
               HDR_MERGE_FAILED } HdrMergeState;
typedef struct {
    HdrMergeState state;
    HdrFrame short_frame,long_frame;
    const uint16_t *short_raw,*long_raw;
    uint32_t *radiance;
    uint8_t *flags;
    size_t samples,cursor;
} HdrMergeJob;
void HdrMerge_Init(HdrMergeJob *job);
/* Optional bounded CPU worker. Same immutable/nonoverlapping buffer contract as
 * Merge. Begin is only for an inactive job; active jobs retain their frame leases.
 * Output is private until DONE. Cancelled/failed partial output is never usable. */
int HdrMerge_Begin(HdrMergeJob *job,const HdrFrame *short_frame,const HdrFrame *long_frame,
                   const uint16_t *short_raw,const uint16_t *long_raw,size_t samples,
                   uint64_t maximum_gap_us,uint32_t *radiance,uint8_t *flags);
/* At most 256 samples per call, including the separate prevalidation pass.
 * Return 0 pending, 1 done, -1 failed. This bounds work, not measured microseconds. */
int HdrMerge_Step(HdrMergeJob *job,unsigned budget);
void HdrMerge_Cancel(HdrMergeJob *job);
#ifdef __cplusplus
}
#endif
#endif
