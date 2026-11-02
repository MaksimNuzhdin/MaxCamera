#include "UART_driver.h"

XUartPs_Config* Config;
MAX_RESULT UART_driver_init(UART_DRIVER* uart, uint32_t base, uint32_t boundRate, XUartPs* Uart_Ps)
{
    uart->m_base = base;
    uart->m_baundRate = boundRate;
    uart->m_Uart_Ps = Uart_Ps;
    Config = XUartPs_LookupConfig(uart->m_base);
	if (NULL == Config) 
    {
		return MAX_RESULT_FAIL;
	}

	
	int Status = XUartPs_CfgInitialize(uart->m_Uart_Ps, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) 
    {
		return MAX_RESULT_FAIL;
	}

	XUartPs_SetBaudRate(uart->m_Uart_Ps, uart->m_baundRate);
    return MAX_RESULT_OK;
}
