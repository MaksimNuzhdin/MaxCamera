# Camera and board integration

The owner-confirmed platform is [ALINX AXU2CGB](https://www.en.alinx.com/Product/SoC-Development-Boards/Zynq-UltraScale-plus-MPSoC/AXU2CGB.html) with
[ALINX AN5641](https://www.en.alinx.com/Product/Add-on-Modules/AN5641.html). This guide describes the interfaces actually
used by MaxCamera, not all peripherals advertised for the board.

Sources: `Ov5640Board.cpp/.h`, `gige/GigeCamera.cpp/.h` and
`gige/GigeVisionBoard.cpp/.h`. These are application integration modules rather
than portable peripheral libraries. They combine the BSP, sensor transport,
CSI/frame writer, cache handling, lwIP/GEM and the protocol engine.

`Ov5640Board_Init(MAX_IIC*)` initializes the legacy board sensor context
`g_camera`; its void API does not replace checking sensor state/diagnostics.
The GigE application has its own camera lifecycle and must not be combined with
a second independent sensor owner merely because both wrappers exist.

`GigeVisionBoard_Init` initializes the board/network application.
`GigeVisionBoard_Poll` services bounded foreground work. `Ready`, `Streaming`
and `StreamIndicatorActive` report different states: link/control availability
is not acquisition. `GigeVisionBoard_NowUs` supplies the monotonic clock used for
timeouts; device timestamp reset must not change that deadline clock.

```c
int result = GigeVisionBoard_Init();
if (result == 0) {
    /* Integrate Poll into the established foreground loop, not a second loop. */
    GigeVisionBoard_Poll();
}
```

Capture buffers remain owned until DMA/transport/resend lifetime permits reuse.
Stop drains an in-flight frame; wait for idle before reconfiguration. Recovery
can release protocol ownership, so report a primary capture error separately
from a subsequent access-denied cleanup. Runtime restoration needs a fresh valid
lease after verifying no other controller owns the device.

The approved November1 scene collection restored the original runtime exactly
and released ownership after a4x upscale timeout. No new load/Flash operation
was used to hide that failure. See [architecture](../../ARCHITECTURE.md),
[operation guide](../../BUILD_AND_USE.md) and saved diagnostics. Do not invoke
initialization/recovery examples beside an active hardware owner.
