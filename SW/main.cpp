#include "TempController.h"
#include "defines.h"
#include "sleep.h"
#include "MaxTimer.h"
MAX_TIMER g_maxTimer;
 
#include "LedsController.h"
LEDS_CONTROLLER g_ledsController;

#include "UART_driver.h"
UART_DRIVER g_uart;
XUartPs Uart_Ps;		/* The instance of the UART Driver */

#include "MaxFanController.h"
MAX_FAN_CONTROLLER g_fanController;



#include "xgpio.h"
TEMP_CONTROLLER g_tempController;
XGpio g_tempGpio; /* The Instance of the GPIO Driver */

#include "MaxIicDriver.h"
#include "CameraDiagnostics.h"
#include "Ov5640Board.h"
#include "gige/GigeVisionBoard.h"
#include "gige/GigeCamera.h"
#include "gige/GigeVisionUnitDefines.h"
#if !MAX_CAMERA_GIGE || !MAX_CAMERA_GIGE_GPIO_SCCB
MAX_IIC g_MaxIicRef_0;
MAX_IIC g_MaxIicRef_1;
MAX_IIC g_MaxIicPs;
#endif

////////////


#define LED_CHANNEL 1

void initPeriphirals()
{
    maxTimer_init(&g_maxTimer, XTMRCTR_BASE, TIMER_COUNTER_0, XPAR_XTMRCTR_0_CLOCK_FREQUENCY);
    
    leds_controller_init(&g_ledsController, LEDS_CONTROLLER_BASE, 
                                0, LED_CHANNEL, LEDS_MODE_STATUS);
    

    UART_driver_init(&g_uart, XUARTPS_BASEADDRESS, 115200, &Uart_Ps);

    MAX_PRINTF("*****************************************************\n\r");
    MAX_PRINTF("*** MaxCamera: FW Version: %d.%d\n\r", FW_VERSION_MAJOR, FW_VERSION_MINOR);
    MAX_PRINTF("*****************************************************\n\r");
    temp_driver_init(&g_tempController, TEMP_CONTROLLER_BASE, &g_tempGpio);
    maxFanController_Init(&g_fanController, MAX_FAN_CONTROLLER_BASR, GIGE_UNIT_FAN_START_C, GIGE_UNIT_FAN_STOP_C, GIGE_UNIT_FAN_DUTY);

    // GPIO SCCB builds have no AXI IIC devices to initialize.
#if !MAX_CAMERA_GIGE || !MAX_CAMERA_GIGE_GPIO_SCCB
    MAX_IIC_RESULT iicStatus = MaxIIC_Init(&g_MaxIicRef_0, MAX_IIC_CAM0,
                                          50000U, MAX_IIC_DEFAULT_TIMEOUT_US);
    if (iicStatus != MAX_IIC_OK) MAX_PRINTF("I2C CAM0 init failed: %d\r\n", iicStatus);
    iicStatus = MaxIIC_Init(&g_MaxIicRef_1, MAX_IIC_CAM1,
                           50000U, MAX_IIC_DEFAULT_TIMEOUT_US);
    if (iicStatus != MAX_IIC_OK) MAX_PRINTF("I2C CAM1 init failed: %d\r\n", iicStatus);
    iicStatus = MaxIIC_Init(&g_MaxIicPs, MAX_IIC_PS1,
                           100000U, MAX_IIC_DEFAULT_TIMEOUT_US);
    if (iicStatus != MAX_IIC_OK) MAX_PRINTF("I2C PS1 init failed: %d\r\n", iicStatus);
#endif


}

///////////
int main()
{
    uint32_t secondCounter = 0;
#if !MAX_CAMERA_GIGE
    uint32_t timer_value = 0;
#endif

    initPeriphirals();
    usleep(30000);
#if MAX_CAMERA_GIGE
    int network_status=GigeVisionBoard_Init();
    MAX_PRINTF("GIGE startup status=%d\r\n",network_status);
#else
    CameraDiagnostics_Run(&g_MaxIicRef_0, &g_MaxIicRef_1);
    Ov5640Board_Init(&g_MaxIicRef_0);
#endif
    MAX_PRINTF("MAIN LOOP STARTED \n\r");
    float tempValue = 0.0;
    uint32_t tempValueFract = 0;
#if MAX_CAMERA_GIGE
    uint64_t next_housekeeping=GigeVisionBoard_NowUs()+1000000U;
    uint64_t next_heartbeat=GigeVisionBoard_NowUs()+GIGE_UNIT_LED_HALF_PERIOD_US;
#endif
    while(1)
    {
#if MAX_CAMERA_GIGE
        GigeVisionBoard_Poll();
        uint64_t now=GigeVisionBoard_NowUs();
        leds_controller_set_status(&g_ledsController,GigeCamera_Ready(),GigeVisionBoard_Ready());
        leds_controller_stream(&g_ledsController,GigeCamera_IndependentStreamLed(),
                               GigeVisionBoard_StreamIndicatorActive(),now);
        if(now>=next_heartbeat) {
            next_heartbeat=now+GIGE_UNIT_LED_HALF_PERIOD_US;
            leds_controller_process(&g_ledsController);
        }
        if(now >= next_housekeeping)
#else
        maxTImer_getValue(&g_maxTimer, &timer_value);
        if(timer_value >= XPAR_XTMRCTR_0_CLOCK_FREQUENCY)
#endif
        {
#if MAX_CAMERA_GIGE
            next_housekeeping=now+1000000U;
#else
            maxTimer_resetTimer(&g_maxTimer);
#endif
            secondCounter++;
#if !MAX_CAMERA_GIGE
            leds_controller_process(&g_ledsController);
#endif
            temp_driver_get_temp(&g_tempController, &tempValue);
            tempValueFract = (tempValue - (uint32_t)tempValue)*100;
            uint32_t dutyCycle = 0;
            maxFanController_ReadReg(&g_fanController, MAX_FAN_CONTROLLER_DUTY_CYCLE, &dutyCycle);


#if MAX_CAMERA_GIGE
            if(!GigeVisionBoard_Streaming())
#endif
            MAX_PRINTF("Seconds %d temp: %d.%d \n\r", secondCounter, 
            (uint32_t)tempValue, tempValueFract);

        }
    }
    return 0;
}

