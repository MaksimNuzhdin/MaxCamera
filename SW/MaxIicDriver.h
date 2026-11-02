#ifndef MAX_IIC_DRIVER_H
#define MAX_IIC_DRIVER_H

#include <stdint.h>
#include "xiic.h"
#include "xiicps.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_IIC_MAX_DATA 256U
#define MAX_IIC_DEFAULT_TIMEOUT_US 100000U

typedef enum {
    MAX_IIC_CAM0 = 0, /* AXI IIC 0, 0xA0020000 */
    MAX_IIC_CAM1 = 1, /* AXI IIC 1, 0xA0030000 */
    MAX_IIC_PS1  = 2  /* PS I2C1, MIO 32..33, 0xFF030000 */
} MAX_IIC_BUS;

typedef enum {
    MAX_IIC_OK = 0,
    MAX_IIC_INVALID_ARGUMENT = -1,
    MAX_IIC_NOT_INITIALIZED = -2,
    MAX_IIC_BUSY = -3,
    MAX_IIC_TIMEOUT = -4,
    MAX_IIC_NACK = -5,
    MAX_IIC_ARBITRATION_LOST = -6,
    MAX_IIC_IO_ERROR = -7
} MAX_IIC_RESULT;

typedef struct {
    MAX_IIC_BUS bus;
    uint32_t clock_hz;
    uint32_t timeout_us;
    int initialized;
    int active;
    int complete;
    MAX_IIC_RESULT error;
    union {
        XIic axi;
        XIicPs ps;
    } controller;
} MAX_IIC;

/* Synchronous, single caller per physical bus. Do not connect these controllers
 * to a GIC handler: BSP interrupt state machines are serviced by polling.
 * AXI camera buses accept 10/50/100 kHz; 50 kHz is verified with OV5640.
 * AXI timing assumes this board's 200 MHz input clock and sets 1 us SDA hold.
 * PS accepts 100/400 kHz. Init requires programmed PL and initialized PS clocks.
 * Use one persistent context per bus; do not reinitialize during a transfer.
 */
MAX_IIC_RESULT MaxIIC_Init(MAX_IIC *iic, MAX_IIC_BUS bus,
                          uint32_t clock_hz, uint32_t timeout_us);

/* slave is an UNshifted 7-bit address (0x08..0x77).
 * reg_bytes: 0 = raw transfer, 1 = 8-bit register, 2 = 16-bit register (MSB first).
 * reg must be 0 for raw transfers, <=255 for 8-bit registers.
 * count: 1..MAX_IIC_MAX_DATA. Read uses repeated START after the register prefix.
 * Write sends register prefix and payload in ONE transaction, followed by STOP.
 * EEPROM page splitting/write-cycle delays and device byte order belong to caller.
 * No automatic retry: a failed write may have partially reached the device.
 * timeout_us bounds the total polling delay across all phases, not CPU overhead.
 */
MAX_IIC_RESULT MaxIIC_Read(MAX_IIC *iic, uint8_t slave, uint16_t reg,
                          uint8_t reg_bytes, uint8_t *data, uint16_t count);
MAX_IIC_RESULT MaxIIC_Write(MAX_IIC *iic, uint8_t slave, uint16_t reg,
                           uint8_t reg_bytes, const uint8_t *data, uint16_t count);

#ifdef __cplusplus
}
#endif
#endif
