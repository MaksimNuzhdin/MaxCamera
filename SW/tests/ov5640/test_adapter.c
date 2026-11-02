#include "Ov5640MaxIic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned calls, fail_at;
static uint16_t first;
static MAX_IIC_RESULT check(uint8_t address, uint16_t reg, uint8_t reg_bytes, uint16_t count)
{
    assert(address == 0x3c && reg_bytes == 2 && count == 1);
    assert(reg == first + calls);
    ++calls;
    return calls == fail_at ? MAX_IIC_NACK : MAX_IIC_OK;
}
MAX_IIC_RESULT MaxIIC_Read(MAX_IIC *bus, uint8_t a, uint16_t r, uint8_t rb, uint8_t *p, uint16_t n)
{
    (void)bus;
    MAX_IIC_RESULT e = check(a, r, rb, n);
    if (!e)
        *p = (uint8_t)r;
    return e;
}
MAX_IIC_RESULT MaxIIC_Write(MAX_IIC *bus, uint8_t a, uint16_t r, uint8_t rb, const uint8_t *p,
                            uint16_t n)
{
    (void)bus;
    assert(*p == (uint8_t)r);
    return check(a, r, rb, n);
}
int usleep(unsigned long us)
{
    (void)us;
    return 0;
}
int main(void)
{
    OV5640 s;
    MAX_IIC bus;
    uint8_t p[3] = {0x0a, 0x0b, 0x0c};
    memset(&bus, 0, sizeof(bus));
    bus.initialized = 1;
    assert(Ov5640_BindMaxIic(&s, &bus) == 0);
    first = 0x300a;
    assert(Ov5640_Read(&s, first, p, 3) == 0 && calls == 3);
    calls = 0;
    assert(Ov5640_Write(&s, first, p, 3) == 0 && calls == 3);
    calls = 0;
    fail_at = 2;
    assert(Ov5640_Read(&s, first, p, 3) == OV5640_E_IO && calls == 2);
    calls = 0;
    assert(Ov5640_Write(&s, first, p, 3) == OV5640_E_IO && calls == 2 && s.state == OV5640_FAULT);
    puts("OV5640 MaxIIC adapter: individually addressed bytes, early error stop passed");
    return 0;
}
