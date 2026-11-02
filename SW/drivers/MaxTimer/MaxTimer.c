#include "MaxTimer.h"

XTmrCtr TimerCounter; /* The instance of the Tmrctr Device */

MAX_RESULT maxTimer_init(MAX_TIMER *timerRef, uint32_t base, uint32_t index, uint32_t frequency)
{
    timerRef->m_base = base;
    timerRef->m_timerIndex = index;
    timerRef->m_timerCounterRef = &TimerCounter;
    timerRef->m_frequency = frequency;


    int Status;

    Status = XTmrCtr_Initialize(timerRef->m_timerCounterRef, timerRef->m_base);
    if (Status != XST_SUCCESS) 
    {
		xil_printf("FAIL to init timer counter");
        return MAX_RESULT_FAIL;
	}

    Status = XTmrCtr_SelfTest(timerRef->m_timerCounterRef, timerRef->m_timerIndex);
	if (Status != XST_SUCCESS) 
    {
		xil_printf("Fail to selftest\n\r");
        return MAX_RESULT_FAIL;
	}
    XTmrCtr_SetOptions(timerRef->m_timerCounterRef, timerRef->m_timerIndex,
            XTC_AUTO_RELOAD_OPTION);
    XTmrCtr_SetResetValue(timerRef->m_timerCounterRef, timerRef->m_timerIndex, 0);
    XTmrCtr_Start(timerRef->m_timerCounterRef, timerRef->m_timerIndex);
    return MAX_RESULT_OK;

}

MAX_RESULT maxTImer_getValue(MAX_TIMER *timerRef, uint32_t *value)
{
    *value = XTmrCtr_GetValue(timerRef->m_timerCounterRef, timerRef->m_timerIndex);
    return MAX_RESULT_OK;
}


MAX_RESULT maxTimer_resetTimer(MAX_TIMER *timerRef)
{
    XTmrCtr_Reset(timerRef->m_timerCounterRef, timerRef->m_timerIndex);
    return MAX_RESULT_OK;
}