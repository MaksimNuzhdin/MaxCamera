#include "CameraDiagnostics.h"
#include "sleep.h"
#include "xil_printf.h"

namespace {
const u32 AxiClock = 200000000U;
const unsigned int Attempts = 10;
const u32 TimingBase = 0x128;
struct Timing { u32 frequency; u32 hold; };
struct Counts { unsigned int passed, nack, timeout, other, wrong; };

u32 baseOf(const MAX_IIC *bus)
{
    return bus->bus == MAX_IIC_CAM0 ? XPAR_AXI_IIC_0_BASEADDR : XPAR_AXI_IIC_1_BASEADDR;
}

void captureTiming(u32 base, u32 values[8])
{
    for (unsigned int i = 0; i < 8; ++i) values[i] = XIic_ReadReg(base, TimingBase + 4*i);
}

void applyTiming(u32 base, const Timing &setting)
{
    // Linux i2c-xiic.c standard-mode setup/hold values, expressed in AXI cycles.
    // Frequency sweep changes THIGH/TLOW only; SDA hold is swept independently.
    const u32 mhz = AxiClock / 1000000U;
    const u32 half = (AxiClock + 2*setting.frequency-1) / (2*setting.frequency) - 8U;
    const u32 values[8] = {5700*mhz/1000-1, 5000*mhz/1000-1,
        4300*mhz/1000-1, 550*mhz/1000-1, 5000*mhz/1000-1,
        half, half, setting.hold};
    for (unsigned int i = 0; i < 8; ++i) XIic_WriteReg(base, TimingBase + 4*i, values[i]);
}

int dynamicByte(u32 base, uint16_t reg, uint8_t *value, const Timing &setting)
{
    if (XIic_DynInit(base) != XST_SUCCESS) return MAX_IIC_IO_ERROR;
    applyTiming(base, setting);
    XIic_WriteIisr(base, XIic_ReadIisr(base));
    XIic_WriteReg(base, XIIC_DTR_REG_OFFSET, XIIC_TX_DYN_START_MASK | 0x78);
    XIic_WriteReg(base, XIIC_DTR_REG_OFFSET, reg >> 8);
    XIic_WriteReg(base, XIIC_DTR_REG_OFFSET, reg & 0xff);
    XIic_WriteReg(base, XIIC_DTR_REG_OFFSET, XIIC_TX_DYN_START_MASK | 0x79);
    XIic_WriteReg(base, XIIC_DTR_REG_OFFSET, XIIC_TX_DYN_STOP_MASK | 1);
    for (unsigned int poll = 0; poll < 20000; ++poll) {
        u32 status = XIic_ReadReg(base, XIIC_SR_REG_OFFSET);
        if (!(status & XIIC_SR_RX_FIFO_EMPTY_MASK)) {
            *value = (uint8_t)XIic_ReadReg(base, XIIC_DRR_REG_OFFSET);
            for (unsigned int idle = 0; idle < 20000; ++idle) {
                if (!(XIic_ReadReg(base, XIIC_SR_REG_OFFSET) & XIIC_SR_BUS_BUSY_MASK)) return 0;
                usleep(1);
            }
            return MAX_IIC_TIMEOUT;
        }
        if (XIic_ReadIisr(base) & XIIC_INTR_ARB_LOST_MASK) return MAX_IIC_ARBITRATION_LOST;
        if (XIic_ReadIisr(base) & XIIC_INTR_TX_ERROR_MASK) {
            usleep(1);
            if (XIic_ReadReg(base, XIIC_SR_REG_OFFSET) & XIIC_SR_RX_FIFO_EMPTY_MASK) return MAX_IIC_NACK;
        }
        usleep(1);
    }
    return MAX_IIC_TIMEOUT;
}

int standardByte(MAX_IIC *bus, uint16_t reg, uint8_t *value,
                 const Timing &setting, bool stop)
{
    const u32 base = baseOf(bus);
    applyTiming(base, setting);
    if (!stop) return MaxIIC_Read(bus, 0x3c, reg, 2, value, 1);
    uint8_t pointer[2] = {(uint8_t)(reg >> 8), (uint8_t)reg};
    int result = MaxIIC_Write(bus, 0x3c, 0, 0, pointer, 2);
    if (result != MAX_IIC_OK) return result;
    usleep(1000);
    applyTiming(base, setting);
    return MaxIIC_Read(bus, 0x3c, 0, 0, value, 1);
}

void countResult(Counts &counts, int hs, int ls, uint8_t high, uint8_t low)
{
    if (hs == 0 && ls == 0) {
        if (high == 0x56 && low == 0x40) ++counts.passed;
        else ++counts.wrong;
    } else if (hs == MAX_IIC_NACK || ls == MAX_IIC_NACK) ++counts.nack;
    else if (hs == MAX_IIC_TIMEOUT || ls == MAX_IIC_TIMEOUT) ++counts.timeout;
    else ++counts.other;
}

void runBus(MAX_IIC *bus, const char *name)
{
    const u32 base = baseOf(bus);
    u32 original[8];
    captureTiming(base, original);
    xil_printf("I2C_TIMING %s original:", name);
    for (unsigned int i = 0; i < 8; ++i) xil_printf(" %d", original[i]);
    xil_printf("\r\n");
    const u32 frequencies[] = {100000U, 50000U, 10000U};
    const u32 holds[] = {1U, 20U, 60U, 200U};
    for (unsigned int f = 0; f < 3; ++f) {
        for (unsigned int h = 0; h < 4; ++h) {
            Timing timing = {frequencies[f], holds[h]};
            // 0 = XIic repeated START; 1 = XIic separate STOP; 2 = direct FIFO.
            for (unsigned int mode = 0; mode < 3; ++mode) {
                (void)MaxIIC_Init(bus, bus->bus, 100000U, 20000U);
                Counts counts = {};
                uint8_t high = 0, low = 0;
                int hs = 0, ls = 0;
                for (unsigned int attempt = 0; attempt < Attempts; ++attempt) {
                    high = low = 0;
                    if (mode == 2) {
                        hs = dynamicByte(base, 0x300a, &high, timing);
                        ls = hs == 0 ? dynamicByte(base, 0x300b, &low, timing) : hs;
                    } else {
                        hs = standardByte(bus, 0x300a, &high, timing, mode == 1);
                        ls = hs == 0 ? standardByte(bus, 0x300b, &low, timing, mode == 1) : hs;
                    }
                    countResult(counts, hs, ls, high, low);
                    usleep(1000);
                }
                xil_printf("I2C_MATRIX %s Hz=%d HOLD=%d MODE=%d PASS=%d/%d NACK=%d TIMEOUT=%d OTHER=%d WRONG=%d LAST=%d,%d,0x%02x%02x\r\n",
                    name, timing.frequency, timing.hold, mode, counts.passed, Attempts,
                    counts.nack, counts.timeout, counts.other, counts.wrong, hs, ls, high, low);
            }
        }
    }
    // Dynamic FIFO testing owns the hardware temporarily; rebuild the BSP context.
    (void)MaxIIC_Init(bus, bus->bus, 100000U, MAX_IIC_DEFAULT_TIMEOUT_US);
    for (unsigned int i = 0; i < 8; ++i) XIic_WriteReg(base, TimingBase + 4*i, original[i]);
}

void stressBus(MAX_IIC *bus)
{
    struct Case { u32 hz, hold; unsigned int mode; };
    const Case cases[] = {{100000,200,0},{100000,400,0},{100000,600,0},
        {50000,200,0},{50000,400,0},{50000,600,0},
        {100000,200,2},{100000,400,2},{100000,600,2},{50000,200,2}};
    const unsigned int attempts = 200;
    const u32 base = baseOf(bus);
    u32 original[8];
    captureTiming(base, original);
    for (const Case &test : cases) {
        (void)MaxIIC_Init(bus, bus->bus, 100000U, 20000U);
        Timing timing = {test.hz, test.hold};
        Counts counts = {};
        for (unsigned int attempt = 0; attempt < attempts; ++attempt) {
            uint8_t high = 0, low = 0;
            int hs = test.mode == 2 ? dynamicByte(base, 0x300a, &high, timing) :
                standardByte(bus, 0x300a, &high, timing, false);
            int ls = hs == 0 ? (test.mode == 2 ? dynamicByte(base, 0x300b, &low, timing) :
                standardByte(bus, 0x300b, &low, timing, false)) : hs;
            countResult(counts, hs, ls, high, low);
            usleep(1000);
        }
        xil_printf("I2C_STRESS CAM0 Hz=%d HOLD=%d MODE=%d PASS=%d/%d NACK=%d TIMEOUT=%d OTHER=%d WRONG=%d\r\n",
            timing.frequency, timing.hold, test.mode, counts.passed, attempts,
            counts.nack, counts.timeout, counts.other, counts.wrong);
    }
    (void)MaxIIC_Init(bus, bus->bus, 100000U, MAX_IIC_DEFAULT_TIMEOUT_US);
    for (unsigned int i = 0; i < 8; ++i) XIic_WriteReg(base, TimingBase + 4*i, original[i]);
}

void driverTest(MAX_IIC *cam0, MAX_IIC *cam1)
{
    Counts counts = {};
    const unsigned int attempts = 1000;
    for (unsigned int attempt = 0; attempt < attempts; ++attempt) {
        uint8_t high = 0, low = 0;
        int hs = MaxIIC_Read(cam0, 0x3c, 0x300a, 2, &high, 1);
        int ls = hs == 0 ? MaxIIC_Read(cam0, 0x3c, 0x300b, 2, &low, 1) : hs;
        countResult(counts, hs, ls, high, low);
        usleep(1000);
    }
    xil_printf("I2C_DRIVER CAM0 Hz=%d PASS=%d/%d NACK=%d TIMEOUT=%d OTHER=%d WRONG=%d\r\n",
        cam0->clock_hz, counts.passed, attempts, counts.nack, counts.timeout,
        counts.other, counts.wrong);
    uint8_t original = 0, changed = 0, restored = 0;
    int before = MaxIIC_Read(cam0, 0x3c, 0x503d, 2, &original, 1);
    if (before == 0 && counts.passed == attempts) {
        uint8_t test = original ^ 0x80U; // Datasheet: color-bar enable, reversible.
        int ws = MaxIIC_Write(cam0, 0x3c, 0x503d, 2, &test, 1);
        int rs = MaxIIC_Read(cam0, 0x3c, 0x503d, 2, &changed, 1);
        // Restore even after a reported write failure: the device may have
        // accepted part or all of a write before the controller signaled error.
        int restoreWrite = MaxIIC_Write(cam0, 0x3c, 0x503d, 2, &original, 1);
        int restoreRead = MaxIIC_Read(cam0, 0x3c, 0x503d, 2, &restored, 1);
        xil_printf("I2C_WRITE_TEST reg=0x503d original=0x%02x requested=0x%02x actual=0x%02x write=%d read=%d restored=0x%02x restore_write=%d restore_read=%d\r\n",
            original, test, changed, ws, rs, restored, restoreWrite, restoreRead);
    } else {
        xil_printf("I2C_WRITE_TEST skipped: ID not stable or initial read failed (%d)\r\n", before);
    }
    uint8_t absent = 0;
    int ns = MaxIIC_Read(cam1, 0x3c, 0x300a, 2, &absent, 1);
    xil_printf("I2C_ABSENT CAM1 status=%d initialized=%d\r\n", ns, cam1->initialized);
}
}

