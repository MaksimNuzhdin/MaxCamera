#ifndef LEDS_CONTROLLER_H_
#define LEDS_CONTROLLER_H_
#include "defines.h"
#include "xgpio.h"
typedef enum {
    LEDS_MODE_OFF=0, LEDS_MODE_BLINK_ALL=1, LEDS_MODE_FORWARD_BACKWARD=2,
    LEDS_MODE_STATUS=3
} LEDS_MODE;
typedef struct {
    uint32_t m_base;
    LEDS_MODE m_mode;
    uint32_t m_mask,m_channel,m_correntState;
    unsigned m_direction;
    int m_streaming;
    uint64_t m_stream_tick;
    XGpio m_gpio;
} LEDS_CONTROLLER;
#ifdef __cplusplus
extern "C" {
#endif
MAX_RESULT leds_controller_init(LEDS_CONTROLLER *,uint32_t base,uint32_t mask,uint32_t channel,LEDS_MODE);
MAX_RESULT leds_controller_set_mode(LEDS_CONTROLLER *,LEDS_MODE);
MAX_RESULT leds_controller_process(LEDS_CONTROLLER *);
/* Logical bit0 heartbeat, bit1 sensor initialized, bit2 GVCP reachable.
 * process() toggles only heartbeat in STATUS mode; readiness changes immediately. */
MAX_RESULT leds_controller_set_status(LEDS_CONTROLLER *,int sensor_ready,int gige_ready);
/* Enable only when the fourth LED is electrically independent of video reset. */
MAX_RESULT leds_controller_stream(LEDS_CONTROLLER *,int independent,int streaming,uint64_t now_us);
#ifdef __cplusplus
}
#endif
#endif
