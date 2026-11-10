#ifndef GIGE_HTTP_H
#define GIGE_HTTP_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Entire region is already reserved/no-map in the A53 device tree. */
#define GIGE_HTTP_CONTROL 0x1f004000U
#define GIGE_HTTP_REQUEST 0x1f010000U
#define GIGE_HTTP_RESPONSE 0x1f500000U
#define GIGE_HTTP_REQUEST_MAX (4U*1024U*1024U+8192U)
#define GIGE_HTTP_RESPONSE_MAX (128U*1024U)
#define GIGE_HTTP_MAGIC 0x31505448U
void GigeHttp_Init(volatile uint32_t *control, volatile uint8_t *request,
                   volatile uint8_t *response);
void GigeHttp_Poll(uint64_t now_us, uint32_t ip, int streaming, unsigned tx_free);
#ifdef __cplusplus
}
#endif
#endif
