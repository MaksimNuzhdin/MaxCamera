#ifndef AXI_GPIO_SCCB_H
#define AXI_GPIO_SCCB_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Matching GPIO-SCCB FPGA only: GPIO0=SCL, GPIO1=SDA. DATA remains zero;
 * TRI releases a line. Exclusive master, bounded clock stretching, no retry. */
typedef struct {
    uint32_t base, half_us, timeout_us, status, interrupts;
    int phase, ready;
} AXI_GPIO_SCCB;
int AxiGpioSccb_Init(AXI_GPIO_SCCB *,uint32_t base,uint32_t hz);
int AxiGpioSccb_Read(void *,uint8_t address,uint16_t reg,uint8_t *,uint16_t count);
int AxiGpioSccb_Write(void *,uint8_t address,uint16_t reg,const uint8_t *,uint16_t count);
#ifdef __cplusplus
}
#endif
#endif
