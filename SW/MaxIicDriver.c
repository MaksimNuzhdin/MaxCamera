#include "MaxIicDriver.h"
#include "xparameters.h"
#include "sleep.h"
#include <stddef.h>
#include <string.h>

/* Board design: AXI IIC input clock is 200 MHz. Timing register layout and
 * standard-mode setup times match the Linux i2c-xiic controller driver.
 * A 200-cycle SDA hold was verified with this OV5640 module at 50 kHz.
 */
static void axi_timing(u32 base, uint32_t frequency)
{
    const u32 input_hz = 200000000U;
    const u32 mhz = input_hz / 1000000U;
    const u32 half = (input_hz + 2U * frequency - 1U) / (2U * frequency) - 8U;
    const u32 values[8] = {5700U * mhz / 1000U - 1U,
        5000U * mhz / 1000U - 1U, 4300U * mhz / 1000U - 1U,
        550U * mhz / 1000U - 1U, 5000U * mhz / 1000U - 1U,
        half, half, 200U};
    unsigned int index;
    for (index = 0; index < 8; ++index) {
        XIic_WriteReg(base, 0x128U + 4U * index, values[index]);
    }
}

static void axi_complete(void *ref, int remaining)
{
    MAX_IIC *iic = (MAX_IIC *)ref;
    if (remaining != 0) {
        iic->error = MAX_IIC_IO_ERROR;
    } else {
        iic->complete = 1;
    }
}

static void axi_status(void *ref, int events)
{
    MAX_IIC *iic = (MAX_IIC *)ref;
    if (events & XII_ARB_LOST_EVENT) {
        iic->error = MAX_IIC_ARBITRATION_LOST;
    } else if (events & XII_SLAVE_NO_ACK_EVENT) {
        iic->error = MAX_IIC_NACK;
    }
}

static void ps_status(void *ref, u32 events)
{
    MAX_IIC *iic = (MAX_IIC *)ref;
    if (events & XIICPS_EVENT_ARB_LOST) {
        iic->error = MAX_IIC_ARBITRATION_LOST;
    } else if (events & XIICPS_EVENT_NACK) {
        iic->error = MAX_IIC_NACK;
    } else if (events & XIICPS_EVENT_TIME_OUT) {
        iic->error = MAX_IIC_TIMEOUT;
    } else if (events & (XIICPS_EVENT_ERROR | XIICPS_EVENT_RX_OVR |
                         XIICPS_EVENT_TX_OVR | XIICPS_EVENT_RX_UNF)) {
        iic->error = MAX_IIC_IO_ERROR;
    }
    if (events & (XIICPS_EVENT_COMPLETE_SEND | XIICPS_EVENT_COMPLETE_RECV)) {
        iic->complete = 1;
    }
}

static MAX_IIC_RESULT configure(MAX_IIC *iic)
{
    int status;
    if (iic->bus == MAX_IIC_PS1) {
        XIicPs_Config *cfg = XIicPs_LookupConfig(XPAR_XIICPS_0_BASEADDR);
        if (cfg == NULL) return MAX_IIC_IO_ERROR;
        status = XIicPs_CfgInitialize(&iic->controller.ps, cfg, cfg->BaseAddress);
        if (status != XST_SUCCESS) return MAX_IIC_IO_ERROR;
        XIicPs_SetStatusHandler(&iic->controller.ps, iic, ps_status);
        status = XIicPs_SetSClk(&iic->controller.ps, iic->clock_hz);
    } else {
        u32 base = (iic->bus == MAX_IIC_CAM0) ?
                   XPAR_AXI_IIC_0_BASEADDR : XPAR_AXI_IIC_1_BASEADDR;
        status = XIic_Initialize(&iic->controller.axi, base);
        if (status != XST_SUCCESS) return MAX_IIC_IO_ERROR;
        axi_timing(base, iic->clock_hz);
        XIic_SetSendHandler(&iic->controller.axi, iic, axi_complete);
        XIic_SetRecvHandler(&iic->controller.axi, iic, axi_complete);
        XIic_SetStatusHandler(&iic->controller.axi, iic, axi_status);
        /* XIic leaves arbitration/bus-release handlers as assert stubs until
         * this optional module is included, even for a single bus master.
         */
        XIic_MultiMasterInclude();
        status = XIic_Start(&iic->controller.axi);
        if (status == XST_SUCCESS) {
            /* Start enables addressed-as-slave by default. This wrapper owns
             * the controller exclusively as a master and has no slave buffers
             * or slave callbacks; never dispatch its optional assert stubs.
             */
            XIic_WriteIier(base, XIic_ReadIier(base) &
                          ~(XIIC_INTR_AAS_MASK | XIIC_INTR_NAAS_MASK));
        }
    }
    return status == XST_SUCCESS ? MAX_IIC_OK : MAX_IIC_IO_ERROR;
}

