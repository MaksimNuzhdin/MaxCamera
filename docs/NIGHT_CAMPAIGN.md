# Unattended reliability campaign

The user authorized exclusive overnight access on November10,2026. The supervisor
runs a bounded eight-hour sequence against the identified physical camera and
expected RAM firmware. It does not write Flash, save UserSets, power-cycle the
board or automatically replace firmware after a fault.

Start with idle/unowned camera, AI disabled, the expected firmware and an online
Linux worker. Use installed Aravis0.8 GI libraries. From the application root:

```sh
python3 tools/gige_night_campaign.py \
  --output diagnostics/overnight-new --hours 8 \
  --expected-firmware 0.3.19-tcpheadroom-dev
```

Always use a fresh output directory; existing campaigns cannot be overwritten.
The original preliminary attempt remains retained with its failures and verified
stop. The current run is overnight-v2 in the local November10 evidence directory.

The output contains `campaign.json`, an automatically refreshed `index.html`,
frozen source files and hashes, a log and evidence directory per stage, and a
final SHA256 manifest. Live output is provisional until its stage and cleanup
finish. The supervisor records its PID and every child PID.

The sequence covers controlled leader/payload/trailer resend with HTTP,
100 format/start/stop transitions, paced full-array YUV, all-feature AI smoke,
paired one-hour baseline/HTTP windows for face detection at100us and20us,
all-feature AI at100us, and a budget-dependent repeat. A step is skipped rather
than shortened if its full allowance would exceed the remaining time.
No pending/skipped/shortened step counts as an hour PASS.

Children restore and compare their volatile profiles. Between stages the
supervisor independently checks MAC, firmware, acquisition/ownership and AI-off
state. It stops if restoration cannot be verified or if the85 C software guard
fires. That guard is a development stop threshold, not an environmental rating.
The native receiver is pinned to a sampled low-load logical CPU, recorded in
`campaign.json`. This is not an exclusive CPU reservation or a change to kernel
UDP buffer limits. The source freeze prevents later edits from silently changing an active campaign.

Create an empty `STOP` file inside the output directory to request a graceful
stop. SIGTERM to the supervisor also requests cleanup. A timed-out child receives
SIGTERM and45 seconds to restore; if it cannot exit it is terminated and the
campaign stops with restoration unverified. Do not start another camera owner
while the campaign is running. The web console can contend with intentional
HTTP saturation; the generated local report is the preferred status view.

The November9 failed hour remains in its original scope. New successful windows
would apply only to their firmware, receiver, geometry, pacing, AI subset and
HTTP load. They would not establish calibrated AI accuracy, maximum-mode or
24-hour endurance, physical MicroSD boot, power-loss safety or certification.
