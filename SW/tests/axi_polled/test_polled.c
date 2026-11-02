#include "AxiIicPolled.h"
#include "xiic_l.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t status, irq, queue[5];
static uint8_t registers[65536], rx;
static unsigned used, starts, resets, sleeps, delayed_start,pending_empty;
static int missing, arbitration, stuck, stale, started, cleared_busy, late_rx, rx_polls;
uint32_t fake_read(uint32_t base, uint32_t offset)
{
    assert(base == 0xa0020000U);
    if (offset == XIIC_SR_REG_OFFSET)
    {
        if (rx_polls && --rx_polls == 0)
        {
            status &= ~XIIC_SR_RX_FIFO_EMPTY_MASK;
            irq |= XIIC_INTR_TX_ERROR_MASK;
        }
        if (started && delayed_start)
        {
            --delayed_start;
            if (!delayed_start)
                status |= XIIC_SR_BUS_BUSY_MASK;
        }
        return status;
    }
    if (offset == XIIC_IISR_OFFSET){
        if(pending_empty && (status&XIIC_SR_BUS_BUSY_MASK) && --pending_empty==0)irq|=XIIC_INTR_TX_EMPTY_MASK;
        return irq;
    }
    if (offset == XIIC_DRR_REG_OFFSET)
    {
        status |= XIIC_SR_RX_FIFO_EMPTY_MASK;
        return rx;
    }
    return 0;
}
void fake_write(uint32_t base, uint32_t offset, uint32_t value)
{
    assert(base == 0xa0020000U);
    if (offset == XIIC_RESETR_OFFSET)
    {
        ++resets;
        used = 0;
        started = 0;
        status = 0xc0;
        irq = 0;
        return;
    }
    if (offset == XIIC_IISR_OFFSET)
    {
        if ((value & XIIC_INTR_BNB_MASK) && (status & XIIC_SR_BUS_BUSY_MASK))
            cleared_busy = 1;
        irq ^= value;
        return;
    }
    if (offset != XIIC_DTR_REG_OFFSET)
        return;
    assert(used < 5);
    queue[used++] = value;
    if (used == 1)
    {
        assert(value == (XIIC_TX_DYN_START_MASK | 0x78));
        ++starts;
        started = 1;
        cleared_busy = 0;
        status &= ~XIIC_SR_TX_FIFO_EMPTY_MASK;
        if (!delayed_start)
            status |= XIIC_SR_BUS_BUSY_MASK;
        if (stale)
            irq |= XIIC_INTR_BNB_MASK;
        else
            cleared_busy = 1;
    }
    if (!(value & XIIC_TX_DYN_STOP_MASK)){pending_empty=2;return;}
    assert(cleared_busy); /* regression: completion flag must clear AFTER busy */
    assert(used == 4 || used == 5);
    assert(queue[1] <= 255 && queue[2] <= 255);
    uint16_t reg = (uint16_t)((queue[1] << 8) | queue[2]);
    if (stuck)
    {
        irq = 0;
        return;
    }
    status = 0xc0;
    irq = XIIC_INTR_BNB_MASK;
    if (arbitration)
        irq |= XIIC_INTR_ARB_LOST_MASK;
    else if (missing)
        irq |= XIIC_INTR_TX_ERROR_MASK;
    else if (used == 5)
    {
        assert(queue[3] == (XIIC_TX_DYN_START_MASK | 0x79));
        assert(value == (XIIC_TX_DYN_STOP_MASK | 1));
        rx = registers[reg];
        status &= ~XIIC_SR_RX_FIFO_EMPTY_MASK;
        irq |= XIIC_INTR_TX_ERROR_MASK; /* expected final master NACK */
        if (late_rx)
        {
            status |= XIIC_SR_RX_FIFO_EMPTY_MASK;
            irq &= ~XIIC_INTR_TX_ERROR_MASK;
            rx_polls = 4;
        }
    }
    else
        registers[reg] = (uint8_t)value;
    used = 0;
    started = 0;
}
int usleep(unsigned long usec)
{
    assert(usec == 1);
    ++sleeps;
    return 0;
}
static void setup(AXI_IIC_POLLED *b)
{
    used = starts = resets = sleeps = delayed_start = pending_empty = 0;
    missing = arbitration = stuck = stale = started = late_rx = rx_polls = 0;
    memset(registers, 0, sizeof(registers));
    assert(AxiIicPolled_Init(b, 0xa0020000, 50000, 200) == 0);
}
int main(void)
{
    AXI_IIC_POLLED b;
    uint8_t v = 0;
    setup(&b);
    registers[0x300a] = 0x56;
    assert(AxiIicPolled_Read(&b, 0x3c, 0x300a, &v, 1) == 0 && v == 0x56 && starts == 1);
    v = 0x80;
    assert(AxiIicPolled_Write(&b, 0x3c, 0x503d, &v, 1) == 0 && registers[0x503d] == 0x80);
    setup(&b);
    stale = 1;
    delayed_start = 8;
    registers[0x300a] = 0x56;
    assert(AxiIicPolled_Read(&b, 0x3c, 0x300a, &v, 1) == 0 && v == 0x56 && sleeps > 0 &&
           cleared_busy);
    setup(&b);
    late_rx = 1;
    registers[0x300a] = 0x56;
    assert(AxiIicPolled_Read(&b, 0x3c, 0x300a, &v, 1) == 0 && v == 0x56 && sleeps > 0);
    setup(&b);
    missing = 1;
    assert(AxiIicPolled_Read(&b, 0x3c, 0x300a, &v, 1) == -5 && starts == 1 && resets == 2);
    setup(&b);
    missing = 1;
    assert(AxiIicPolled_Write(&b, 0x3c, 0x503d, &v, 1) == -5 && starts == 1);
    setup(&b);
    arbitration = 1;
    assert(AxiIicPolled_Read(&b, 0x3c, 0x300a, &v, 1) == -6);
    setup(&b);
    stuck = 1;
    b.timeout_us = 5;
    assert(AxiIicPolled_Read(&b, 0x3c, 0x300a, &v, 1) == -4 && sleeps <= 5);
    setup(&b);
    status |= XIIC_SR_BUS_BUSY_MASK;
    assert(AxiIicPolled_Read(&b, 0x3c, 0x300a, &v, 1) == -3 && starts == 0 && resets == 1);
    assert(AxiIicPolled_Read(&b, 0x3c, 65535, &v, 2) == -1);
    puts("AXI FIFO: framing, delayed START/stale BNB, final NACK, absent slave, timeout and "
         "arbitration passed");
}
