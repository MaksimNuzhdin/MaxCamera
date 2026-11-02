// Reuse the independently implemented SCCB slave and the synchronous baseline.
#define main synchronous_regression_main
#include "../gpio_sccb/test_gpio.cpp"
#undef main
#include "AxiGpioSccbAsync.h"
#include "gige/GigeSensorTelemetry.h"

static unsigned io_calls;
static void release(void *,unsigned mask){++io_calls;slave.drive(mask);}
static unsigned levels(void *){++io_calls;return slave.levels();}
static SccbAsync async_bus(unsigned transaction_us=100000,unsigned hz=50000)
{
    (void)setup();SccbAsync s{};SccbAsyncIo io={nullptr,release,levels};
    assert(!SccbAsync_Init(&s,&io,hz,1000,transaction_us));return s;
}
static void poll(SccbAsync &s,uint64_t now)
{
    io_calls=0;uint64_t sleeps=slave.elapsed;SccbAsync_Poll(&s,now);
    assert(io_calls<=2 && slave.elapsed==sleeps);
}
static int complete(SccbAsync &s,uint64_t &now,uint8_t &value,unsigned step=1)
{
    int result=123;
    for(unsigned n=0;n<200000;++n,now+=step) {
        poll(s,now);
        if(SccbAsync_Take(&s,&value,&result))return result;
    }
    assert(false);return 123;
}
static unsigned publications;
static void publish(void *,uint16_t reg,uint8_t value)
{assert(slave.regs[reg]==value);++publications;}
static void telemetry_complete(GigeSensorTelemetry &s,uint64_t &now)
{
    for(unsigned n=0;s.pending && n<200000;++n,++now)GigeSensorTelemetry_Poll(&s,now,0);
    assert(!s.pending);
}
int main()
{
    synchronous_regression_main();
    for(unsigned hz: {10000U,50000U,100000U}) for(unsigned step: {1U,7U,200U}) {
        SccbAsync s=async_bus(100000,hz);uint64_t now=0;uint8_t value=0;
        assert(!SccbAsync_BeginRead(&s,0x3c,0x300a,now));
        assert(s.count<=256 && SccbAsync_Owned(&s));
        assert(SccbAsync_BeginRead(&s,0x3c,0x300b,now)==SCCB_ASYNC_BUSY);
        assert(!complete(s,now,value,step) && value==0x56);
        assert(slave.starts==2 && slave.stops==2 && slave.master_nack);
        assert((slave.sent==std::vector<uint8_t>{0x78,0x30,0x0a,0x79}));
        assert(!SccbAsync_Owned(&s) && slave.tri==3);
    }
    for(unsigned nack=1;nack<=4;++nack) {
        SccbAsync s=async_bus();slave.fail_byte=nack;uint64_t now=0;uint8_t value=0xa5;
        assert(!SccbAsync_BeginRead(&s,0x3c,0x300a,now));
        assert(complete(s,now,value)==SCCB_ASYNC_NACK && value==0xa5);
        assert(slave.bytes==nack && !slave.writes && slave.tri==3);
    }
    for(unsigned cancel_at: {0U,10U,500U,1100U}) {
        SccbAsync s=async_bus();uint64_t now=0;uint8_t value=0xa5;
        assert(!SccbAsync_BeginRead(&s,0x3c,0x300a,now));
        for(;now<cancel_at;++now)poll(s,now);
        SccbAsync_Cancel(&s,now);SccbAsync_Cancel(&s,now);
        assert(complete(s,now,value)==SCCB_ASYNC_CANCELLED && value==0xa5);
        assert(slave.tri==3 && !SccbAsync_Owned(&s) && !slave.writes);
    }
    for(unsigned nack=1;nack<=4;++nack) {
        SccbAsync s=async_bus();slave.fail_byte=nack;uint64_t now=0;uint8_t value=0xa5;
        assert(!SccbAsync_BeginRead(&s,0x3c,0x300a,now));
        SccbAsync_Cancel(&s,now);
        assert(complete(s,now,value)==SCCB_ASYNC_NACK && value==0xa5);
    }
    {
        SccbAsync s=async_bus(20);uint64_t now=0;uint8_t value=0xa5;
        assert(!SccbAsync_BeginRead(&s,0x3c,0x300a,now));
        uint64_t deadline=s.deadline;SccbAsync_Cancel(&s,now);
        assert(s.deadline==deadline);
        assert(complete(s,now,value)==SCCB_ASYNC_TIMEOUT && value==0xa5);
        assert(now<1100 && slave.tri==3);
    }
    {
        SccbAsync s=async_bus();slave.stuck_scl=true;uint64_t now=0;uint8_t value=0xa5;
        assert(!SccbAsync_BeginRead(&s,0x3c,0x300a,now));
        assert(complete(s,now,value)==SCCB_ASYNC_TIMEOUT && value==0xa5);
        assert(now<2100 && slave.tri==3);
    }
    {
        SccbAsync s=async_bus(20);uint64_t now=0;uint8_t value=0xa5;
        assert(!SccbAsync_BeginRead(&s,0x3c,0x300a,now));
        assert(complete(s,now,value)==SCCB_ASYNC_TIMEOUT && value==0xa5);
        assert(now<1100 && slave.tri==3);
    }
    {
        SccbAsync s=async_bus();slave.stretch_read_stop=true;uint64_t now=0;uint8_t value=0xa5;
        assert(!SccbAsync_BeginRead(&s,0x3c,0x300a,now));
        assert(complete(s,now,value)==SCCB_ASYNC_TIMEOUT && value==0xa5 && slave.tri==3);
    }
    {
        SccbAsync s=async_bus();slave.stuck_sda=true;uint64_t now=0;uint8_t value=0xa5;
        assert(!SccbAsync_BeginRead(&s,0x3c,0x300a,now));
        assert(complete(s,now,value)==SCCB_ASYNC_BUS && value==0xa5 && slave.tri==3);
    }
    {
        SccbAsync s=async_bus();slave.stuck_scl=true;uint64_t now=0;uint8_t value=0;
        assert(!SccbAsync_BeginRead(&s,0x3c,0x300a,now));
        for(;now<100;++now)poll(s,now);
        slave.stuck_scl=false;
        assert(!complete(s,now,value) && value==0x56);
    }
    {
        SccbAsync s=async_bus();uint64_t now=0;uint8_t value=0xa5;
        assert(!SccbAsync_BeginRead(&s,0x3c,0x300a,now));
        poll(s,now); // SCL is already high; delayed service is not stretching.
        now=1001;poll(s,now);
        assert(complete(s,now,value)==0 && value==0x56);
    }
    {
        SccbAsync s=async_bus();slave.stuck_scl=true;uint64_t now=0;uint8_t value=0xa5;
        assert(!SccbAsync_BeginRead(&s,0x3c,0x300a,now));
        poll(s,now); // Actually low at release: a late high remains unqualified.
        slave.stuck_scl=false;now=1001;poll(s,now);
        assert(complete(s,now,value)==SCCB_ASYNC_TIMEOUT && value==0xa5);
    }
    {
        SccbAsync s=async_bus(1000);uint64_t now=0;uint8_t value=0xa5;
        assert(!SccbAsync_BeginRead(&s,0x3c,0x300a,now));
        poll(s,now);now=1001;poll(s,now);
        assert(complete(s,now,value)==SCCB_ASYNC_TIMEOUT && value==0xa5);
    }
    {
        SccbAsync s=async_bus();uint64_t now=0;uint8_t value=0xa5;int result=123;
        assert(SccbAsync_BeginRead(&s,0x78,0x300a,now)==SCCB_ASYNC_ARGUMENT);
        assert(!SccbAsync_BeginRead(&s,0x3c,0x300a,now));
        assert(!SccbAsync_Take(&s,&value,&result) && value==0xa5 && result==123);
        for(;now<2000;++now)poll(s,now);
        assert(SccbAsync_Owned(&s)); // Completion remains owned until consumed.
        assert(SccbAsync_BeginRead(&s,0x3c,0x300b,now)==SCCB_ASYNC_BUSY);
        assert(SccbAsync_Take(&s,&value,&result) && result==0 && value==0x56);
    }
    {
        SccbAsync bus=async_bus();GigeSensorTelemetry t{};publications=0;
        assert(!GigeSensorTelemetry_Init(&t,&bus,0x3c,100000,nullptr,publish));
        uint64_t now=0;uint8_t value=0xa5;slave.regs[0x3500]=42;
        assert(!GigeSensorTelemetry_Read(&t,0x3500,&value) && value==0xa5);
        GigeSensorTelemetry_Poll(&t,now,0);assert(!t.pending);
        GigeSensorTelemetry_Poll(&t,now,1);assert(t.pending);
        telemetry_complete(t,now);
        assert(publications==1 && GigeSensorTelemetry_Read(&t,0x3500,&value) && value==42);
        GigeSensorTelemetry_Poll(&t,now,1);assert(!t.pending);
        // Rotate through the full list; failure of the next cycle keeps last-good data.
        for(unsigned i=1;i<14;++i) {
            now=t.next_due;GigeSensorTelemetry_Poll(&t,now,1);telemetry_complete(t,now);
        }
        assert(publications==14 && t.valid==0x3fff);
        slave.regs[0x3500]=99;slave.fail_byte=slave.bytes+1;
        now=t.next_due;GigeSensorTelemetry_Poll(&t,now,1);telemetry_complete(t,now);
        assert(t.last_error==SCCB_ASYNC_NACK && t.error_register==0x3500);
        assert(publications==14 && GigeSensorTelemetry_Read(&t,0x3500,&value) && value==42);
        now=t.next_due;GigeSensorTelemetry_Poll(&t,now,1);assert(t.pending);
        GigeSensorTelemetry_Pause(&t,now);GigeSensorTelemetry_Resume(&t,now);
        assert(t.paused);telemetry_complete(t,now);assert(publications==14);
        now+=1000000;GigeSensorTelemetry_Poll(&t,now,1);assert(!t.pending);
        GigeSensorTelemetry_Resume(&t,now);GigeSensorTelemetry_Poll(&t,now,1);
        telemetry_complete(t,now);assert(publications==15);
        // A completed-but-unpublished transaction is discarded by pause too.
        now=t.next_due;GigeSensorTelemetry_Poll(&t,now,1);
        for(unsigned n=0;n<2000;++n,++now)poll(bus,now);
        GigeSensorTelemetry_Pause(&t,now);telemetry_complete(t,now);
        assert(publications==15 && !SccbAsync_Owned(&bus));
    }
    {
        // A reset/wrapped clock must not strand an owned transaction until
        // the old time value is reached again.
        SccbAsync s=async_bus();uint64_t now=1000000;uint8_t value=0xa5;
        assert(!SccbAsync_BeginRead(&s,0x3c,0x300a,now));poll(s,now);
        now=0;
        int result=123;bool done=false;
        for(;now<3000;++now){poll(s,now);if(SccbAsync_Take(&s,&value,&result)){done=true;break;}}
        assert(done && result==SCCB_ASYNC_ARGUMENT && value==0xa5 && slave.tri==3);
    }
    {
        SccbAsync bus=async_bus();GigeSensorTelemetry t{};publications=0;
        assert(!GigeSensorTelemetry_Init(&t,&bus,0x3c,100000,nullptr,publish));
        GigeSensorTelemetry_Poll(&t,1000000,0);
        GigeSensorTelemetry_Poll(&t,0,1);
        assert(t.paused && !t.pending && t.last_error==SCCB_ASYNC_ARGUMENT);
        GigeSensorTelemetry_Resume(&t,0);GigeSensorTelemetry_Poll(&t,0,1);
        uint64_t now=0;telemetry_complete(t,now);assert(publications==1);
    }
    {
        // Exercise cancellation at every instruction boundary, including a
        // completed read whose final hold time has not yet been consumed.
        SccbAsync template_bus=async_bus();
        assert(!SccbAsync_BeginRead(&template_bus,0x3c,0x300a,0));
        unsigned count=template_bus.count;
        for(unsigned position=0;position<=count;++position) {
            SccbAsync s=async_bus();uint64_t now=0;uint8_t value=0xa5;
            assert(!SccbAsync_BeginRead(&s,0x3c,0x300a,now));
            while(s.position<position){poll(s,now);++now;assert(now<10000);}
            SccbAsync_Cancel(&s,now);
            uint64_t deadline=s.deadline;
            SccbAsync_Cancel(&s,now+1);assert(s.deadline==deadline);
            assert(complete(s,now,value)==SCCB_ASYNC_CANCELLED);
            assert(value==0xa5 && slave.tri==3 && !slave.writes);
            assert((slave.levels()&3U)==3U);
            // Reuse the released physical bus without reset or recovery clocks.
            AXI_GPIO_SCCB synchronous{};synchronous.base=0xa00d0000U;
            synchronous.half_us=10;synchronous.timeout_us=1000;synchronous.ready=1;
            uint8_t id[2]={0,0};
            assert(!AxiGpioSccb_Read(&synchronous,0x3c,0x300a,id,2));
            assert(id[0]==0x56 && id[1]==0x40);
        }
    }
    {
        SccbAsync s=async_bus();uint8_t value=0xa5;int result=123;
        assert(!SccbAsync_BeginRead(&s,0x3c,0x300a,UINT64_MAX-2));
        assert(s.deadline==UINT64_MAX);
        poll(s,UINT64_MAX-2);poll(s,UINT64_MAX);
        poll(s,UINT64_MAX); // Saturated cleanup deadline must still terminate.
        assert(SccbAsync_Take(&s,&value,&result));
        assert(result==SCCB_ASYNC_TIMEOUT && value==0xa5 && slave.tri==3);
    }
    {
        SccbAsync s=async_bus();uint64_t now=UINT64_MAX-2;uint8_t value=0xa5;
        assert(!SccbAsync_BeginRead(&s,0x3c,0x300a,now));poll(s,now);
        now=0;assert(complete(s,now,value)==SCCB_ASYNC_ARGUMENT && slave.tri==3);
    }
    {
        SccbAsync bus=async_bus();GigeSensorTelemetry t{};publications=0;
        assert(!GigeSensorTelemetry_Init(&t,&bus,0x3c,100000,nullptr,publish));
        uint64_t now=UINT64_MAX-2;
        GigeSensorTelemetry_Poll(&t,now,1);assert(t.next_due==UINT64_MAX);
        now=0;telemetry_complete(t,now);
        assert(t.paused && !publications && t.last_error==SCCB_ASYNC_ARGUMENT);
        assert(!SccbAsync_Owned(&bus));
    }
    std::puts("PASS: asynchronous SCCB bounded polls, waveform, cancellation, errors and last-good telemetry cache");
}
