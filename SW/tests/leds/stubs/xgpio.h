#ifndef TEST_XGPIO_H
#define TEST_XGPIO_H
#include <stdint.h>
typedef struct {uint32_t value,direction,base;} XGpio;
int XGpio_Initialize(XGpio *,uint32_t);
void XGpio_SetDataDirection(XGpio *,unsigned,uint32_t);
void XGpio_DiscreteWrite(XGpio *,unsigned,uint32_t);
#endif
