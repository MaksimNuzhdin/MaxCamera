# OV5640 sensor driver

The deployed module is [ALINX AN5641](https://www.en.alinx.com/Product/Add-on-Modules/AN5641.html), containing
the OV5640 sensor. The portable driver operates the sensor, not every function
of a vendor carrier or module.

Sources: `Ov5640.c/.h`, `Ov5640Registers.h`, `Ov5640Tuning.inc` and
`Ov5640MaxIic.c/.h` in the application root. This portable C driver configures
sensor clocks, windowing, format, exposure, gains and streaming through supplied
transport/delay callbacks. It is not a CSI receiver or frame-buffer driver.

## API and operation

Bind one persistent `OV5640` context to the chosen transport. `Ov5640_Init` or
`Ov5640_InitWithClock` performs reset and initialization; `Ov5640_DefaultConfig`
provides a configuration to refine; `Ov5640_Configure` programs the mode.
`Ov5640_Stream(camera, 1)` enables output only after CSI/DMA are ready;
`Ov5640_Stream(camera, 0)` blocks frame output. Stop is not equivalent to power-off.

`Ov5640_SetAutoExposure` selects exposure/gain automation. Manual
`Ov5640_SetExposure(camera, lines, gain16)` uses grouped sensor writes; its success
is not evidence that the next frame already has that exposure. Validate clock,
line period and exposure bounds before computing microsecond controls.

```c
OV5640_CONFIG config;
Ov5640_DefaultConfig(&config);
/* Set verified physical XCLK, interface, format and geometry before Configure. */
OV5640_RESULT result = Ov5640_Configure(&camera, &config);
/* Check result; configure matching CSI/DMA before Ov5640_Stream(&camera, 1). */
```

## Dependencies and limitations

The MaxIIC adapter depends on the Xilinx BSP; the core driver does not. Serialize
all synchronous calls against asynchronous SCCB telemetry, draining cancellation
before acquiring the bus. Do not retry partially applied writes automatically.
Errors retain transport/register diagnostics. RAW Bayer phase and supported
formats depend on the complete camera pipeline, not just sensor capability.

See [full sensor guide](../../../SW/OV5640_README.md),
[third-party provenance](../../../SW/THIRD_PARTY_OV5640.md) and
[transport guide](../MaxIic/README.md). Optical calibration and HDR exposure-frame
association remain unqualified. Examples here do not execute hardware tests.
