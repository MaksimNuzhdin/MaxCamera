# Fan PWM/hysteresis controller

Sources: sibling `drivers/MaxFanController/`. Depends on project MMIO macros,
`defines.h`, math conversion and matching PL fan IP.

| Offset | Register | Payload |
| --- | --- | --- |
| 0x00 | Threshold | Temperature code, documented bits9:0 |
| 0x04 | Floor threshold | Lower hysteresis code, bits9:0 |
| 0x08 | Duty cycle | PWM code, bits7:0 |

The driver converts Celsius to `round((C+273.15)*4096/(4*503.975))`.
`maxFanController_Init` writes all three values; setters and ReadReg/WriteReg
perform MMIO. The higher GenICam layer supplies user-facing percent/temperature
validation; this low-level wrapper does not clamp or validate every raw value.

Example: initialize with verified board base and safe thresholds, then call
`maxFanController_SetThreshold`/`SetFloorThreshold` while maintaining floor below
start threshold. Duty is a hardware code here, not necessarily user percent.

`maxFanController_selfTest` writes0x20/0x30/0x10 and compares readback; it does
**not restore previous values**. It must not be presented as a harmless runtime
health check. No RPM sensing or fan-stall guarantee is implemented. Header linkage
and multi-language integration need review before publishing a reusable library.
The current documentation does not run that hardware self-test.
