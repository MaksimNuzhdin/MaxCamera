#include "AxiIicPolled.h"
#include "sleep.h"
#include "xiic_l.h"
#include <stddef.h>
/* Command FIFO tags and register semantics follow AMD xiic_l.c Dyn* helpers.
 * Unlike those helpers all waits here are bounded. Only a few FIFO entries
 * are queued, so neither TX refill nor RX interrupts are required. */
static uint32_t rd(AXI_IIC_POLLED *b, uint32_t r) { return XIic_ReadReg(b->base, r); }
static void wr(AXI_IIC_POLLED *b, uint32_t r, uint32_t v) { XIic_WriteReg(b->base, r, v); }
static void reset(AXI_IIC_POLLED *b)
{
    unsigned i;
    uint32_t half = (200000000U + 2 * b->clock_hz - 1) / (2 * b->clock_hz) - 8;
    const uint32_t timing[] = {1139, 999, 859, 109, 999, half, half, b->hold_cycles};
    wr(b, XIIC_RESETR_OFFSET, XIIC_RESET_MASK);
    wr(b, XIIC_DGIER_OFFSET, 0);
    wr(b, XIIC_IIER_OFFSET, 0);
    for (i = 0; i < 8; ++i)
        wr(b, 0x128 + 4 * i, timing[i]);
    wr(b, XIIC_RFD_REG_OFFSET, 15);
    wr(b, XIIC_CR_REG_OFFSET, XIIC_CR_TX_FIFO_RESET_MASK);
    wr(b, XIIC_CR_REG_OFFSET, XIIC_CR_ENABLE_DEVICE_MASK);
    wr(b, XIIC_IISR_OFFSET, rd(b, XIIC_IISR_OFFSET));
}
int AxiIicPolled_Init(AXI_IIC_POLLED *b, uint32_t base, uint32_t hz, uint32_t hold)
{
    if (!b || (base != 0xa0020000U && base != 0xa0030000U) ||
        (hz != 10000 && hz != 50000 && hz != 100000) || !hold || hold > 200000000U / (2 * hz) - 8)
        return -1;
    b->base = base;
    b->clock_hz = hz;
    b->hold_cycles = hold;
    b->timeout_us = 100000;
    b->status = 0;
    b->interrupts = 0;
    b->phase = 0;
    reset(b);
    return (rd(b, XIIC_SR_REG_OFFSET) & XIIC_SR_BUS_BUSY_MASK) ? -3 : 0;
}
static void clear_tx_empty(AXI_IIC_POLLED *b)
{
    wr(b,XIIC_IISR_OFFSET,rd(b,XIIC_IISR_OFFSET)&XIIC_INTR_TX_EMPTY_MASK);
}
static int wait_tx_empty(AXI_IIC_POLLED *b,uint32_t *elapsed)
{
    for(;;){
        b->interrupts=rd(b,XIIC_IISR_OFFSET);b->status=rd(b,XIIC_SR_REG_OFFSET);
        if(b->interrupts&XIIC_INTR_ARB_LOST_MASK)return -6;
        if(b->interrupts&XIIC_INTR_TX_ERROR_MASK)return -5;
        if(b->interrupts&XIIC_INTR_TX_EMPTY_MASK)return 0;
        if((*elapsed)++>=b->timeout_us)return -4;
        usleep(1);
    }
}
static int transaction(AXI_IIC_POLLED *b, uint8_t a, uint16_t reg, uint8_t *v, int receive)
{
    uint32_t elapsed = 0, empty_end_polls = 0;
    int result = -4, got = 0;
    if (!b || !v || a < 8 || a > 0x77)
        return -1;
    b->phase = 1;
    if (rd(b, XIIC_SR_REG_OFFSET) & XIIC_SR_BUS_BUSY_MASK)
        return -3;
    if (!(rd(b, XIIC_SR_REG_OFFSET) & XIIC_SR_TX_FIFO_EMPTY_MASK) ||
        !(rd(b, XIIC_SR_REG_OFFSET) & XIIC_SR_RX_FIFO_EMPTY_MASK))
        return -7;
    /* IISR is toggle-on-write. Write only currently asserted bits. */
    wr(b, XIIC_IISR_OFFSET, rd(b, XIIC_IISR_OFFSET));
    b->phase = 2;
    wr(b, XIIC_DTR_REG_OFFSET, XIIC_TX_DYN_START_MASK | ((uint32_t)a << 1));
    clear_tx_empty(b);
    /* BNB can assert while the START command crosses to the IIC clock domain.
     * Observe BUS_BUSY first, then clear BNB while the bus is actually busy.
     * Clearing it only before START can mistake a stale BNB for completion. */
    while (!(rd(b, XIIC_SR_REG_OFFSET) & XIIC_SR_BUS_BUSY_MASK))
    {
        uint32_t irq = rd(b, XIIC_IISR_OFFSET);
        if (irq & XIIC_INTR_ARB_LOST_MASK)
        {
            b->interrupts = irq;
            reset(b);
            return -6;
        }
        if (irq & XIIC_INTR_TX_ERROR_MASK)
        {
            b->interrupts = irq;
            reset(b);
            return -5;
        }
        if (elapsed++ >= b->timeout_us)
        {
            reset(b);
            return -4;
        }
        usleep(1);
    }
    wr(b, XIIC_IISR_OFFSET, rd(b, XIIC_IISR_OFFSET) & XIIC_INTR_BNB_MASK);
    /* Match AMD DynSendData pacing: drain each byte before the next.
     * In particular, never change direction/STOP with prefix bytes queued. */
    result=wait_tx_empty(b,&elapsed);if(result){reset(b);return result;}
    b->phase=3;wr(b,XIIC_DTR_REG_OFFSET,reg>>8);clear_tx_empty(b);
    result=wait_tx_empty(b,&elapsed);if(result){reset(b);return result;}
    b->phase=4;wr(b,XIIC_DTR_REG_OFFSET,reg&255);clear_tx_empty(b);
    result=wait_tx_empty(b,&elapsed);if(result){reset(b);return result;}
    b->phase=5;
    if(receive){
        wr(b,XIIC_DTR_REG_OFFSET,XIIC_TX_DYN_START_MASK|((uint32_t)a<<1)|1);
        wr(b,XIIC_IISR_OFFSET,rd(b,XIIC_IISR_OFFSET)&XIIC_INTR_BNB_MASK);
        wr(b,XIIC_DTR_REG_OFFSET,XIIC_TX_DYN_STOP_MASK|1);
    }else wr(b,XIIC_DTR_REG_OFFSET,XIIC_TX_DYN_STOP_MASK|*v);
    result=-4;
    for (;;)
    {
        uint32_t irq = rd(b, XIIC_IISR_OFFSET), status = rd(b, XIIC_SR_REG_OFFSET);
        b->interrupts = irq;
        b->status = status;
        if (irq & XIIC_INTR_ARB_LOST_MASK)
        {
            result = -6;
            break;
        }
        if (receive && !got && !(status & XIIC_SR_RX_FIFO_EMPTY_MASK))
        {
            *v = (uint8_t)rd(b, XIIC_DRR_REG_OFFSET);
            got = 1;
            b->phase = 3;
        }
        if (!receive && (irq & XIIC_INTR_TX_ERROR_MASK))
        {
            result = -5;
            break;
        }
        /* TX_ERROR during final receive NACK is expected, but only successful
         * if a byte actually arrived AND the STOP completed. Address NACK
         * produces no byte and must never be accepted as a successful read. */
        if ((irq & XIIC_INTR_BNB_MASK) && !(status & XIIC_SR_BUS_BUSY_MASK) &&
            !(rd(b,XIIC_CR_REG_OFFSET)&XIIC_CR_MSMS_MASK))
        {
            if (receive && !got)
            {
                /* RX FIFO visibility can lag STOP/BNB across clock domains.
                 * Only classify a real TX_ERROR as NACK, after allowing the
                 * receive status to settle. BNB alone is not a NACK. */
                if ((irq & XIIC_INTR_TX_ERROR_MASK) && ++empty_end_polls >= 3)
                {
                    result = -5;
                    break;
                }
            }
            else if (status & XIIC_SR_TX_FIFO_EMPTY_MASK)
            {
                result = 0;
                break;
            }
        }
        if (elapsed++ >= b->timeout_us)
            break;
        usleep(1);
    }
    if (result)
        reset(b); /* no retry; preserve sampled error status/phase */
    return result;
}
int AxiIicPolled_Read(void *u, uint8_t a, uint16_t r, uint8_t *p, uint16_t n)
{
    uint16_t i;
    if (!p || !n || (uint32_t)r + n > 65536U)
        return -1;
    for (i = 0; i < n; ++i)
    {
        int e = transaction((AXI_IIC_POLLED *)u, a, (uint16_t)(r + i), p + i, 1);
        if (e)
            return e;
    }
    return 0;
}
int AxiIicPolled_Write(void *u, uint8_t a, uint16_t r, const uint8_t *p, uint16_t n)
{
    uint16_t i;
    if (!p || !n || (uint32_t)r + n > 65536U)
        return -1;
    for (i = 0; i < n; ++i)
    {
        uint8_t v = p[i];
        int e = transaction((AXI_IIC_POLLED *)u, a, (uint16_t)(r + i), &v, 0);
        if (e)
            return e;
    }
    return 0;
}
