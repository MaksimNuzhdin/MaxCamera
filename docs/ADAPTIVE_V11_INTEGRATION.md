# Adaptive RGB tone integration, processing v11

## Status and paired artifacts

The complete v11 FPGA implementation and matching0.3.13-adaptive-dev R5 firmware
build pass offline. A subsequent explicitly authorized board stage loaded this
sealed pair into volatile PS/PL/R5 and passed the bounded paced profile below.
It is running in RAM with original runtime restored, tone off/128 and owner0.
Flash, boot images and saved UserSets were not changed. The earlier offline
bundle remains an immutable record of its pre-deployment scope.

| Artifact | SHA256 |
|---|---|
| v11 bitstream | `321bcd1c7538cec101361080887e3b296929904a8ff7728f364df5410852b760` |
| v11 XSA | `717569c5b059419f6c4aa140a02c306724a46fba9dcc0a5dce59ff1fcec374e4` |
|0.3.13 R5 ELF | `faacce1c85b346e68f76929af583d6d1547784ecea0e7692d7ed594717c231fb` |

The matching image reports signature0x4953500B, processing version11 and
capabilities131071 (old65535 plus adaptive-tone bit16). Firmware recognizes
v11 as using the same independent LED/reset mapping as v10: four board LEDs on
GPIO channel1 and active-low video reset on channel2 bit0 at0xA0010008.
The authenticated v11 capability is also eligible for the already audited live
Black Level path. Legacy v8/v9/v10 never access the absent adaptive register bank.
Old firmware which does not recognize v11 cannot be used with this bitstream;
rollback must restore a compatible sealed FPGA/firmware pair.

## Datapath, controls and persistence

The inserted stream order is Polyphase -> CLAHE -> adaptive tone -> scene metrics
-> output packing. Tone is at0xA0180000. All original peripheral addresses, ISP
parameters, input/output ports and LED/reset wiring are preserved. Because the
original AXI control fabric already had16 occupied masters, only the CLAHE M15
branch is cascaded through a new1-to-2 control interconnect. CLAHE retains its
old0xA0130000 address. Six exported PS-init entries are byte-identical to v10.
The firmware ABI is checked against20 constants from the actual exported HLS IP.

Tone defaults off; target defaults128 DN. Five append-only GenICam nodes expose
availability, enable, target16..224, applied gain0.5..2.0x and preceding mean.
Enable/target remain stopped-only. No additional live-control exception is added.
Gain/mean are unlatched current-invocation telemetry, not an atomic pair, not part
of the common statistics latch and not associated with a specific GVSP frame ID.
Before the first valid invocation gain is reported as unity. Final scene intensity
statistics remain downstream of tone; color/AWB admission statistics remain at
their original stage.

The previous input-frame weighted-luma mean drives a shared per-channel rational
RGB8 display curve. Gain changes by at most16/256 per frame, with range128..512.
Black/white endpoints are fixed. The curve may change RGB ratios and temporal
brightness; it is not HDR radiance reconstruction or calibrated color correction.
Start reinitializes video state and each programming call increments an epoch;
first/start/reset/dimension/target-change frames bypass. Disabled mode passes
pixels unchanged. See the isolated adaptive-tone documentation/evidence for the
numerical model and full-frame accumulator checks.

Preset schema ISPA now stores30 FPGA scalars in1772 bytes. The schema identifier
is separate from processing version11. Genuine ISP9/28-field1764-byte records,
including all matrix/offset/gamma/LSC data, decode with tone disabled/target128.
ISP1 through ISP8 preserve their prior defaults and native-YUV bypass semantics.
Unsupported tone-enabled profiles are rejected during import prevalidation on
old hardware. Old readback/snapshot helpers skip unavailable new fields; the
working preview recipe explicitly resets adaptive state only on capable images.
No saved camera preset was overwritten by this integration.

## Routed resources and timing

| Resource | Used | Available | Remaining |
|---|---:|---:|---:|
| LUT |41706 |47232 |5526 |
| FF |44420 |94464 |50044 |
| BRAM36 tiles |107.5 |150 |42.5 |
| DSP |235 |240 |5 |

At200MHz, routed WNS is+0.382ns, WHS+0.010ns and pulse-width slack+0.145ns.
All84077 routable nets are fully routed, with zero routing errors. DRC has no
Errors/Critical Warnings; DSP-pipeline/power/LUT advisories and warnings are
retained in the report. Both bus-skew checks pass at+4.447/+4.114ns. The actual
Vivado synthesis of the tone IP uses2641 LUT,1.5BRAM36 and2DSP; this supersedes
the HLS1DSP/4698LUT estimate. Do not infer resource headroom from that estimate.

A separate report checker validates setup/hold/pulse width, unconstrained internal
endpoints, resource capacity, route completion, DRC and bus skew. An injected
negative setup slack was rejected. The checker invocation was added to the build
script after this completed run and also executed explicitly on these reports;
no netlist or functional source change was required for that added gate.

