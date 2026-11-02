# Board LED controller

Sources: sibling `drivers/LedsController/LedsController.c/.h`. Depends on XGpio
and the project `defines.h`. The four board outputs are active-low; logical one
means illuminated and the driver writes its inverse. Initialization writes inactive
data before enabling outputs to avoid a startup flash.

`leds_controller_init` takes base, GPIO direction mask, channel and mode.
`set_mode`, `process`, `set_status` and `stream` update the persistent context.
Status mode uses logical bit0 heartbeat, bit1 sensor-ready, bit2 GVCP-ready and
bit3 streaming. Call `process` at the desired heartbeat cadence; it contains no
own timer. `stream(context, independent, streaming, now_us)` produces a 2 Hz
indicator with 250 ms half-period, skipping missed edges without a busy wait.

```c
leds_controller_set_status(&leds, sensor_ready, gige_ready);
leds_controller_stream(&leds, independent_reset_mapping, actual_streaming, now_us);
```

## Image-specific electrical boundary

FPGA v10/v11 separate video reset on GPIO channel2 bit0, address0xA0010008.
LED3/bit3 is then pinAB13 and may blink. Legacy v9 aliases channel1 bit3 to video
reset: `independent` must be false. Do not select test modes that toggle all bits
on a legacy image. Existing status/error LEDs must remain intact.

Native tests under `tests/leds` and reset-mapping tests cover logical behavior.
Saved board evidence proves GPIO state and stream health, not an optical observer
seeing light. See TODO's LED evidence and [live-control guide](../../FPGA_LIVE_BLACK_LEVEL.md).
