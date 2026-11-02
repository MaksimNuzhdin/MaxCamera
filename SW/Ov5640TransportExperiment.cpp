#include "Ov5640TransportExperiment.h"
#include "AxiIicPolled.h"
#include "Ov5640.h"
#include "sleep.h"
#include "xil_cache.h"
#include "xiic_l.h"
#include "xil_printf.h"
static int stdread(void *b, uint8_t a, uint16_t r, uint8_t *p, uint16_t n)
{
    return MaxIIC_Read((MAX_IIC *)b, a, r, 2, p, n);
}
static int stdwrite(void *b, uint8_t a, uint16_t r, const uint8_t *p, uint16_t n)
{
    return MaxIIC_Write((MAX_IIC *)b, a, r, 2, p, n);
}
static bool trace_errors = false;
static int dynread(void *u, uint8_t a, uint16_t r, uint8_t *p, uint16_t n)
{
    int e = AxiIicPolled_Read(u, a, r, p, n);
    AXI_IIC_POLLED *b = (AXI_IIC_POLLED *)u;
    if (e && trace_errors)
        xil_printf("DYNAMIC ERROR READ reg=%04x status=%d sr=%08x irq=%08x phase=%d\r\n", r, e,
                   b->status, b->interrupts, b->phase);
    return e;
}
static int dynwrite(void *u, uint8_t a, uint16_t r, const uint8_t *p, uint16_t n)
{
    int e = AxiIicPolled_Write(u, a, r, p, n);
    AXI_IIC_POLLED *b = (AXI_IIC_POLLED *)u;
    if (e && trace_errors)
        xil_printf(
            "DYNAMIC ERROR WRITE reg=%04x value=%02x status=%d sr=%08x irq=%08x phase=%d\r\n", r,
            p[0], e, b->status, b->interrupts, b->phase);
    return e;
}
static void delay(void *, uint32_t us) { usleep(us); }
static void measure(const char *name, OV5640_IO *io)
{
    unsigned read_errors = 0, wrong_id = 0, write_errors = 0, verify_errors = 0, mismatch = 0;
    const uint16_t regs[] = {0x300a, 0x300b, 0x3008, 0x3103, 0x3034,
                             0x3035, 0x3036, 0x3037, 0x3108, 0x503d};
    uint8_t v = 0, original = 0;
    int absent = io->read(io->user, 0x3d, 0x300a, &v, 1);
    xil_printf("TRANSPORT %s absent-address=%d (expected NACK)\r\n", name, absent);
    for (unsigned k = 0; k < 1000; ++k)
    {
        int e = io->read(io->user, 0x3c, regs[k % 10], &v, 1);
        if (e)
            ++read_errors;
        else if ((k % 10 == 0 && v != 0x56) || (k % 10 == 1 && v != 0x40))
            ++wrong_id;
    }
    int e = io->read(io->user, 0x3c, 0x503d, &original, 1);
    if (e)
    {
        xil_printf("TRANSPORT %s cannot save 503D: %d; skipping writes\r\n", name, e);
        return;
    }
    for (unsigned k = 0; k < 200; ++k)
    {
        uint8_t target = (k & 1) ? original : (uint8_t)(original ^ 0x80);
        if (io->write(io->user, 0x3c, 0x503d, &target, 1))
            ++write_errors;
        int r = io->read(io->user, 0x3c, 0x503d, &v, 1);
        if (r)
            ++verify_errors;
        else if (v != target)
            ++mismatch;
    }
    int restore = io->write(io->user, 0x3c, 0x503d, &original, 1);
    int check = io->read(io->user, 0x3c, 0x503d, &v, 1);
    xil_printf("TRANSPORT %s reads=1000 read_errors=%d wrong_id=%d writes=200 write_errors=%d "
               "verify_errors=%d mismatch=%d restore=%d check=%d restored=%d\r\n",
               name, read_errors, wrong_id, write_errors, verify_errors, mismatch, restore, check,
               check == 0 && v == original);
}
struct Setting
{
    uint16_t reg;
    uint8_t value;
    uint16_t ms;
};
static const Setting linux_table[] = {
#include "Ov5640Tuning.inc"
};
static int power_command(OV5640_IO *io, uint8_t value)
{
    int e = io->write(io->user, 0x3c, 0x3008, &value, 1);
    if (e && e != -5)
        return e;
    usleep(value == 0x82 ? 100000 : 25000);
    uint8_t state = 0, hi = 0, lo = 0;
    int r = io->read(io->user, 0x3c, 0x3008, &state, 1);
    if (!r)
        r = io->read(io->user, 0x3c, 0x300a, &hi, 1);
    if (!r)
        r = io->read(io->user, 0x3c, 0x300b, &lo, 1);
    bool matches = (value == 0x82) ? !(state & 0x80) : ((state & 0xc2) == value);
    xil_printf("LINUX MIN power=%02x ack=%d read=%d state=%02x ID=%02x%02x verified=%d\r\n", value,
               e, r, state, hi, lo, r == 0 && matches && hi == 0x56 && lo == 0x40);
    return (r == 0 && matches && hi == 0x56 && lo == 0x40) ? 0 : (r ? r : -7);
}
static void linux_sequence(OV5640_IO *io)
{
    uint8_t v = 0;
    int e = power_command(io, 0x82);
    if (e)
        return;
    e = power_command(io, 0x42);
    if (e)
        return;
    for (unsigned i = 0; i < sizeof(linux_table) / sizeof(linux_table[0]); ++i)
    {
        v = linux_table[i].value;
        e = linux_table[i].reg == 0x3008 ? power_command(io, v)
                                         : io->write(io->user, 0x3c, linux_table[i].reg, &v, 1);
        if (e)
        {
            xil_printf("LINUX MIN table failed index=%d reg=%04x value=%02x status=%d\r\n", i,
                       linux_table[i].reg, v, e);
            return;
        }
        if (linux_table[i].ms)
            usleep((uint32_t)linux_table[i].ms * 1000);
    }
    /* Reset defaults keep MIPI disabled throughout the unmodified Linux table.
     * Explicitly gate output before giving the device to the application API. */
    v = 0x0f;
    e = io->write(io->user, 0x3c, 0x4202, &v, 1);
    if (e)
        return;
    v = 0x40;
    e = io->write(io->user, 0x3c, 0x300e, &v, 1);
    if (e)
        return;
    const uint16_t verify_regs[] = {0x5000, 0x5001, 0x5180, 0x5381, 0x5481};
    for (unsigned k = 0; k < 5; ++k)
    {
        v = 0;
        e = io->read(io->user, 0x3c, verify_regs[k], &v, 1);
        xil_printf("LINUX MIN table readback %04x=%02x status=%d\r\n", verify_regs[k], v, e);
        if (e)
            return;
    }
    OV5640 sensor;
    OV5640_CONFIG c;
    OV5640_STATUS status;
    Ov5640_Bind(&sensor, io, 0x3c);
    sensor.state = OV5640_READY;
    sensor.config.clock.xclk_hz = 25000000;
    Ov5640_DefaultConfig(&c);
    e = Ov5640_Configure(&sensor, &c);
    if (!e)
        e = Ov5640_GetStatus(&sensor, &status);
    xil_printf("LINUX MIN table=PASS config=%d bus=%d reg=%04x state=%d\r\n", e,
               sensor.transport_error, sensor.error_register, sensor.state);
    if (!e)
        xil_printf("LINUX MIN readback HTS=%d VTS=%d gain16=%d frame_output=GATED\r\n", status.hts,
                   status.vts, status.gain16);
}
#if MAX_CAMERA_ILA_CAPTURE
/* Write-only trace: no expected master receive NACK can trigger the ILA.
 * Stops at the first failed command; never replays a partially applied write. */
