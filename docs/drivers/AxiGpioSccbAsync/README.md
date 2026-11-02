# Nonblocking SCCB telemetry transport

Sources: `AxiGpioSccbAsync.c/.h`; cache/lifecycle integration lives in `gige/`.
The transport reads one sensor byte without sleeping in the camera poll loop.
Callbacks release open-drain lines through TRI and sample levels; DATA stays zero.

## API and scheduling

`SccbAsync_Init` takes callbacks, bus frequency, stretch timeout and transaction
budget. Call it only for an unowned context; it does not recover GPIO hardware.
`SccbAsync_BeginRead` starts one 7-bit-address/16-bit-register read.
`SccbAsync_Poll(now_us)` performs at most two callbacks. Time must be monotonic
microseconds, independent of the resettable device timestamp.

```c
SccbAsync_Poll(&bus, monotonic_us);
uint8_t value; int result;
if (SccbAsync_Take(&bus, &value, &result)) {
    /* Publish value only when result == 0; retain last-good cache on failure. */
}
```

`SccbAsync_Cancel` drains the current transaction, including final master NACK
and STOP, under its original deadline. Continue polling until Take completes;
logical ownership is not released merely by requesting cancellation. Failed
reads never change the caller's value. There is no hidden retry.

## Limits and tests

Deadline cleanup requires CPU polling; it cannot release wires while the CPU is
stalled. Main-loop scheduling and high-rate telemetry deferral remain explicit
performance constraints. Cached exposure/gain is not immutable per-frame metadata.
See tests and contracts (archived development reference; see [verification scope](../../VERIFICATION.md)) and historical
SCCB evidence referenced by TODO. Offline cancellation, wrap, delay and bus-error
coverage does not replace board freshness/FPS acceptance for a new image.
