#ifndef TEMP_CONTROLLER_H_
#define TEMP_CONTROLLER_H_

#include "defines.h"

#include "xgpio.h"
typedef struct
{
    uint32_t m_base;
    XGpio* m_xilGPIORef;
} TEMP_CONTROLLER;

MAX_RESULT temp_driver_init(TEMP_CONTROLLER* controller, uint32_t base, XGpio* gpio_ref);
MAX_RESULT temp_driver_get_temp(TEMP_CONTROLLER* controller, float* temp_result);

#endif // TEMP_CONTROLLER_H_