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
