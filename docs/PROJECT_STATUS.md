# Project status and remaining work

Updated: November 10, 2026. This is the current index; older dated plans and
qualification records retain their original scope.

## Implemented and demonstrated

- R5 owns sensor control, capture buffers, FPGA controls, GenICam/GVCP/GVSP,
  resend and Gigabit Ethernet. Source and bounded format/ISP tests are retained.
- The v13 paired design repairs one RGB4x profile:320x240 ROI to1280x960,
  57 complete frames at14.293804 FPS with20 us spacing. Wider4x is unqualified.
- A53 Linux performs eight CPU analytics functions with a single-slot frame
  lease and coherent R5 result snapshots. Optical accuracy remains unqualified.
- AI Studio uses TCP80 on the same camera IP and Ethernet cable as GigE Vision.
  R5 serves static assets and forwards bounded API requests to Linux. Writes
  are rejected during acquisition; GVCP ownership has priority.
- On November10, face detection plus HTTP load delivered143 complete RGB640x480
  frames in each10-second comparison, about14.294 FPS, zero missing/incomplete
  packets/frames, device drops or CSI errors. The HTTP phase completed351
  requests; last inference was316021 us. This is not maximum-mode/endurance
  qualification. See [Ethernet guide](../SW/ai/web/ETHERNET.md).
- An isolated SD boot image can be built from the tested artifacts. FAT32 and
  embedded BOOT.BIN integrity are checked. Physical SD boot is not yet accepted.

## November10 evening reliability update

The0.3.18 web candidate queued concurrent status readers. Its10-second idle
checks passed819 requests with two clients and816 with four. A later two-minute
loaded20us window still had9 URL-level connection errors, while all1716 video
frames arrived. The100us window delivered1716 good frames and7761 HTTP200
responses. These short results do not close the hour gate.

The0.3.19-tcpheadroom-dev candidate retains four HTTP request contexts and
reserves16 lwIP TCP PCBs for active plus closing connections. Its initial
30-second four-client check served2409 requests, all200. The first video gate still lost3 host-socket packets at100us. A subsequent
CPU8/incremental-statistics gate passes both60-second face windows with858
frames each and3704 HTTP200 replies under four readers. The unattended
campaign remains live; no overnight or new hour PASS is claimed.

A newly retained interleaving failure had162 late previous-frame packets after
the next leader, with no socket drops. Both affected frames' complete packet-ID
sets exist in the trace. A bounded four-frame receiver and deterministic-pixel
replay repair this receiver limitation. This does not reinterpret the original
November9 hour with449 socket drops. Whole-frame gaps, missing trailers and
window exhaustion still fail. See [reliability procedure](RELIABILITY_VALIDATION.md)
and [overnight supervisor](NIGHT_CAMPAIGN.md).

## Next acceptance gates

| Priority | Work | Completion evidence needed |
| --- | --- | --- |
|1|Physical MicroSD boot and persistence|Cold boot with the documented boot switches, correct FPGA/R5/Linux/HTTP identity, DHCP and video, profile/reference persistence across reboot and clean shutdown. Recover a known-good card. No physical card has been written by this update.|
|1|Transport reliability and long runs|Resolve the Nov9 hour failure and intermittent switching loss. Repeat the intended maximum modes with HTTP idle/load, bounded receiver resources, resends and thermal traces. Keep camera versus socket/network loss distinct.|
|1|Production web access and storage behavior|Define trusted-network deployment or add authentication/TLS as required; test upload quotas, multiple clients, slow/disconnected clients, backend failure, power loss and storage full/read-only cases. The current HTTP token is not login authentication.|
|2|AI quality and throughput|Collect consented/annotated application fixtures, validate recognition threshold/false matches, object recall, plate/OCR/QR/motion behavior and sustained latency with the intended feature subset. No custom training or accuracy target is established.|
|2|Image calibration|Dark/defect maps, flat-field lens shading, color-target CCM/AWB/gamma/noise/detail tuning, calibrated intrinsics/distortion and held-out optical validation. Functional image effects are not calibration.|
|2|Expand image-path coverage|Wider4x geometry, full-resolution/remap bounds, all relevant pixel formats and recovery/reset interleavings; preserve hardware resource/timing margins.|
|3|True HDR|Verified frame-effective exposure/gain tagging, bracket ownership/settling, on-camera merge, saturation/ghosting and measured scene dynamic range. Offline merge code and tone mapping do not close this gate.|
|3|Productization and reproducibility|Fresh machine/platform rebuild, dependencies/licenses, artifact signing/versioning, rollback and safe network update. QSPI autonomous boot remains separate from SD preparation.|
|3|External synchronization and certification|Hardware trigger/PTP/multi-camera if selected for the product; formal GenICam/GigE Vision, electrical/environmental/safety tests are not completed.|

## Existing QA outcomes that remain open

The November9 campaign exercised149 current subjects over236 attempts:
129 PASS,9 FAIL,11 SKIP across identified firmware/receiver scopes. The actual
3600-second run delivered51454 complete frames but had2 incomplete frames,
448 missing packets and449 socket drops. No hour-long PASS exists. A separate
60-second100 us paced RGB profile passed857 frames without observed transport
or camera faults; it does not replace the failed hour or qualify every format.

Remaining failures include full-array zero-delay host profiles, an intermittent
switching loss with zero socket/camera fault counters, a host-overflow transport
lock capture and low-gain real-scene recipes. Linux-AI/optical skips in that
camera-only campaign remain its original coverage gaps; the later bounded AI
trial has a separate scope. [QA workflow](CAMERA_QA_CYCLE.md),
[telemetry mitigation](QA_SCCB_TELEMETRY_GUARD.md).

## Deferred product choices

Factory Default compatibility policy, an official standards matrix,24-hour
endurance and other previously deferred work require a deliberate new scope.
Nothing here schedules or silently starts such campaigns. Advanced proposals
such as stabilization, auto-framing, pyramids and simultaneous resolutions are
not implied by the current CPU detector/tracker implementation.

## Additional bounded evening checks

Early resend requests recover500 intentionally discarded leader/payload/trailer
packets across100 frames with byte-exact comparison and401 HTTP200 requests.
An earlier trailer-triggered attempt returned PACKET_UNAVAILABLE after40 complete
frames; it remains retained. Full-array YUV has a separate10-second5us pass:
53 good frames at5.211289 FPS. Other tested delays lose packets or lower FPS.
These are narrower scopes than arbitrary receiver stalls or maximum-mode endurance.