static void repeated_init(OV5640_IO *io)
{
    OV5640 sensor;
    OV5640_CONFIG config;
    Ov5640_Bind(&sensor,io,0x3c);
    Ov5640_DefaultConfig(&config);
    unsigned passed=0, consecutive=0;
    for(unsigned i=0;i<10;++i) {
        OV5640_RESULT init=Ov5640_Init(&sensor);
        OV5640_RESULT configure=init ? init : Ov5640_Configure(&sensor,&config);
        OV5640_STATUS status;
        OV5640_RESULT readback=configure ? configure : Ov5640_GetStatus(&sensor,&status);
        xil_printf("REINIT trial=%d init=%d config=%d readback=%d bus=%d reg=%04x power_nacks=%d previous_pass=%d\r\n",
                   i+1,init,configure,readback,sensor.transport_error,sensor.error_register,sensor.verified_power_nacks,consecutive);
        if(!readback) {++passed;++consecutive;}
        else consecutive=0;
        usleep(100000);
    }
    xil_printf("REINIT END passed=%d total=10 NO_HARDWARE_RESET\r\n",passed);
}
static void ila_init_writes(OV5640_IO *io, bool prepared = false)
{
    const Setting prefix[] = {{0x3008,0x82,100}, {0x3008,0x42,25},
                              {0x4202,0x0f,0}, {0x300e,0x40,0}};
    for (unsigned group=prepared ? 1 : 0;group<2;++group) {
        const Setting *table=group ? linux_table : prefix;
        unsigned count=group ? sizeof(linux_table)/sizeof(linux_table[0]) : sizeof(prefix)/sizeof(prefix[0]);
        for(unsigned i=0;i<count;++i) {
            uint8_t value=table[i].value;
            int e=io->write(io->user,0x3c,table[i].reg,&value,1);
            if(e) {
                xil_printf("ILA INIT FAIL group=%d index=%d reg=%04x value=%02x status=%d\r\n",group,i,table[i].reg,value,e);
                return;
            }
            if(table[i].ms) usleep((uint32_t)table[i].ms*1000);
        }
    }
    xil_printf("ILA INIT WRITE TABLE PASS\r\n");
}
#endif
#if MAX_CAMERA_ILA_CAPTURE
extern "C" {
/* Own both cache lines: invalidating a scalar also discarded adjacent BSP
 * timer state and caused Prefetch Abort in usleep(). */
volatile uint32_t g_transport_start[16] __attribute__((aligned(64))) = {0};
}
static void ila_write_test(OV5640_IO *io, MAX_IIC *standard_bus)
{
    /* Read-only preparation. No wake/reset or register writes before ILA is armed. */
    uint8_t original=0,v=0;
    int e;
#if !defined(MAX_CAMERA_ILA_PREP_READ) || MAX_CAMERA_ILA_PREP_READ
    e = io->read(io->user, 0x3c, 0x503d, &original, 1);
#else
    e=-1;
    xil_printf("ILA PREP read deliberately skipped; controller is fresh\r\n");
#endif
    int saved_status=e;
    if(e) xil_printf("ILA PREP read503D failed=%d; register-toggle mode unavailable\r\n",e);
    AXI_IIC_POLLED *capture_bus=(AXI_IIC_POLLED *)io->user;
    XIic_WriteReg(capture_bus->base,XIIC_IISR_OFFSET,XIic_ReadReg(capture_bus->base,XIIC_IISR_OFFSET));
    Xil_DCacheFlushRange((INTPTR)g_transport_start, sizeof(g_transport_start));
    xil_printf("ILA READY original503D=%02x start_flag=%08x; waiting for JTAG arm\r\n", original,
               (unsigned)(uintptr_t)&g_transport_start);
    unsigned waited = 0;
    for (;;)
    {
        Xil_DCacheInvalidateRange((INTPTR)&g_transport_start, 64);
        if (g_transport_start[0])
            break;
        if (++waited >= 300000)
        {
            xil_printf("ILA WAIT timeout; no writes performed\r\n");
            return;
        }
        usleep(1000);
    }
    trace_errors = true;
    if(g_transport_start[0]>=8 && g_transport_start[0]<=10) {
        /* Explicit diagnostic writes with known targets; no dependency on a
         * successful read/save. Abort at the first error, never replay it. */
        unsigned count=0;int status=0;
        OV5640_IO selected=*io;
        if(g_transport_start[0]==10) {
            status=AxiIicPolled_Init((AXI_IIC_POLLED *)io->user,0xa0020000,50000,200);
            xil_printf("ILA ORDINARY WRITE transport=FIFO hold=200 setup=%d\r\n",status);
        }
        if(g_transport_start[0]==9) {
            status=MaxIIC_Init(standard_bus,MAX_IIC_CAM0,50000,MAX_IIC_DEFAULT_TIMEOUT_US);
            selected={standard_bus,stdread,stdwrite,delay};
            xil_printf("ILA ORDINARY WRITE transport=BSP setup=%d\r\n",status);
        }
        for(;!status && count<1000;++count) {
            uint8_t target=(count&1U)?0x80:0;
            status=selected.write(selected.user,0x3c,0x503d,&target,1);
            if(status)break;
        }
        xil_printf("ILA ORDINARY WRITE count=%d status=%d reg=503D target=%02x\r\n",count,status,(count&1U)?0x80:0);
        return;
    }
    if (g_transport_start[0] == 7)
    {
        int e=AxiIicPolled_Init((AXI_IIC_POLLED *)io->user,0xa0020000,50000,200);
        xil_printf("REINIT FIFO HOLD200 setup=%d\r\n",e);
        if(!e) repeated_init(io);
        return;
    }
    if (g_transport_start[0] == 6)
    {
        int e=MaxIIC_Init(standard_bus,MAX_IIC_CAM0,50000,MAX_IIC_DEFAULT_TIMEOUT_US);
        xil_printf("REINIT BSP setup=%d\r\n",e);
        OV5640_IO bsp={standard_bus,stdread,stdwrite,delay};
        if(!e) repeated_init(&bsp);
        return;
    }
    if (g_transport_start[0] == 5)
    {
        int e=power_command(io,0x82);
        if(!e) e=power_command(io,0x42);
        if(e) {xil_printf("ILA TABLE PREP FAILED status=%d\r\n",e);return;}
        XIic_WriteReg(capture_bus->base,XIIC_IISR_OFFSET,XIic_ReadReg(capture_bus->base,XIIC_IISR_OFFSET));
        g_transport_start[0]=0;
        Xil_DCacheFlushRange((INTPTR)g_transport_start,sizeof(g_transport_start));
        xil_printf("ILA TABLE READY: reset/standby verified; rearm and release again\r\n");
        unsigned wait=0;
        do {
            Xil_DCacheInvalidateRange((INTPTR)g_transport_start,sizeof(g_transport_start));
            if(++wait>=300000) {xil_printf("ILA TABLE WAIT timeout\r\n");return;}
            usleep(1000);
        } while(!g_transport_start[0]);
        ila_init_writes(io,true);
        return;
    }
    if (g_transport_start[0] == 4)
    {
        repeated_init(io);
        return;
    }
    if (g_transport_start[0] == 3)
    {
        ila_init_writes(io);
        return;
    }
    if (g_transport_start[0] == 2)
    {
        int reset_status = power_command(io, 0x82);
        xil_printf("ILA RESET DONE status=%d\r\n", reset_status);
        return;
    }
    if(saved_status) return;
    unsigned k;
    for (k = 0; k < 1000; ++k)
    {
        uint8_t target = (k & 1) ? original : (uint8_t)(original ^ 0x80);
        e = io->write(io->user, 0x3c, 0x503d, &target, 1);
        if (e)
        {
            xil_printf("ILA WRITE FAILED index=%d target=%02x status=%d\r\n", k, target, e);
            break;
        }
    }
    int restore = io->write(io->user, 0x3c, 0x503d, &original, 1);
    int r = io->read(io->user, 0x3c, 0x503d, &v, 1);
    xil_printf("ILA WRITE DONE count=%d restore=%d read=%d restored=%d\r\n", k, restore, r,
               r == 0 && v == original);
}
#endif
void Ov5640TransportExperiment_Run(MAX_IIC *bus)
{
    AXI_IIC_POLLED dyn;
    OV5640_IO standard = {bus, stdread, stdwrite, delay};
    OV5640_IO independent = {&dyn, dynread, dynwrite, delay};
#if MAX_CAMERA_ILA_CAPTURE
    int init = AxiIicPolled_Init(&dyn, 0xa0020000, 50000, 400);
    if (!init)
        ila_write_test(&independent,bus);
    return;
#endif
    xil_printf("TRANSPORT EXPERIMENT BEGIN no retries, both methods identical timing\r\n");
    MaxIIC_Init(bus, MAX_IIC_CAM0, 50000, MAX_IIC_DEFAULT_TIMEOUT_US);
    measure("BSP-50k", &standard);
    int e = AxiIicPolled_Init(&dyn, 0xa0020000, 50000, 200);
    xil_printf("TRANSPORT dynamic init=%d\r\n", e);
    if (!e)
        measure("DYNAMIC-50k", &independent);
    e = AxiIicPolled_Init(&dyn, 0xa0020000, 10000, 200);
    if (!e)
        measure("DYNAMIC-10k", &independent);
    AxiIicPolled_Init(&dyn, 0xa0020000, 10000, 400);
    trace_errors = true;
    for (unsigned trial = 0; trial < 3; ++trial)
    {
        xil_printf("LINUX MIN dynamic 10k hold=400 trial=%d BEGIN\r\n", trial + 1);
        linux_sequence(&independent);
    }
    xil_printf("TRANSPORT EXPERIMENT END sr=%08x irq=%08x phase=%d\r\n", dyn.status, dyn.interrupts,
               dyn.phase);
}
