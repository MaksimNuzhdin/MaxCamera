//#include <cstdint>
#include <xparameters.h>
#include <xil_printf.h>
#include <xil_io.h>
// drivers path https://github.com/Xilinx/embeddedsw/tree/master/XilinxProcessorIPLib/drivers

typedef uint8_t MAX_RESULT;
#define MAX_RESULT_OK 0
#define MAX_RESULT_FAIL -1

#define MAX_PRINTF xil_printf
#define MAX_HW_READ(base, reg) Xil_In32(base + reg)
#define MAX_HW_WRITE(base, reg, data) Xil_Out32(base + reg, data)

#define TIMER_COUNTER_0	 0

#define XTMRCTR_BASE	        XPAR_XTMRCTR_0_BASEADDR
#define	LEDS_CONTROLLER_BASE	XPAR_LEDS_CONTROLLER_BASEADDR
#define	XUARTPS_BASEADDRESS		XPAR_XUARTPS_0_BASEADDR
#define TEMP_CONTROLLER_BASE    XPAR_TEMP_CONTROLLER_BASEADDR
#define MAX_FAN_CONTROLLER_BASR XPAR_MAX_FAN_CONTROLLER_0_BASEADDR
#define MAX_IIC_0_BASE XPAR_AXI_IIC_0_BASEADDR
#define MAX_IIC_1_BASE XPAR_AXI_IIC_1_BASEADDR
#define IIC_PS_BASE XPAR_XIICPS_0_BASEADDR

#define PROJECT_NAME "MaxCamera"
#define FW_VERSION_MAJOR 1
#define FW_VERSION_MINOR 1