# FPGA temperature GPIO adapter

Sources: sibling `drivers/TempController/`. The driver reads a PL temperature
sample through XGpio; it does not measure OV5640 sensor temperature.

`temp_driver_init(context, base, gpio_ref)` records caller-owned XGpio storage,
initializes it and sets channel1 direction. `temp_driver_get_temp(context, &celsius)`
reads channel1 and computes `((raw << 2)*503.975/4096)-273.15`.

```c
float temperature;
MAX_RESULT rc = temp_driver_get_temp(&temperature_controller, &temperature);
/* Report only from a previously verified initialized board context. */
```

Dependencies are `defines.h`, XGpio and the matching PL temperature-code format.
The current initializer logs XGpio failure but still returns OK, and configures
GPIO direction as zero. These are existing implementation limitations requiring
review against the actual read-only PL wiring; callers must not interpret the
return alone as comprehensive initialization proof. No timestamp, averaging,
calibration uncertainty or sensor fault code is provided by this wrapper.
No new thermal calibration or hardware read was performed for this README.
