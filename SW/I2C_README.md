# I2C

Shared synchronous driver: `MaxIicDriver.h` / `MaxIicDriver.c`.
The old `IIC_PS_driver.c/.h` scanner and EEPROM-specific code have been removed.

## Buses in the current hardware design

Checked `design_1.bd`, AXI IIC XCI files, `Constrains.xdc`, and BSP `xparameters.h`.

| Bus | Controller | Base address | Connection | SCL frequency |
|---|---|---|---|---|
| `MAX_IIC_CAM0` | AXI IIC 0 | `0xA0020000` | cam0, SCL AH13 / SDA AE13 | 10 / 50 / 100 kHz; 50 kHz verified |
| `MAX_IIC_CAM1` | AXI IIC 1 | `0xA0030000` | cam1, SCL AD14 / SDA AC13 | 10 / 50 / 100 kHz; 50 kHz verified |
| `MAX_IIC_PS1` | PS I2C1 | `0xFF030000` | MIO 32-33 | 100 or 400 kHz |

The AXI clock is 200 MHz, not the I2C bus frequency. The PS reference clock is
approximately 100 MHz and is also distinct from I2C. For AXI, the driver programs
timing registers for a 200 MHz input; 10/50/100 kHz are supported. SDA hold is
200 cycles (approximately 1 us). The current OV5640 module was verified at
50 kHz; NACKs were observed at 100 kHz.

`main.cpp` initializes camera buses at 50 kHz and PS I2C at 100 kHz, reporting
initialization failures. Startup reads the OV5640 ID once on each camera bus.
Extended tests and sensor register modifications are disabled during normal
startup. Load the PL bitstream and configure PS clocks/MIO using the normal
project launch procedure before initialization.

## Interface

```c
MAX_IIC_RESULT MaxIIC_Init(MAX_IIC *iic, MAX_IIC_BUS bus,
                          uint32_t clock_hz, uint32_t timeout_us);
MAX_IIC_RESULT MaxIIC_Read(MAX_IIC *iic, uint8_t slave, uint16_t reg,
                          uint8_t reg_bytes, uint8_t *data, uint16_t count);
MAX_IIC_RESULT MaxIIC_Write(MAX_IIC *iic, uint8_t slave, uint16_t reg,
                           uint8_t reg_bytes, const uint8_t *data, uint16_t count);
```

- `slave`: unshifted 7-bit address `0x08..0x77`, without the R/W bit.
- `reg_bytes = 0`: no register address; `reg` must be 0.
- `reg_bytes = 1`: register address 0..255.
- `reg_bytes = 2`: register address 0..65535, most significant byte first.
- `count`: 1..256 data bytes. The buffer must be large enough.
- The caller selects data byte order according to the device datasheet.
- `MAX_IIC_OK` means the entire operation completed and released the bus.
- Errors: `INVALID_ARGUMENT`, `NOT_INITIALIZED`, `BUSY`, `TIMEOUT`, `NACK`,
  `ARBITRATION_LOST`, `IO_ERROR`, all prefixed with `MAX_IIC_`.

Write: START -> slave+W -> register (if present) -> all data -> STOP.
Register read: START -> slave+W -> register -> repeated START -> slave+R -> data -> STOP.
Read without register: START -> slave+R -> data -> STOP.

Example (address `0x36` and register values are placeholders; use the device datasheet):

```c
#include "MaxIicDriver.h"

static MAX_IIC camera;

MAX_IIC_RESULT example(void)
{
    uint8_t value = 0x01;
    uint8_t result[2];
    MAX_IIC_RESULT status = MaxIIC_Init(&camera, MAX_IIC_CAM0, 50000U,
                                        MAX_IIC_DEFAULT_TIMEOUT_US);
    if (status != MAX_IIC_OK) return status;
    status = MaxIIC_Write(&camera, 0x36, 0x0100, 2, &value, 1);
    if (status != MAX_IIC_OK) return status;
    return MaxIIC_Read(&camera, 0x36, 0x0000, 2, result, sizeof(result));
}
```

## Waiting and error handling

BSP state machines are serviced by polling `XIic_InterruptHandler` /
`XIicPs_MasterInterruptHandler`. GIC configuration and global exceptions are not
required. Do not simultaneously connect these controllers to an ISR: the driver
must have exclusive ownership of the controller state machine.

Use one persistent context per physical bus and one calling thread. `active`
prevents re-entry in ordinary sequential code; it is not an atomic RTOS mutex.
Do not call from an ISR or copy an initialized context: the BSP stores a callback
to that context's address.

`timeout_us` is the total budget of 1 us polling delays for the whole operation,
including waiting for an idle bus and both read phases. Actual elapsed time may
be longer because of execution and interrupt overhead. Default: 100000 us.
After an error in a started transfer, the controller is reset and reconfigured
to release HOLD and avoid leaving a transfer active with a temporary buffer.
If reconfiguration fails, the context becomes uninitialized. A controller reset
cannot release an external device holding SDA/SCL. If the bus is busy before a
transfer starts, the driver waits until timeout without resetting it.

There are no automatic retries: a failed write may already have partially
reached the device. EEPROM page boundaries, write-cycle delays, ACK polling,
address-encoded memory banks, device-specific SMBus commands, and PEC belong in
the individual device driver. The old scanner's dummy-byte write is not used
because it can change device state.

## Verification

```sh
gcc -std=c11 -Wall -Wextra -Werror -Itests/i2c/stubs -I. \
    MaxIicDriver.c tests/i2c/test_i2c.c -o /tmp/maxcamera-i2c-tests
/tmp/maxcamera-i2c-tests
```

Host tests cover all three buses, the full 256-byte payload, register prefixes,
raw transfers, repeated START, validation, NACK, arbitration loss, timeout,
reuse after an error, and a busy bus without reset. They emulate the BSP API,
not electrical signals on the board. Hardware verification needs known slave
addresses/registers and an available board; check ACK, byte order, repeated
START/STOP, and bounded behavior with the device disconnected.

## Hardware verification, October 22, 2026

CAM0 / J23 / 0x3C: 1000/1000 reads of ID 0x5640 at 50 kHz, without errors or
automatic retries. Register 0x503D was written 0x00 -> 0x80 -> 0x00 and read
back successfully. CAM1 with no camera returns NACK and remains initialized.

After a power cycle, a bitstream with CAM_PWUP=1 on AH14/AD15 is required.
The fix is present in hardware logic.v / Constrains.xdc; the complete source
design had not yet been rebuilt after that change. The verified bitstream
with two added enable outputs is `build/camera-enable-eco/logic-camera-enable.bit`;
`_ide/launch.json` and `tools/run_board.tcl` point to it. If build/ is removed,
recreate it with tools/build_camera_enable_eco.tcl or rebuild the hardware and
update the launch path. Flash was not programmed.

Full logs: diagnostics/i2c-timing-matrix-uart.txt,
diagnostics/i2c-timing-stress-uart.txt, diagnostics/i2c-driver-1000-uart.txt.
MIPI frame reception is outside this verification. According to the placement
report, I2C CAM0 belongs to J23 but its MIPI receiver has index 1;
CAM1/J24 uses MIPI receiver index 0.
