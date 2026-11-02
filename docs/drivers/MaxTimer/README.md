# AXI timer adapter

Sources: sibling `drivers/MaxTimer/`. Uses XTmrCtr and project `defines.h`.
`maxTimer_init(context, base, index, frequency)` initializes/self-tests the device,
selects auto-reload, sets reset value0 and starts the chosen timer.
`maxTImer_getValue` (capitalization retained from the public API) reads the counter;
`maxTimer_resetTimer` resets it.

```c
uint32_t ticks;
maxTImer_getValue(&timer, &ticks);
uint32_t elapsed = ticks - previous_ticks; /* modulo arithmetic, bounded interval */
```

`frequency` is stored for callers; the function does not synthesize a new clock.
Use the verified clock rate to convert ticks and handle 32-bit wrap. The
implementation has one global `XTmrCtr TimerCounter`, so contexts are not fully
independent devices. Initialization and reset change shared timing state and
must not be used as casual diagnostics while camera timing depends on it.
A resettable counter is not automatically suitable for SCCB monotonic deadlines.
No timer operation is executed by this documentation.
