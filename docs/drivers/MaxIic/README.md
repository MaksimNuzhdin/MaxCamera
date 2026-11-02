# MaxIIC synchronous transport

Sources: `MaxIicDriver.c/.h` and `Ov5640MaxIic.c/.h`. One persistent `MAX_IIC`
context owns one bus; Xilinx XIic/XIicPs state machines are serviced by polling,
not a concurrently installed GIC callback.

| Bus | Controller/base | Allowed requested bus clocks |
| --- | --- | --- |
| MAX_IIC_CAM0 | AXI IIC, 0xA0020000 | 10/50/100 kHz |
| MAX_IIC_CAM1 | AXI IIC, 0xA0030000 | 10/50/100 kHz |
| MAX_IIC_PS1 | PS I2C1, 0xFF030000, MIO32/33 | 100/400 kHz |

These addresses describe the corresponding AXI-IIC image, not every later GPIO
SCCB FPGA image. AXI timing assumes 200 MHz input; requested bus rate is separate.

## API example

```c
MAX_IIC bus;
MAX_IIC_RESULT rc = MaxIIC_Init(&bus, MAX_IIC_CAM0, 50000, 100000);
uint8_t value;
if (rc == MAX_IIC_OK)
    rc = MaxIIC_Read(&bus, 0x3c, 0x300a, 2, &value, 1);
```

The slave address is unshifted 7-bit; register lengths are 0, 1 or 2 bytes, with
16-bit register prefix MSB-first. Transfers accept 1..256 data bytes. Read uses
repeated START; write sends prefix and payload as one transaction. EEPROM page
splitting and delays belong to callers. No automatic retry is performed.

## Failure and ownership contract

Results distinguish invalid arguments, not initialized, busy, timeout, NACK,
arbitration loss and I/O error. Default timeout is 100000 us and bounds polling
delay, not all CPU overhead. A failed write can have partly reached the device.
Never reinitialize or use another controller context during an owned transfer.
50 kHz was qualified with this sensor; historical 100 kHz NACKs are documented.
See [I2C guide](../../../SW/I2C_README.md) for exact historical evidence.
