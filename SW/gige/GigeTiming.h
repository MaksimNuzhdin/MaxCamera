#ifndef GIGE_TIMING_H
#define GIGE_TIMING_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t GigeTiming_MaxVts(uint32_t clock_hz,uint32_t hts);
uint32_t GigeTiming_MinRate(uint32_t clock_hz,uint32_t hts);
uint32_t GigeTiming_Vts(uint32_t clock_hz,uint32_t hts,uint32_t base_vts,uint32_t rate_millihz);
int GigeTiming_CanServiceSensor(uint32_t rate_millihz,uint64_t elapsed_us);
int GigeTiming_DeferTelemetry(uint32_t payload_bytes,uint32_t rate_millihz,
                             uint32_t software_converted_pixels);
#ifdef __cplusplus
}
#endif
#endif
