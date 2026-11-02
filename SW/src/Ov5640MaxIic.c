/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "Ov5640MaxIic.h"
#include "sleep.h"
static int sensor_read(void *bus, uint8_t a, uint16_t r, uint8_t *p, uint16_t n)
{
    uint16_t i;
    /* Linux ov5640 uses an individually addressed transfer for each byte. */
    for (i = 0; i < n; ++i)
    {
        int e = (int)MaxIIC_Read((MAX_IIC *)bus, a, (uint16_t)(r + i), 2, p + i, 1);
        if (e)
            return e;
    }
    return 0;
}
static int sensor_write(void *bus, uint8_t a, uint16_t r, const uint8_t *p, uint16_t n)
{
    uint16_t i;
    /* Linux ov5640 uses an individually addressed transfer for each byte. */
    for (i = 0; i < n; ++i)
    {
        int e = (int)MaxIIC_Write((MAX_IIC *)bus, a, (uint16_t)(r + i), 2, p + i, 1);
        if (e)
            return e;
    }
    return 0;
}
static void sensor_delay(void *bus, uint32_t us)
{
    (void)bus;
    usleep(us);
}
OV5640_RESULT Ov5640_BindMaxIic(OV5640 *s, MAX_IIC *bus)
{
    OV5640_IO io;
    if (!bus || !bus->initialized)
        return OV5640_E_ARGUMENT;
    io.user = bus;
    io.read = sensor_read;
    io.write = sensor_write;
    io.delay_us = sensor_delay;
    return Ov5640_Bind(s, &io, OV5640_ADDRESS);
}