## Tests and limits

Native ASan/UBSan tests cover register programming, dimensions/target/enable,
epoch changes, RO/stopped/transport guards, v11 LED identity/live Black Level,
and zero absent-bank transactions on v8/v9/v10. A genuine sealed-v9 encoder
creates the legacy fixture; new decoding verifies every old table/value, ISP1-8
migration, ISPA roundtrip and atomic corrupt/truncated rejection. Existing UserSet
journal, GigE, upscale and live Black Level regressions pass. GenApi schema,
ROM/URL/header coherence and104 formulas/39 boundary cases pass.

The actual exported tone AXIS RTL passes five consecutive frames with an
independent temporal integer pixel oracle, source bubbles, sink backpressure and
all pixel/SOF/EOL/KEEP/STRB/stall checks. Gains256/272/288/272/272 cover temporal
ramping, white, black and zero-mean history. At a5ns test clock it takes13743
cycles under synthetic source/sink pacing. The isolated IP has separate Csim/RTL
cosimulation/full-frame accumulator evidence. These are not camera FPS results.

The natural-scene collection on v10 captured2x ROI enlargement, but a centered4x
ROI attempt timed out and triggered recovery. That unresolved acquisition gate is
inherited by v11 and must not be hidden by the successful isolated VPSS or tone
simulations. No full-chain real-time FIFO/CSI proof or camera tone/FPS acceptance
is claimed. No timeout was raised or threshold relaxed to accept the failed case.
All hardware access is paused pending explicit handback. Endurance is postponed.

Frozen bundle: `diagnostics/adaptive-v11-integration-20261101/`.

## Subsequent bounded board acceptance, 2026-11-01

Evidence: `diagnostics/v11-board-20261101/`. The normal v10 2x control had57
completed receiver frames (camera transmitted58 in a different counter window),
14.295498FPS and zero new CSI/drop/recovery. Two finite4x trials failed before
any GVSP packet/framebuffer completion: nominal14.293FPS raised the CSI
error-bearing-poll counter by12110/recovery3->4; requested1FPS (derived0.999)
with unchanged35550ns line period raised it by3533/recovery4->5. Those are poll
counts, not numbers of damaged frames. Lower VTS-based average FPS did not fix
the case; geometry/buffering/line pacing remain under investigation.

The authorized v11 load included run_board.tcl's initial system reset, sibling
FSBL onA53#0 and R5#0 reset/download. Subsequent v11 tests had zero recovery/CSI
increments. No additional explicit conditional recovery reset or rollback was
needed/exercised. The compatible v10 rollback pair was preserved.

Two unpaced1280x960 receiver attempts failed strict transport checks despite
zero camera CSI/drop/recovery and bit-exact surviving payloads. The second lost
1278 packets, exactly matching1278 host UDP RcvbufErrors. Removing expensive
large-frame FNV alone did not resolve loss and is not claimed as the sole cause.
The actual socket buffer was425984 bytes; no sysctl/security setting changed.
These failures are retained and are distinct from the upstream4x capture fault.

A single controlled GevSCPD20us profile then passed all eight cases and500
completed frames with zero bad/missing/duplicate/malformed packets, socket drops,
camera drops, CSI events and recoveries. Host RcvbufErrors delta was also zero.
No acceptance threshold or camera timeout was relaxed. The result is qualified
only for this paced profile, not for unpaced high-payload reception on this host.

| Case | Complete receiver frames | Delivered FPS |
|---|---:|---:|
| Existing2x |57 |14.290956 |
| Tone bypass |57 |14.293064 |
| High target224 |57 |14.292689 |
| Low target16 |57 |14.292449 |
| High restarted |57 |14.292457 |
| Disabled again |57 |14.293523 |
| Live Black Level |72 |14.293481 |
| LED/reset isolation |86 |14.292930 |

Input was a deterministic sensor pattern with fixed exposure100/gain16 and other
color functions disabled. High/low/restarted first frames exactly equal bypass;
steady last frames exactly equal an independent integer rational-curve oracle.
The preceding input mean was129; high gain445Q8 and low gain128Q8 matched hardware.
This proves the stated functional/history path, not optical image calibration.
Existing2x bypass pixels exactly equal v10. Live Black Level0/64/1023/0 worked
under transport lock without stopping; structural/target/range guards remained.
Sixty passive GPIO samples retained reset1 while LED3 toggled; logical readiness
also held. This is register evidence, not an optical observation of emitted LEDs.

All original runtime groups and identity/network/provisioned data were restored
(with the firmware-version string changed and new tone fields defaulted off/128).
Original packet delay0, native2592x1944 YUV422, selector/default0/mask255 were
restored. The camera was stopped, ready1/owner0; the owned hw_server was stopped.
No UserSet save, Flash/boot write or persistent network change occurred. The4x
case and unpaced high-payload host transport remain outside acceptance.
