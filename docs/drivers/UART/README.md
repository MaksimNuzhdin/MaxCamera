# PS UART initialization adapter

Sources: sibling `drivers/UART/UART_driver.c/.h`. This wrapper initializes an
AMD XUartPs instance; it is not a buffered console, interrupt receiver or framing
protocol. The caller supplies persistent `XUartPs` storage and a verified BSP base.

`UART_driver_init(context, base, baud, instance)` looks up configuration,
initializes the instance and calls `XUartPs_SetBaudRate`. It stores base, baud and
instance in `UART_DRIVER`; fields retain their original spelling `m_baundRate`.

```c
XUartPs instance;
UART_DRIVER uart;
MAX_RESULT rc = UART_driver_init(&uart, verified_uart_base, 115200, &instance);
```

Lookup/initialization failure returns FAIL. The current code ignores the return
from SetBaudRate, so OK does not prove an arbitrary baud was accepted. It also
uses a global configuration pointer; multi-instance/concurrent initialization
needs review. Output/input operations come from BSP/console code outside this
wrapper. Never attach a competing UART consumer during a board-owner session.
No UART bytes were read or sent for this guide.
