# Camera portfolio release

Current-status note, November10,2026: the dated plans below retain their original
scope. The later A53 analytics and same-IP Ethernet web implementation is now
bounded-camera tested. Publication of the current update is authorized. See
[Project status](PROJECT_STATUS.md) and [MicroSD boot](MICROSD_BOOT.md) for the
current implemented/open items; do not interpret an older research-only or
publishing-pending statement as the current feature state.


The November 1, 2026 scope is a reproducible engineering portfolio, not formal
GenICam/GigE Vision certification. SDK interoperability and project tests are
reported with their exact versions and limits. Nothing is published by this task.

## Selected additions

| Feature | Current evidence and implementation boundary | Remaining release gate |
| --- | --- | --- |
| Upscale | Existing six-tap/64-phase AMD VPSS is instantiated; firmware originally rejected enlargement. A bounded 4x-per-axis planner/control extension passes 11 RTL scenarios and native tests; firmware builds. | Paired PPC4 v13 repairs the reproduced RGB4x profile:320x240 to1280x960,57 frames at14.293804 FPS with20 us packet spacing, no loss/CSI/recovery and one exact Q12 saved-frame comparison. Seven format regressions match v12. Wider4x geometries and endurance remain unqualified. No invented optical detail. |
| Adaptive tone | Native AMD GTM synthesis estimates 62 DSP, exceeding seven remaining DSP. The previous-frame RGB8 adaptive LUT passes independent numerical and RTL tests, including gain bounds, temporal slew, first-frame bypass and history reset. HLS estimates 1 DSP, 3 BRAM18, 4698 LUT and 4292 FF with pixel-loop II=1. | Combined v11 synthesis/routing passes at 200 MHz; control/preset regressions pass. The subsequent eight-case board series passes with 20 us packet spacing, exact first/final synthetic tone pixels and approximately 14.293 FPS. Zero-delay host reception lost packets; real-scene quality and wider modes remain open. RGB display mapping is not HDR. |
| Distortion | Accepted640x480 nearest/+/-7-row profile; full-resolution bypass. Ten RTL calls and delayed-read drain checks pass. Actual isolated Vivado1538 LUT/1850 FF/16.5 BRAM36/0 DSP. Firmware0.3.14 links volatile map upload/CRC/cache/lease/manual-frame controls with native regression. | Corrected v12 routing passes at200 MHz (43841 LUT/124 BRAM36/235 DSP, setup/hold+0.477/+0.010 ns). Identity and +/-5 horizontal,+/-7 vertical maps pass exact camera pixels at about14.294 FPS; restart/disable/invalidation and existing-feature regressions pass. Calibrated lens parameters and endurance remain absent. Identity/shift maps demonstrate arithmetic, not optical correction. See [controls](REMAP_CONTROL.md). |
| HDR | Current camera streams one exposure at a time. Automatic exposure/gain metadata does not prove synchronized exposure brackets. Existing RGB8 output has already discarded precision. A tested portable RAW10 bracket validator/radiance merge is available offline, not linked into firmware. | Verified exposure timing/settling and gain/black metadata, a bracket-buffer lifetime contract, radiance merge and saturation/ghosting tests, then measured scene dynamic range. Tone mapping alone cannot close this gate. |

The HDR session/lease contract and bounded merge worker also pass offline tests.
Production inspection still cannot associate a commanded exposure with a specific
completed sensor frame. A built first-fault recorder supports the next4x
investigation but remains undeployed. Exact pending scopes are documented in
[capture gates](OFFLINE_CAPTURE_GATES.md). No new camera session is implied.

## Budget and preserved behavior

The v11 full routed candidate reports 41706/47232 LUT, 107.5/150 BRAM36,
235/240 DSP, setup/hold/pulse slack +0.382/+0.010/+0.145 ns at 200 MHz.
All 84077 routable nets are routed, with zero route errors and no DRC error or
critical warning. Source: `build/fpga-color-v11/report-check.json`; it is not
camera acceptance. This leaves 5526 LUT, 42.5 BRAM36 and only five DSP.

The preceding full v10 routed design reports 39054 LUT, 106 BRAM36 equivalents and
233/240 DSP at 200 MHz, setup/hold slack +0.691/+0.011 ns. Source:
`diagnostics/stream-led-fpga-20261027/`. These are historical implementation
results, not measurements of the new candidates. A standalone fit estimate is
not a combined placement result. Do not silently reduce advertised frame rates
or remove working CLAHE, scaler, RAW, LEDs or live Black Level to claim a fit.

Keep processing in the camera. No receiver-PC processing or new Linux/core boot
is introduced to conceal a camera resource limit. Software-only host or synthetic
checks do not establish optical calibration, measured camera latency or HDR.

## Excluded release work

External hardware trigger, dataset acquisition and custom training are not
release requirements. Existing code, collection tools and history are retained.
No model is advertised as deployed. Endurance remains postponed and will not
restart automatically. Factory Default policy and Flash/boot changes remain
outside the current implementation authorization.

## Delivery and validation

For each completed increment retain source/artifact hashes, numerical and RTL
oracles, build logs, exact mode limits and explicit unrun hardware gates. Current
workspace inspection reports no usable Git metadata; do not assume a clean index
or create/push a repository without a separate release action. Prepare English
README/build/operator documentation and preserve vendor license notices before
publication. The current task authorizes local implementation and verification,
not public sharing or a new board deployment.
