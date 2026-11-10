# Isolated camera QA cycle

`tools/camera_qa_cycle.py` runs serial hardware tests against the development
unit with MAC `02:00:00:00:00:01`. The local web interface starts with the runner.
Before every case, its `/api/camera` endpoint performs GigE Vision discovery,
checks that the unit is idle/unowned, and identifies the camera. The runner then
acquires control, executes one subject-specific test, restores the original
runtime, compares the restored controls and tables, and releases control.
Restoration or release failure stops the sequence. The HTTP server listens only
on localhost; the camera itself does not need an HTTP server.

New snapshots also preserve stream destination IP/port and `TLParamsLocked`.
Restoration first unlocks stopped structural controls, then restores that route
and the original lock value. Older archived snapshots omit these three fields;
their restoration comparisons retain that narrower historical scope. Do not
reinterpret an older restored flag as verification of omitted fields.
Firmware release clears the destination and lock; the latest runner also reads
all three fields after release and requires zero, alongside an unowned camera.

## Commands

Run from the application root with other camera controllers closed:

```sh
export GI_TYPELIB_PATH="$PWD/build/aravis/usr/lib/x86_64-linux-gnu/girepository-1.0"
export LD_LIBRARY_PATH="$PWD/build/aravis/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
python3 tools/camera_qa_cycle.py --plan --output diagnostics/qa-plan-new
python3 tools/camera_qa_cycle.py --ip auto --output diagnostics/qa-run-new
python3 tools/camera_qa_cycle.py --ip auto --select 'native.bars.*' \
  --output diagnostics/qa-formats-new
python3 tools/camera_qa_cycle.py --ip auto --select 'endurance.*' \
  --soak-seconds 3600 --output diagnostics/qa-endurance-new
```

The default host interface is `10.0.0.7`; use `--host` for another bench.
`--ip auto` requests a unicast discovery acknowledgment, avoiding the broadcast
reply filtering observed on this host. The web panel is at
`http://127.0.0.1:8787`; use `--web-port` for another local port. It displays
current case states and links to the retained evidence. Each output directory
must be new. The final standalone `report.html` works without a running server.

`--stress-cycles` defaults to 100. `--soak-seconds` defaults to 3600 and is limited
to 4..3600. The scene test is limited to 60 seconds; the continuous rolling-pattern
test uses the full requested duration. Shortened runs retain their exact duration
in JSON and must not be described as hour-long passes.

The optional `tools/run_qa_camera_jtag.tcl` performs a disruptive volatile v13/R5
load using the sealed release-staging 0.3.16 ELF by default. Its environment
overrides match the camera artifact overrides in `run_ai_linux_jtag.tcl`.
It leaves A53 at FSBL exit and does not start Linux or qualify AI functionality.
The original Linux/AI loader remains available for a separate Linux campaign.
Verify artifact hashes and connected hardware before loading. Neither the QA
runner nor this loader writes Flash, SD, startup selection or saved UserSets.

## Coverage and pass criteria

- Each XML pixel-format entry, including aliases: actual decoded color-bar pixels,
  image metadata and complete stream delivery.
- Mono8, YUV422, RGB8 and packed RAW10 at six geometries from 160x120 to 2592x1944.
  Width854 is a negative availability test for packed RAW10, which requires a
  multiple of four pixels. RAW bar oracles account for the configured sensor
  window width plus32 and horizontal guard offset16; they do not assume ISP
  scaling of the RAW pattern to output width.
- Existing independent pixel-response tests: black/restoration, JPEG quality,
  orientation, exposure/gain, white balance, RAW response, effects and measured FPS.
- FPGA feature enable/disable sequences and ROI/scaler 1x/2x/4x regressions.
  These establish transport/decoding behavior, not optical efficacy of each ISP.
- Sensor frame rates 1/2/5/10/15 FPS; repeated format/geometry/acquisition changes.
- Invalid dimensions/formats/packet settings, protected writes during streaming,
  live Black Level, repeated RGB444 captures and independent full-YUV Stop cases.