MAX_IIC_RESULT MaxIIC_Init(MAX_IIC *iic, MAX_IIC_BUS bus,
                          uint32_t clock_hz, uint32_t timeout_us)
{
    MAX_IIC_RESULT result;
    if (iic == NULL || (bus != MAX_IIC_CAM0 && bus != MAX_IIC_CAM1 &&
        bus != MAX_IIC_PS1) || timeout_us == 0 ||
        (bus == MAX_IIC_PS1 ? (clock_hz != 100000U && clock_hz != 400000U) :
         (clock_hz != 10000U && clock_hz != 50000U && clock_hz != 100000U))) {
        return MAX_IIC_INVALID_ARGUMENT;
    }
    memset(iic, 0, sizeof(*iic));
    iic->bus = bus;
    iic->clock_hz = clock_hz;
    iic->timeout_us = timeout_us;
    result = configure(iic);
    iic->initialized = (result == MAX_IIC_OK);
    return result;
}

static int bus_busy(MAX_IIC *iic)
{
    return iic->bus == MAX_IIC_PS1 ? XIicPs_BusIsBusy(&iic->controller.ps) :
                                   XIic_IsIicBusy(&iic->controller.axi);
}

static void service(MAX_IIC *iic)
{
    if (iic->bus == MAX_IIC_PS1) {
        XIicPs_MasterInterruptHandler(&iic->controller.ps);
    } else {
        XIic_InterruptHandler(&iic->controller.axi);
    }
}

static MAX_IIC_RESULT delay_poll(uint32_t *budget)
{
    if (*budget == 0) return MAX_IIC_TIMEOUT;
    usleep(1);
    --*budget;
    return MAX_IIC_OK;
}

static MAX_IIC_RESULT wait_idle(MAX_IIC *iic, uint32_t *budget)
{
    while (bus_busy(iic)) {
        if (delay_poll(budget) != MAX_IIC_OK) return MAX_IIC_TIMEOUT;
    }
    return MAX_IIC_OK;
}

static MAX_IIC_RESULT wait_complete(MAX_IIC *iic, uint32_t *budget)
{
    for (;;) {
        service(iic);
        if (iic->error != MAX_IIC_OK) return iic->error;
        if (iic->complete) return MAX_IIC_OK;
        if (delay_poll(budget) != MAX_IIC_OK) return MAX_IIC_TIMEOUT;
    }
}

static void repeated_start(MAX_IIC *iic, int enable)
{
    if (iic->bus == MAX_IIC_PS1) {
        if (enable) {
            (void)XIicPs_SetOptions(&iic->controller.ps, XIICPS_REP_START_OPTION);
        } else {
            (void)XIicPs_ClearOptions(&iic->controller.ps, XIICPS_REP_START_OPTION);
        }
    } else {
        XIic_SetOptions(&iic->controller.axi,
                        enable ? XII_REPEATED_START_OPTION : 0);
    }
}

static MAX_IIC_RESULT transfer(MAX_IIC *iic, uint8_t slave, uint8_t *data,
                               uint16_t count, int receive, uint32_t *budget)
{
    iic->complete = 0;
    iic->error = MAX_IIC_OK;
    if (iic->bus == MAX_IIC_PS1) {
        if (receive) XIicPs_MasterRecv(&iic->controller.ps, data, count, slave);
        else XIicPs_MasterSend(&iic->controller.ps, data, count, slave);
    } else {
        int status;
        XIic_SetAddress(&iic->controller.axi, XII_ADDR_TO_SEND_TYPE, slave);
        status = receive ? XIic_MasterRecv(&iic->controller.axi, data, count) :
                           XIic_MasterSend(&iic->controller.axi, data, count);
        if (status != XST_SUCCESS) {
            return status == XST_IIC_BUS_BUSY ? MAX_IIC_BUSY : MAX_IIC_IO_ERROR;
        }
    }
    return wait_complete(iic, budget);
}

