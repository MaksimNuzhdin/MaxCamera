#define main synchronous_regression_main
#include "../gpio_sccb/test_gpio.cpp"
#undef main
#include "gige/GigeSensorTelemetry.h"
static unsigned io_calls,publications;
static AXI_GPIO_SCCB sync_bus;
static void release(void *,unsigned mask){++io_calls;slave.drive(mask);}
static unsigned levels(void *){++io_calls;return slave.levels();}
static void publish(void *,uint16_t reg,uint8_t value)
{assert(slave.regs[reg]==value);++publications;}
static SccbAsync async_bus()
{
    sync_bus=setup();SccbAsync s{};SccbAsyncIo io={nullptr,release,levels};
    assert(!SccbAsync_Init(&s,&io,50000,1000,100000));return s;
}
#include "gige/GigeSccbAccess.h"
int main()
{
    // Exercise the production arbitration helper with the independent slave.
    SccbAsync bus=async_bus();GigeSensorTelemetry telemetry{};
    assert(!GigeSensorTelemetry_Init(&telemetry,&bus,0x3c,100000,nullptr,publish));
    GigeSccbAccess access(telemetry);uint64_t now=0;
    // Stopped cameras must not acquire SCCB between ordinary SDK controls.
    for(unsigned i=0;i<8;++i) {
        now+=100000;access.poll(now,true,false);
        assert(!telemetry.pending && !SccbAsync_Owned(&bus));
        GigeSccbGuard guard(access,now);assert(guard);
    }
    now+=200000;access.poll(now,false,false);now=telemetry.next_due;
    access.poll(now,true,true);assert(telemetry.pending);
    for(unsigned i=0;i<80;++i)access.poll(++now,false,true);
    unsigned before=publications;
    {GigeSccbGuard guard(access,now);assert(!guard);}
    assert(telemetry.paused && SccbAsync_Owned(&bus));
    // Readiness/start deferral never prevents cancellation cleanup.
    for(unsigned i=0;i<2000 && telemetry.pending;++i)access.poll(++now,false,false);
    assert(!telemetry.pending && !SccbAsync_Owned(&bus) && publications==before);
    {GigeSccbGuard outer(access,now);assert(outer);
        {GigeSccbGuard inner(access,now);assert(inner);
            uint8_t value=0x12,readback=0;
            assert(!AxiGpioSccb_Write(&sync_bus,0x3c,0x350a,&value,1));
            assert(!AxiGpioSccb_Read(&sync_bus,0x3c,0x350a,&readback,1));
            assert(readback==value && !SccbAsync_Owned(&bus));
        }
        unsigned calls=io_calls;access.poll(now+100,true,true);assert(io_calls==calls);
    }
    now+=100;access.poll(now,true,true);assert(!telemetry.pending);
    uint64_t next=telemetry.next_due;
    access.poll(next-1,true,true);assert(!telemetry.pending);
    access.poll(next,true,true);assert(telemetry.pending);
    now=next;
    for(unsigned i=0;i<2000 && telemetry.pending;++i)access.poll(++now,false,true);
    assert(!telemetry.pending && publications==before+1);
    // Reinitialize only under an acquired guard (same lifecycle as reset/init).
    {GigeSccbGuard guard(access,now);assert(guard);telemetry={};
        assert(!GigeSensorTelemetry_Init(&telemetry,&bus,0x3c,100000,nullptr,publish));
        assert(!telemetry.valid);
    }
    access.poll(++now,false,true);assert(telemetry.next_due>now);
    // Legacy transport has no telemetry bus and permits nested synchronous work.
    GigeSensorTelemetry legacy{};GigeSccbAccess legacy_access(legacy);
    {GigeSccbGuard guard(legacy_access,now);assert(guard);legacy_access.poll(now,true,true);}
    // Backward-clock errors stay paused without an explicit synchronous boundary.
    now=telemetry.next_due;access.poll(now,true,true);access.poll(now-1,false,true);
    for(unsigned i=0;i<2000 && telemetry.pending;++i)access.poll(now+i,false,true);
    assert(telemetry.paused && !telemetry.pending && !SccbAsync_Owned(&bus));
    puts("PASS: production SCCB arbitration, cancellation drain, nested access, retry window, reset cache and legacy path");
}
