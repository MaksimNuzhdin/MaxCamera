# QA finding: full-array YUV Stop can trigger sensor recovery

The November9 QA campaign reproduced a2592x1944 YUV Stop failure on firmware
`0.3.16-ai-linux-dev` with FPGA v13. The receiver delivered complete frames before
Stop; then `CameraRecoveryCount` increased and the controller lost ownership.
UART reproduction records `SCCB wr reg=4202 e=-3 phase=1`, followed by
`GIGE CAMERA drained stop=-101` and automatic sensor reinitialization. CSI counters
remained zero. Evidence is in `diagnostics/qa-cycle-20261109/corrected/` and
`diagnostics/qa-cycle-20261109/yuv-stop-reproduction-uart.log`.

Large synchronous CPU image conversion can delay asynchronous SCCB polls beyond
their100ms deadline and strand a read. This is the working explanation; the
original binary did not retain a full poll-latency trace, so the mechanism is not
represented as fully instrumented root-cause proof. General I2C bus-clear behavior
is unchanged.

## First mitigation

Candidate `0.3.17-qa-sccb-dev` keeps the existing105MB/s telemetry deferral and
also defers background SCCB reads while CPU-converting more than307200 pixels
(the qualified640x480 software path). Direct DMA RAW10/RAW10p and FPGA color
retain their bandwidth policy. Hardware sensor AE/AGC continues. Cached dynamic
values can remain stale while `SensorTelemetryDeferred=true`; its GenICam tooltip
now describes both gates. Camera/protocol timeouts and advertised FPS are unchanged.

Unit cases cover low-FPS large software frames, VGA, direct DMA, the exact105MB/s
boundary and64-bit multiplication. The original ELF is preserved at
`diagnostics/qa-cycle-20261109/rollback/MaxCameraApp-0.3.16.elf`, SHA-256
`67a07d2778924011c846c9de81823abf7177d6b48d9d36715f8fe20ebdf3f866`.
The candidate builds separately in `build/qa-sccb-app`; the sealed ELF and v13
FPGA artifacts remain intact.

## Bounded board regression

The first ten-case regression on0.3.17/v13 passes. Five independent full-YUV Stop
cycles report `SensorTelemetryDeferred=1`, unchanged recovery/CSI counters,
retained control, exact profile restoration and an unowned camera afterwards.
Full RAW10p, native VGA, ROI4x and short rolling-pattern cases also pass. These
short windows do not establish an hour-long result; long-run status is separate.

The lifecycle cases have an ownership/SCCB subject. Some retain UDP socket drops
at zero packet delay; those losses remain visible and are not transport passes.
A separate full-YUV profile with explicit1us pacing passes complete reception,
pixel checks and nominal FPS. The host receive-buffer limit is212992 bytes
(the doubled socket allocation reports425984). Administrator access to increase
it is unavailable. The1us result qualifies only that bounded host/camera profile.

The QA runner may reacquire only an unowned camera after a lost cleanup lease to
restore the retained profile. It keeps the test failed and never silently resets
hardware. Further adversarial/endurance outcomes are under
`diagnostics/qa-cycle-20261109/`.

## Completed campaign scope

The later3600-second FPGA RGB capture reports no new recovery or CSI events and
restores control/profile successfully, but its transport result is FAIL because
the host socket drops449 packets. Subsequent large-frame and optical regressions
also retain control without new SCCB recovery. This supports the stated bounded
mitigation; it does not convert the transport failures into camera qualification.
The final campaign report keeps firmware, host-buffer, test-oracle and
observability findings separate.
