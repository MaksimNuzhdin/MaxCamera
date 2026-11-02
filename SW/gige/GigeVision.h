#ifndef GIGE_VISION_H
#define GIGE_VISION_H
#include <stddef.h>
#include <stdint.h>
#include "GigeVisionDefaults.h"
#include "GigeImage.h"
#ifdef __cplusplus
extern "C" {
#endif
#define GIGE_CONTROL_MAX 524U
#define GIGE_ACK_MAX 524U
/* IPv4 values here use host arithmetic, e.g. 0x0a000007. */
typedef struct { uint32_t ip; uint16_t port; } GigePeer;
typedef struct {
    void *user;
    int (*acquisition)(void *user, int enable); /* bounded; nonzero = failure */
    int (*send_stream)(void *user, uint32_t ip, uint16_t port,
                       const uint8_t *data, size_t length, int df);
    uint16_t (*feature_read)(void *user,uint32_t address,uint32_t *value);
    uint16_t (*feature_write)(void *user,uint32_t address,uint32_t value);
    uint8_t *(*stream_buffer)(void *user); /* optional synchronous TX staging */
} GigeIO;
typedef struct {
    GigePeer peer;
    uint64_t time;
    uint16_t request_size, ack_size;
    uint8_t request[GIGE_CONTROL_MAX], ack[GIGE_ACK_MAX];
} GigeReplay;
#define GIGE_HISTORY_COUNT 8U
#define GIGE_RESEND_QUEUE 8U
typedef struct {
    const uint8_t *data;
    uint32_t size,row_bytes,stride,width,height,format,compression,packet_size,sent;
    uint16_t block;
    uint64_t timestamp;
} GigeFrameHistory;
typedef struct {uint16_t block;uint32_t first,last;} GigeResend;
typedef struct {
    uint32_t version,mac_high,mac_low,mode,ip,mask,gateway;
    char user_name[16],serial[16];
    uint32_t provisioned;
} GigeDeviceConfig;
typedef struct {
    GigeIO io;
    uint8_t bootstrap[4096];
    uint32_t heartbeat, privilege, destination, packet_size, packet_delay, locked;
    uint32_t width,height,pixel_format,payload_size;
    uint32_t compression,frame_size,frame_row_bytes,frame_stride;
    uint16_t destination_port;
    GigePeer owner;
    uint64_t owner_time, now, timestamp_origin, timestamp_latch, factory_until;
    uint8_t active, test_pending, force_ip_pending;
    uint32_t force_ip, force_mask, force_gateway;
    GigeReplay replay[8];
    unsigned replay_next;
    const uint8_t *frame;
    uint32_t frame_offset, packet_id;
    uint16_t block_id;
    uint64_t frame_timestamp, next_packet,frame_submitted;
    uint32_t frame_send_us;
    uint32_t frames_sent, packets_sent, send_errors, frames_aborted;
    GigeDeviceConfig config;
    uint8_t config_dirty;
    uint64_t config_changed;
    GigeFrameHistory *current_history;
    GigeFrameHistory history[GIGE_HISTORY_COUNT];
    GigeResend resend[GIGE_RESEND_QUEUE];
    unsigned history_next,resend_head,resend_count,resend_turn;
    uint32_t resend_requests,resent_packets,resend_unavailable,resend_overflow;
} GigeVision;
/* Invalidate BEFORE DMA/CPU writes a retained buffer. No frame copies required. */
void GigeVision_ForgetBuffer(GigeVision*,const uint8_t*);
void GigeVision_ClearHistory(GigeVision*);
void GigeVision_Release(GigeVision*);
int GigeVision_ConfigValid(const GigeDeviceConfig*);
void GigeVision_ApplyConfig(GigeVision*,const GigeDeviceConfig*);
int GigeVision_FactoryAccess(const GigeVision *g);
void GigeVision_LockFactory(GigeVision *g);
void GigeVision_Init(GigeVision *g, const GigeIO *io);
void GigeVision_SetAddress(GigeVision *g, uint32_t ip, uint32_t mask, uint32_t gateway,
                           uint32_t configuration);
void GigeVision_Tick(GigeVision *g, uint64_t now_us);
/* Convert monotonic board time in the current epoch. Latch at DMA arm, before
 * a later TimestampReset can change the origin. Valid for now_us >= origin. */
uint64_t GigeVision_TimestampAt(const GigeVision *g, uint64_t now_us);
/* Returns ACK byte count, zero for ignored packet. reply_broadcast initialized by caller. */
size_t GigeVision_Control(GigeVision *g, GigePeer peer, const uint8_t *request,
                          size_t length, uint8_t ack[GIGE_ACK_MAX], int *reply_broadcast);
/* Submission takes a device-clock timestamp already latched at capture start,
 * not absolute board time. Preserve it across reset and in retained Resend data. */
int GigeVision_SubmitFrameStrided(GigeVision*,const uint8_t*,uint32_t,uint32_t,uint64_t);
int GigeVision_SubmitFrameSized(GigeVision*,const uint8_t*,uint32_t,uint64_t);
int GigeVision_SubmitFrame(GigeVision *g, const uint8_t *mono, uint64_t timestamp_us);
/* Send at most one packet; caller must retain frame until FrameBusy returns false. */
void GigeVision_StreamPoll(GigeVision *g);
int GigeVision_FrameBusy(const GigeVision *g);
uint16_t Gige_Get16(const uint8_t *p);
uint32_t Gige_Get32(const uint8_t *p);
void Gige_Put16(uint8_t *p, uint16_t v);
void Gige_Put32(uint8_t *p, uint32_t v);
#ifdef __cplusplus
}
#endif
#endif
