#ifndef MAX_FAN_CONTROLLER_H
#define MAX_FAN_CONTROLLER_H
#include "defines.h"

typedef enum
{
    MAX_FAN_CONTROLLER_THRESHOLD = 0x0, // [9:0]
    MAX_FAN_CONTROLLER_FLOOR_THRESHOLD = 0x4, // [9:0]
    MAX_FAN_CONTROLLER_DUTY_CYCLE = 0x8 // [7:0]
} MAX_FAN_CONTROLLER_REGS;

typedef struct 
{
    uint32_t m_base;
    float m_threshold;
    float m_floorThreshold;
    uint32_t m_dutyCycle;
} MAX_FAN_CONTROLLER;

#ifdef __cplusplus
extern "C"
MAX_RESULT maxFanController_Init(MAX_FAN_CONTROLLER* controlRef, uint32_t base, float threshold, float floorThreshold, uint32_t dutyCycle);
MAX_RESULT maxFanController_SetThreshold(MAX_FAN_CONTROLLER* controlRef,  float threshold);
MAX_RESULT maxFanController_SetFloorThreshold(MAX_FAN_CONTROLLER* controlRef, float threshold);
MAX_RESULT maxFanController_SetDutyCycle(MAX_FAN_CONTROLLER* controlRef, uint32_t dutyCycle);
MAX_RESULT maxFanController_ReadReg(MAX_FAN_CONTROLLER* controlRef, MAX_FAN_CONTROLLER_REGS reg, uint32_t* data);
MAX_RESULT maxFanController_WriteReg(MAX_FAN_CONTROLLER* controlRef, MAX_FAN_CONTROLLER_REGS reg, uint32_t data);
MAX_RESULT maxFanController_selfTest(MAX_FAN_CONTROLLER* controlRef);
#endif

#endif //MAX_FAN_CONTROLLER_H