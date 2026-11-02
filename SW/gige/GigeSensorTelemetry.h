#ifndef GIGE_SENSOR_TELEMETRY_H
#define GIGE_SENSOR_TELEMETRY_H
#include "../AxiGpioSccbAsync.h"
#ifdef __cplusplus
extern "C" {
#endif
#define GIGE_TELEMETRY_REGISTERS 14U
typedef struct {
    SccbAsync *bus;
    void *user;
    void (*publish)(void *user,uint16_t reg,uint8_t value);
    uint64_t next_due, last_now;
    uint32_t interval_us;
    uint16_t valid, error_register;
    uint8_t values[GIGE_TELEMETRY_REGISTERS], address, index, pending_index;
    int pending, paused, last_error, time_valid;
} GigeSensorTelemetry;
/* The bus must be initialized and reserved for this scheduler while active.
 * Publication is byte-level, as in the existing telemetry cache; it does not
 * claim a sensor-latched multi-register exposure/WB snapshot. */
int GigeSensorTelemetry_Init(GigeSensorTelemetry *,SccbAsync *,uint8_t address,
    uint32_t interval_us,void *user,void (*publish)(void *,uint16_t,uint8_t));
/* May start a read only when allowed. An outstanding read always progresses.
 * Pausing/cancelling must finish before synchronous access or sensor reset. */
void GigeSensorTelemetry_Poll(GigeSensorTelemetry *,uint64_t now,int allowed);
void GigeSensorTelemetry_Pause(GigeSensorTelemetry *,uint64_t now);
void GigeSensorTelemetry_Resume(GigeSensorTelemetry *,uint64_t now);
int GigeSensorTelemetry_Read(const GigeSensorTelemetry *,uint16_t reg,uint8_t *value);
#ifdef __cplusplus
}
#endif
#endif
