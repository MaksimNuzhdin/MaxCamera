#ifndef AXI_GPIO_SCCB_ASYNC_H
#define AXI_GPIO_SCCB_ASYNC_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* One-byte telemetry reads only. GPIO DATA must remain zero; release(mask)
 * writes TRI bits (bit 0 SCL, bit 1 SDA). Callbacks must not block.
 * The caller owns bus arbitration with synchronous users: do not call them
 * while Owned() is true. Cancel drains the current read under its original
 * deadline, including the final master NACK and STOP; Poll until Take succeeds.
 * Time is monotonic microseconds, independent of the resettable device clock.
 * Poll performs at most two callbacks and never sleeps. Deadline detection
 * requires continued polling; it cannot release wires while the CPU is stalled.
 * Init is for an unowned context only and does not initialize/recover the GPIO. */
typedef struct {
    void *user;
    void (*release)(void *user, unsigned mask);
    unsigned (*levels)(void *user);
} SccbAsyncIo;
typedef struct { uint8_t kind, argument; uint16_t delay; } SccbAsyncOp;
typedef struct {
    SccbAsyncIo io;
    SccbAsyncOp ops[256];
    unsigned count, position, cleanup, state, half_us, stretch_us, transaction_us;
    uint64_t due, deadline, stretch_deadline, last_now;
    uint8_t value;
    int result, waiting, cleaning, cancelled;
} SccbAsync;
enum { SCCB_ASYNC_ARGUMENT=-1, SCCB_ASYNC_BUSY=-2, SCCB_ASYNC_BUS=-3,
       SCCB_ASYNC_TIMEOUT=-4, SCCB_ASYNC_NACK=-5, SCCB_ASYNC_CANCELLED=-6 };
int SccbAsync_Init(SccbAsync *, const SccbAsyncIo *, unsigned hz,
                   unsigned stretch_us, unsigned transaction_us);
int SccbAsync_BeginRead(SccbAsync *, uint8_t address, uint16_t reg, uint64_t now);
void SccbAsync_Poll(SccbAsync *, uint64_t now);
void SccbAsync_Cancel(SccbAsync *, uint64_t now);
int SccbAsync_Owned(const SccbAsync *);
/* Returns 1 for a completed operation, 0 otherwise. A failed operation never
 * changes *value. Taking completion releases logical ownership of the bus. */
int SccbAsync_Take(SccbAsync *, uint8_t *value, int *result);
#ifdef __cplusplus
}
#endif
#endif