- Pattern-specific Black/Random/Square checks and moving-pattern freeze detection.
- Continuous FPGA RGB rolling-pattern reception with timestamp and BlockID
  continuity, per-frame byte statistics, content hashes and freeze detection.
- A separate real-scene automatic exposure/gain/gamma check with observable spatial
  content. It requires an open lens and suitable illumination.

Every transport sample validates packet sizes, image leader/trailer metadata,
payload completeness, duplicates and malformed packets. The receiver tolerates
UDP reordering until the next leader proves a missing frame. CSV records every
complete frame. A completed-frame BlockID gap, non-monotonic timestamp, more than
10% delivered-FPS deficit, excessive frame interval, new CSI/recovery event,
camera drop, or blank byte mean fails the scenario. The final image is decoded;
bar tests additionally apply an independent color/packing/Bayer-phase oracle.
ROI patterns can legitimately cover one uniform color. Real scenes cannot prove
absolute optical correctness without a controlled scene/reference.

Temperature and hardware counters are sampled during continuous captures.
`*-progress.json` updates while a long capture runs; `*-telemetry.json` retains
the samples afterwards. The explicit QA temperature interval is -20..95 C,
not a claim about the board's rated environmental specification.

Missing optical observability may cause existing relational tests to skip.
Failures remain failures; a successful repeat never erases earlier evidence.
No certification, electrical power-cycle, Flash persistence/endurance, calibrated
colorimetry or AI accuracy result is implied. A camera-only boot has no Linux
worker, so it cannot establish AI availability or inference performance.

The QA cycle compiles a private C `recvmmsg` executable per run and uses it for
both continuous streams and short image/JPEG captures. This avoids stalling UDP
while Python handles control, decoding or image reduction. The source and binary
hashes are retained. The standalone portable CLI keeps its Python default;
`--allow-reorder`, `--buffer-packets` and `--c-receiver` explicitly select the
corresponding receiver policies. Loss, duplicate packets, metadata errors and
queue overflow never become passes.

`--image-packet-delay` explicitly changes short functional captures from their
default20us profile. The separate `transport.fpga_rgb.paced100` case tests60s of
rolling FPGA RGB at100us, with unchanged nominal-FPS and frame-integrity checks.
A bounded paced PASS does not replace a failed20us hour or establish a new hour.

The observed host `net.core.rmem_max=212992` clamps the requested32MiB receiver
socket to a reported425984 bytes. Camera counters can stay zero while that
socket drops packets. The reversible administrative mitigation for this bench
is `sudo sysctl -w net.core.rmem_max=33554432`, followed by a new qualification
run; restore the original limit with `sudo sysctl -w net.core.rmem_max=212992`.
These commands are documented, not executed: passwordless administrator access
is unavailable. No persistent sysctl file is changed by the QA runner.

New C captures retain a bounded packet-metadata window when a frame fails:
the previous4096 packets and up to4096 following packets, capped at65536 output
records per capture. `*-packet-failure-window.csv` and `*-packet-window-*.csv`
show BlockID, packet kind/ID, length and host time. A header-only file means no
failure trigger was reached. `foreign_frame_packets` counts payload/trailer
packets received outside the current frame window. These observations help
distinguish loss from cross-frame reordering; they do not repair or hide either.
Writing a diagnostic window after the first failure can perturb subsequent
reception, so additional losses in that same capture are not independent
root-cause proof. The original continuous-hour run uses its sealed earlier
receiver without this instrumentation.

## Evidence and interruption

Results include the test purpose, discovery identity, duration, restoration and
release outcome, raw images, decoded PNGs, frame CSVs, per-capture counters and
exceptions. A SHA-256 manifest seals completed output. Preserve the entire
directory when sharing the HTML report. Failed captures may retain raw data even
when a pixel assertion was not reached.

SIGINT/SIGTERM request cleanup and stop the current runner after restoration.
Loss of power or a forcibly killed process can bypass cleanup; inspect ownership
and restore the retained `runtime-before.json` through supported controls before
starting another campaign. The runner never silently resets/reloads hardware to
hide a failed test.
