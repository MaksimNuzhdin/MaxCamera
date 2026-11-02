#ifndef TEST_GPIO_IO_H
#define TEST_GPIO_IO_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t Xil_In32(uint32_t);
void Xil_Out32(uint32_t,uint32_t);
#ifdef __cplusplus
}
#endif
#endif
