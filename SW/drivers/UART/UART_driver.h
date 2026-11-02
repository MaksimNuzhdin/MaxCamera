#ifndef UART_DRIVER_H_
#define UART_DRIVER_H_
#include "defines.h"
#include "xuartps.h"

typedef struct
{
    uint32_t m_base;
    uint32_t m_baundRate;
    XUartPs* m_Uart_Ps;		
	XUartPs_Config *m_Config;

}UART_DRIVER;
/**
	Config = XUartPs_LookupConfig(BaseAddress);
	if (NULL == Config) 
    {
		return XST_FAILURE;
	}

	Status = XUartPs_CfgInitialize(&Uart_Ps, &Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	XUartPs_SetBaudRate(&Uart_Ps, 115200);

*/
MAX_RESULT UART_driver_init(UART_DRIVER* uart, uint32_t base, uint32_t boundRate, XUartPs* Uart_Ps);
#endif 