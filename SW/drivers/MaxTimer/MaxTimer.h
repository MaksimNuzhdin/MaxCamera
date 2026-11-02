#ifndef MAX_TIMER_H
#define MAX_TIMER_H
#include "defines.h"
#include <sys/_stdint.h>
#include <xtmrctr.h> // https://github.com/Xilinx/embeddedsw/blob/master/XilinxProcessorIPLib/drivers/tmrctr/examples/xtmrctr_polled_example.c

typedef struct
{
    uint32_t m_base;
    uint32_t m_timerIndex;
    XTmrCtr* m_timerCounterRef; 
    uint32_t m_frequency;
} MAX_TIMER ;

#ifdef __cplusplus
extern "C"
{
#endif
MAX_RESULT maxTimer_init(MAX_TIMER *timerRef, uint32_t base, uint32_t index, uint32_t frequency);
MAX_RESULT maxTImer_getValue(MAX_TIMER *timerRef, uint32_t *value);
MAX_RESULT maxTimer_resetTimer(MAX_TIMER *timerRef);
#ifdef __cplusplus
}
#endif

#endif