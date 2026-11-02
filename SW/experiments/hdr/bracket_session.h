#ifndef CAMERA_HDR_BRACKET_SESSION_H
#define CAMERA_HDR_BRACKET_SESSION_H
#include "bracket_merge.h"
/* Offline single-threaded integration contract; not a sensor driver or scheduler.
 * All storage is caller-owned, dedicated, CPU-addressable unpacked RAW10.
 * Adapter must handle DMA/cache barriers before Complete, and prove effective
 * per-frame exposure independently. A successful SCCB write is not that proof. */
typedef enum { HDR_IDLE, HDR_CONFIGURE, HDR_CAPTURE, HDR_READY,
               HDR_DRAINING, HDR_FAILED } HdrSessionState;
typedef enum { HDR_FREE, HDR_RESERVED, HDR_DMA, HDR_HELD } HdrLeaseState;
typedef struct { uint16_t *data; size_t samples; HdrLeaseState state; } HdrLease;
typedef struct {
    HdrSessionState state;
    HdrLease leases[2];
    HdrFrame requested[2], frames[2];
    uint64_t generation, started_us, timeout_us, maximum_gap_us;
    unsigned phase;
    int configuration_pending;
} HdrSession;
#ifdef __cplusplus
extern "C" {
#endif
void HdrSession_Init(HdrSession *session);
/* Requested templates contain geometry/exposure limits only. Their frame IDs,
 * timestamps and verification flag are ignored; they never become evidence. */
int HdrSession_Begin(HdrSession *s, const HdrFrame requested[2],
                     uint16_t *short_buffer, uint16_t *long_buffer, size_t samples,
                     uint64_t now_us, uint64_t timeout_us, uint64_t maximum_gap_us);
/* A configuration command is claimed once; token identifies session and phase.
 * No buffer may be armed until the command is acknowledged. */
int HdrSession_Configure(HdrSession *s, uint64_t now_us, uint64_t *token, HdrFrame *request);
int HdrSession_Configured(HdrSession *s, uint64_t token, uint64_t now_us, int success);
int HdrSession_Arm(HdrSession *s, uint64_t token, uint64_t now_us, uint16_t **buffer);
int HdrSession_Complete(HdrSession *s, uint64_t token, uint64_t now_us, const HdrFrame *effective);
void HdrSession_Poll(HdrSession *s, uint64_t now_us);
void HdrSession_Cancel(HdrSession *s);
/* Cancellation retains DMA/config ownership until adapter confirms quiescence.
 * Never acknowledge before the old DMA and sensor command have actually stopped. */
int HdrSession_Quiesced(HdrSession *s, uint64_t token);
/* READY retains both leases until the consumer has finished reading them.
 * Release is also the explicit acknowledgement of a safely drained failure. */
int HdrSession_Release(HdrSession *s);
#ifdef __cplusplus
}
#endif
#endif
