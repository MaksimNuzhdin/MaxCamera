#ifndef AXI_IIC_POLLED_H
#define AXI_IIC_POLLED_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Independent, bounded, single-byte register transport for diagnosis.
 * Owns the hardware exclusively. No XIic ISR/callback/instance is used.
 * 200MHz AXI input clock, 7-bit device address, 16-bit register address.
 */
typedef struct
{
    uint32_t base, clock_hz, hold_cycles, timeout_us;
    uint32_t status, interrupts;
    int phase;
} AXI_IIC_POLLED;
int AxiIicPolled_Init(AXI_IIC_POLLED *, uint32_t base, uint32_t hz, uint32_t hold_cycles);
int AxiIicPolled_Read(void *, uint8_t address, uint16_t reg, uint8_t *, uint16_t count);
int AxiIicPolled_Write(void *, uint8_t address, uint16_t reg, const uint8_t *, uint16_t count);
#ifdef __cplusplus
}
#endif
#endif
