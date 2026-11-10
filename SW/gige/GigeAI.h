#ifndef GIGE_AI_H
#define GIGE_AI_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define GIGE_AI_PHYS 0x1f000000U
#define GIGE_AI_BYTES 8192U
#define GIGE_AI_MAGIC 0x3149414dU
#define GIGE_AI_MAX_OBJECTS 32U
/* Little-endian 32-bit words; each producer commits its sequence last. */
enum { AI_MAGIC=0, AI_VERSION, AI_REQUEST, AI_COPY_ACK, AI_RESULT, AI_RESULT_ACK,
       AI_HEARTBEAT, AI_SUPPORTED, AI_CONFIG, AI_MASK, AI_SCORE, AI_PERIOD,
       AI_ERROR, AI_FACE_THRESHOLD, AI_DESCRIPTOR=32, AI_OUTPUT=64, AI_OBJECTS=96 };
/* Descriptor: sequence, config, address, width, height, stride, PFNC,
 * capture timestamp low/high, capture counter. Output: request, config, count,
 * inference microseconds, error. Object: feature, class, score millionths,
 * x, y, width, height, track ID, 32 bytes UTF-8 text (NUL terminated). */
void GigeAI_Init(volatile uint32_t *mailbox);
void GigeAI_Poll(uint64_t now);
/* Optional web channel in reserved words 640..653; R5 remains configuration owner. */
int GigeAI_WebPoll(uint64_t now, int gvcp_owned);
int GigeAI_Pinned(const void *buffer);
void GigeAI_Offer(const void *buffer,uint32_t width,uint32_t height,uint32_t stride,
                  uint32_t pfnc,uint64_t timestamp,uint32_t capture);
int GigeAI_Address(uint32_t address);
uint16_t GigeAI_Read(uint32_t address,uint32_t *value);
uint16_t GigeAI_Write(uint32_t address,uint32_t value);
void GigeAI_RevokeControl(void);
#ifdef __cplusplus
}
#endif
#endif
