# GPIO SCCB synchronous transport

Sources: `AxiGpioSccb.c/.h`. GPIO bit0 is SCL and bit1 SDA. Open-drain behavior is
implemented by keeping DATA zero and changing TRI to release a line. Driving
DATA high is not an acceptable substitute for releasing the bus.

`AxiGpioSccb_Init(context, base, hz)` initializes the matching FPGA GPIO bank.
`AxiGpioSccb_Read`/`Write` accept a 7-bit device address, 16-bit sensor register
and byte data/count. The context contains half-period, timeout, phase, status
and interrupt diagnostics. Clock stretching is bounded; retries are not implicit.

## Integration

The OV5640 normally uses address0x3c. Initialize once, then bind callbacks to the
sensor context. A synchronous lifecycle operation must acquire the camera's SCCB
guard and finish any cancelled asynchronous telemetry transaction first. Never
call this transport while `SccbAsync_Owned()` remains true.

Dependencies are platform MMIO/timing and the exact GPIO-capable FPGA image.
This is not compatible with blindly treating an AXI-IIC peripheral as GPIO.
Configuration and stream controls belong to the higher camera lifecycle. A
successful register transfer does not prove frame-effective exposure or optical
image quality. See the [asynchronous contract](../AxiGpioSccbAsync/README.md).
