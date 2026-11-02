#include "MaxFanController.h"
#include <math.h>

static uint32_t MAX_FAN_CONTROLLER_DEBUG = 1;

MAX_RESULT maxFanController_Init(MAX_FAN_CONTROLLER* controlRef, uint32_t base, float threshold, float floorThreshold, uint32_t dutyCycle)
{
    controlRef->m_base = base;
    controlRef->m_threshold = threshold;
    controlRef->m_floorThreshold = floorThreshold;
    controlRef->m_dutyCycle = dutyCycle;
    maxFanController_SetThreshold(controlRef, controlRef->m_threshold);
    maxFanController_SetFloorThreshold(controlRef, controlRef->m_floorThreshold);
    maxFanController_SetDutyCycle(controlRef, controlRef->m_dutyCycle);

    return MAX_RESULT_OK;
}
MAX_RESULT maxFanController_ReadReg(MAX_FAN_CONTROLLER* controlRef, MAX_FAN_CONTROLLER_REGS reg, uint32_t* data)
{
    *data = MAX_HW_READ(controlRef->m_base, reg);
    return MAX_RESULT_OK;
}
MAX_RESULT maxFanController_WriteReg(MAX_FAN_CONTROLLER* controlRef, MAX_FAN_CONTROLLER_REGS reg, uint32_t data)
{
    MAX_HW_WRITE(controlRef->m_base, reg, data);
    return MAX_RESULT_OK;
}

MAX_RESULT maxFanController_SetThreshold(MAX_FAN_CONTROLLER* controlRef,  float threshold)
{
    uint32_t tempReg = (uint32_t)round((threshold + 273.15) * 4096.0 / (4.0 * 503.975));
    MAX_PRINTF("SET THRESHOLD %d\n\r", tempReg);
    return maxFanController_WriteReg(controlRef, MAX_FAN_CONTROLLER_THRESHOLD, tempReg);
}
MAX_RESULT maxFanController_SetFloorThreshold(MAX_FAN_CONTROLLER* controlRef, float threshold)
{
    uint32_t tempReg = (uint32_t)round((threshold + 273.15) * 4096.0 / (4.0 * 503.975));
    MAX_PRINTF("SET FLOOR THRESHOLD %d\n\r", tempReg);
    return maxFanController_WriteReg(controlRef, MAX_FAN_CONTROLLER_FLOOR_THRESHOLD, tempReg);
}
MAX_RESULT maxFanController_SetDutyCycle(MAX_FAN_CONTROLLER* controlRef, uint32_t dutyCycle)
{
    MAX_PRINTF("SSetDutyCycle %d\n\r", dutyCycle);
    return maxFanController_WriteReg(controlRef, MAX_FAN_CONTROLLER_DUTY_CYCLE, dutyCycle);
}

MAX_RESULT maxFanController_selfTest(MAX_FAN_CONTROLLER* controlRef)
{
    MAX_RESULT result = MAX_RESULT_OK;
    uint32_t dummy_read_threshold = 0x20;
    uint32_t dummy_read_f_threshold = 0x30;
    uint32_t dummy_read_duty = 0x10;

    uint32_t read_threshold = 0;
    uint32_t read_f_threshold = 0;
    uint32_t read_duty = 0;


    
    maxFanController_WriteReg(controlRef, MAX_FAN_CONTROLLER_THRESHOLD, dummy_read_threshold);
    maxFanController_WriteReg(controlRef, MAX_FAN_CONTROLLER_FLOOR_THRESHOLD, dummy_read_f_threshold);
    maxFanController_WriteReg(controlRef, MAX_FAN_CONTROLLER_DUTY_CYCLE, dummy_read_duty);
    maxFanController_ReadReg(controlRef, MAX_FAN_CONTROLLER_THRESHOLD, &read_threshold);
    maxFanController_ReadReg(controlRef, MAX_FAN_CONTROLLER_FLOOR_THRESHOLD, &read_f_threshold);
    maxFanController_ReadReg(controlRef, MAX_FAN_CONTROLLER_DUTY_CYCLE, &read_duty);
    MAX_PRINTF("READ VALUES: threshold: %d, flood %d duty: %d\n\r",read_threshold, read_f_threshold, read_duty);
    
    if((dummy_read_threshold != read_threshold) ||
    dummy_read_f_threshold != read_f_threshold ||
    dummy_read_duty != read_duty)
    {
        result = MAX_RESULT_FAIL;    
    }
    return result;

}