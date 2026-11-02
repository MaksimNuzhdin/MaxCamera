#ifndef GIGE_SCCB_ACCESS_H
#define GIGE_SCCB_ACCESS_H
#include "GigeSensorTelemetry.h"
/* Single-main-loop arbitration. A failed acquisition requests cancellation;
 * subsequent polls drain it. No synchronous caller waits inside a command.
 * Nested calls are permitted only within the same synchronous call stack. */
class GigeSccbAccess {
    GigeSensorTelemetry &telemetry;
    unsigned depth=0;
    bool resume_pending=false;
public:
    explicit GigeSccbAccess(GigeSensorTelemetry &t):telemetry(t) {}
    bool acquire(uint64_t now) {
        if(depth){++depth;return true;}
        if(telemetry.bus) {
            GigeSensorTelemetry_Pause(&telemetry,now);
            resume_pending=true;
            if(telemetry.pending || SccbAsync_Owned(telemetry.bus))return false;
        }
        ++depth;return true;
    }
    void release() {if(depth && !--depth)resume_pending=true;}
    /* Stopped-camera controls must never race a newly started telemetry read.
     * Existing work still drains regardless of acquisition/start permission. */
    void poll(uint64_t now,bool allowed,bool acquisition_active) {
        if(depth || !telemetry.bus)return;
        GigeSensorTelemetry_Poll(&telemetry,now,allowed && acquisition_active && !resume_pending);
        if(resume_pending && !telemetry.pending && !SccbAsync_Owned(telemetry.bus)) {
            GigeSensorTelemetry_Resume(&telemetry,now);
            /* Preserve a command retry window; do not immediately reacquire
             * the bus after a cancelled telemetry read or synchronous access. */
            telemetry.next_due=telemetry.interval_us>UINT64_MAX-now?
                UINT64_MAX:now+telemetry.interval_us;
            resume_pending=false;
        }
    }
};
class GigeSccbGuard {
    GigeSccbAccess &access;
    bool acquired;
public:
    GigeSccbGuard(GigeSccbAccess &a,uint64_t now):access(a),acquired(a.acquire(now)) {}
    ~GigeSccbGuard(){if(acquired)access.release();}
    explicit operator bool() const {return acquired;}
    GigeSccbGuard(const GigeSccbGuard &)=delete;
    GigeSccbGuard &operator=(const GigeSccbGuard &)=delete;
};
#endif
