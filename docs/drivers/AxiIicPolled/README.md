# AXI IIC diagnostic polled transport

Sources: `AxiIicPolled.c/.h`. This independent register-level implementation is a
bounded diagnostic alternative to MaxIIC; it does not use XIic ISR callbacks or
an XIic instance. It requires exclusive ownership of the physical controller.

`AxiIicPolled_Init(context, base, hz, hold_cycles)` configures timing for the
200 MHz AXI clock. `AxiIicPolled_Read` and `AxiIicPolled_Write` use unshifted
7-bit slave addresses, 16-bit sensor register addresses and byte payloads.
The callbacks have the same transport shape used by the sensor driver.

The context records status, interrupts and phase for failure diagnosis. Timeout
is bounded; success must be checked before using read data. The interface is
single-register diagnostic transport, not a concurrent replacement for active
GPIO SCCB or the camera lifecycle. Use only a matching FPGA address map.

Example sequence: initialize an unowned context, bind its callbacks to an isolated
sensor transport experiment, read chip ID, then release the experiment. Do not
run a second bus master beside production firmware. Source-level offline checks
and preserved transport experiments establish different evidence from a new
physical bus qualification; this guide triggers no bus transaction.
