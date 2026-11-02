#include "TempController.h"

MAX_RESULT temp_driver_init(TEMP_CONTROLLER* controller, uint32_t base, XGpio* gpio_ref)
{
    controller->m_base = base;
    controller->m_xilGPIORef = gpio_ref;
    int Status;
    Status = XGpio_Initialize(controller->m_xilGPIORef, controller->m_base );

	if (Status != XST_SUCCESS) 
    {
		xil_printf("Gpio Initialization Failed\r\n");
    }

	XGpio_SetDataDirection(controller->m_xilGPIORef, 1, 0 );
    
    return MAX_RESULT_OK;
}
MAX_RESULT temp_driver_get_temp(TEMP_CONTROLLER* controller, float* temp_result)
{
    uint32_t tempReg = XGpio_DiscreteRead(controller->m_xilGPIORef, 1);
    *temp_result = ((tempReg << 2) * 503.975 / 4096) - 273.15;
    return MAX_RESULT_OK;
}