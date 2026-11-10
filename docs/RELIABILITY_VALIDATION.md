# Video, AI and Ethernet web reliability trials

The camera remains a development candidate. Test windows retain the exact
firmware, pacing, stimulus and receiver scope; a short pass never replaces a
failed hour or qualifies every supported mode. The November9 hour failure
remains open.

## Receiver acceptance

`tools/qa_cycle_receiver.c` is an independent, no-resend receiver. It reports
payload loss, malformed/duplicate packets, full-frame leader BlockID gaps,
sequence errors and Linux per-socket drop counters. Classic BlockID wraps from
65535 to1. A missing whole frame fails even when all received frames are intact.
Up to four pending frames are reassembled in leader order, allowing late
payloads from the preceding frame. Window exhaustion retains an explicit
missing-frame failure. The final unterminated frame at a deliberate capture
boundary remains distinct from a missing trailer in an earlier frame.
Exact byte sums and raster-order FNV hashes are accumulated as contiguous
payload packets become available, avoiding a full-frame CPU pause after each
trailer. Tests compare these values with an independent byte-wise oracle.
Socket-drop availability is explicit; it is never inferred from a global counter.
The fault-injection tests include whole-frame omission, clean wraparound,
missing trailers, window exhaustion and exact replay of the observed November10
interleaving with known deterministic pixels. Original failed captures remain
unchanged. The fixture contains packet order only, not image content.

## Reproduce bounded comparisons

Use the installed Aravis0.8 GI library. With an idle, unowned camera and exclusive
access, run from the application directory (SW/ in the portable repository):

```sh
python3 tools/gige_web_reliability.py --ip 10.0.0.10 \
  --output diagnostics/reliability-new --seconds 120 \
  --delays 20 100 --ai-masks 0 1 --http-clients 4
python3 tools/gige_http_concurrency.py --ip 10.0.0.10 \
  --output diagnostics/concurrency-new --clients 4 --seconds 30
python3 tools/gige_resend_reliability.py --ip 10.0.0.10 \
  --output diagnostics/resend-new --frames 100
```

The comparison snapshots/restores volatile camera controls and tables, enables
rolling sensor bars, and runs baseline/HTTP windows for each selected packet
delay and AI mask. Four clients use simultaneous read-only status requests;
one client cycles status and static assets. AI result snapshots explicitly use
ResultLatch/ResultRelease. Output includes packet failure windows, frame traces,
telemetry, HTTP status/latency records, host diagnostics and restoration evidence.

The resend trial deliberately discards original leader, payload and trailer
packets in its own application. It compares every recovered byte with the
received original, checks camera resend counters and retains simultaneous HTTP
outcomes. This proves only its controlled software-loss scope; it does not
establish recovery from arbitrary kernel/network loss or sustained overload.

A failed, timed-out or interrupted run remains evidence. Diagnose harness
mistakes separately, repair them and run another attempt. Do not edit an older
outcome into a pass. No tool here writes Flash, saves UserSets or performs a
physical power cycle. Simultaneous unrelated camera clients invalidate an
isolated comparison and must be recorded.

## Full-array pacing study

`tools/gige_full_yuv_pacing.py` compares stated delays at2592x1944 YUV with AI off
and no HTTP generator. It records strict socket/frame integrity and delivered
versus nominal FPS and restores the original profile. On the November10 CPU8
study,5us passed53 frames in10 seconds;1/10/15us lost packets and20us halved
nominal throughput. That single short5us pass is not a universal default or
maximum-mode/endurance qualification.