static MAX_IIC_RESULT validate(MAX_IIC *iic, uint8_t slave, uint16_t reg,
                               uint8_t reg_bytes, const uint8_t *data,
                               uint16_t count)
{
    if (iic == NULL || data == NULL || slave < 0x08 || slave > 0x77 ||
        count == 0 || count > MAX_IIC_MAX_DATA || reg_bytes > 2 ||
        (reg_bytes == 0 && reg != 0) || (reg_bytes == 1 && reg > 0xff)) {
        return MAX_IIC_INVALID_ARGUMENT;
    }
    if (!iic->initialized) return MAX_IIC_NOT_INITIALIZED;
    if (iic->active) return MAX_IIC_BUSY;
    return MAX_IIC_OK;
}

static void prefix(uint8_t *buffer, uint16_t reg, uint8_t reg_bytes)
{
    if (reg_bytes == 2) {
        buffer[0] = (uint8_t)(reg >> 8);
        buffer[1] = (uint8_t)reg;
    } else if (reg_bytes == 1) {
        buffer[0] = (uint8_t)reg;
    }
}

static MAX_IIC_RESULT finish(MAX_IIC *iic, MAX_IIC_RESULT result)
{
    if (result != MAX_IIC_OK) {
        /* Abort before stack buffers go out of scope, releasing controller HOLD.
         * Reset cannot free SDA held low externally; the next call times out.
         * Do not retry a potentially partially completed device write.
         */
        if (iic->bus == MAX_IIC_PS1) {
            XIicPs_Reset(&iic->controller.ps);
        } else {
            XIic_Reset(&iic->controller.axi);
            /* Hardware reset does not clear XIic.IsStarted. Stop after reset
             * releases the software state so CfgInitialize accepts the instance.
             */
            if (XIic_Stop(&iic->controller.axi) != XST_SUCCESS) {
                iic->initialized = 0;
                iic->active = 0;
                return result;
            }
        }
        iic->initialized = (configure(iic) == MAX_IIC_OK);
    }
    iic->active = 0;
    return result;
}

MAX_IIC_RESULT MaxIIC_Write(MAX_IIC *iic, uint8_t slave, uint16_t reg,
                           uint8_t reg_bytes, const uint8_t *data, uint16_t count)
{
    uint8_t buffer[MAX_IIC_MAX_DATA + 2U];
    uint32_t budget;
    MAX_IIC_RESULT result = validate(iic, slave, reg, reg_bytes, data, count);
    if (result != MAX_IIC_OK) return result;
    iic->active = 1;
    budget = iic->timeout_us;
    /* No transfer was started on a busy bus; do not reset another master's bus. */
    result = wait_idle(iic, &budget);
    if (result != MAX_IIC_OK) { iic->active = 0; return result; }
    prefix(buffer, reg, reg_bytes);
    memcpy(buffer + reg_bytes, data, count);
    repeated_start(iic, 0);
    result = transfer(iic, slave, buffer, count + reg_bytes, 0, &budget);
    if (result == MAX_IIC_OK) result = wait_idle(iic, &budget);
    return finish(iic, result);
}

MAX_IIC_RESULT MaxIIC_Read(MAX_IIC *iic, uint8_t slave, uint16_t reg,
                          uint8_t reg_bytes, uint8_t *data, uint16_t count)
{
    uint8_t buffer[2];
    uint32_t budget;
    MAX_IIC_RESULT result = validate(iic, slave, reg, reg_bytes, data, count);
    if (result != MAX_IIC_OK) return result;
    iic->active = 1;
    budget = iic->timeout_us;
    result = wait_idle(iic, &budget);
    if (result != MAX_IIC_OK) { iic->active = 0; return result; }
    if (reg_bytes != 0) {
        prefix(buffer, reg, reg_bytes);
        repeated_start(iic, 1);
        result = transfer(iic, slave, buffer, reg_bytes, 0, &budget);
        if (result != MAX_IIC_OK) return finish(iic, result);
        /* The bus is intentionally held here: waiting for idle would deadlock.
         * Clearing the option selects STOP at the end of the following read.
         */
    }
    repeated_start(iic, 0);
    result = transfer(iic, slave, data, count, 1, &budget);
    if (result == MAX_IIC_OK) result = wait_idle(iic, &budget);
    return finish(iic, result);
}