void CameraDiagnostics_Run(MAX_IIC *cam0, MAX_IIC *cam1)
{
    xil_printf("I2C_MATRIX_BEGIN AXI_CLOCK=%d ID_EXPECTED=0x5640\r\n", AxiClock);
    // Normal boot only probes ID. Modes 0/1/2 are explicit engineering tests;
    // mode 2 also temporarily changes and restores the color-bar register.
    const unsigned int diagnosticMode = 3;
    if (diagnosticMode == 0) {
        runBus(cam0, "CAM0");
        runBus(cam1, "CAM1");
    } else if (diagnosticMode == 1) {
        stressBus(cam0);
    } else if (diagnosticMode == 2) {
        driverTest(cam0, cam1);
    } else {
        MAX_IIC *buses[] = {cam0, cam1};
        for (unsigned int index = 0; index < 2; ++index) {
#if MAX_CAMERA_VIDEO_GPIO_SCCB || MAX_CAMERA_GIGE_GPIO_SCCB
            if(index==0) {xil_printf("OV5640 CAM0 legacy AXI-IIC bypassed; camera driver uses GPIO SCCB\r\n");continue;}
#endif
            uint8_t high = 0, low = 0;
            int hs = MaxIIC_Read(buses[index], 0x3c, 0x300a, 2, &high, 1);
            int ls = hs == 0 ? MaxIIC_Read(buses[index], 0x3c, 0x300b, 2, &low, 1) : hs;
            xil_printf("OV5640 CAM%d Hz=%d ID=0x%02x%02x status=%d,%d %s\r\n",
                index, buses[index]->clock_hz, high, low, hs, ls,
                hs == 0 && ls == 0 && high == 0x56 && low == 0x40 ? "FOUND" : "NOT DETECTED");
        }
    }
    xil_printf("I2C_MATRIX_END\r\n");
}
