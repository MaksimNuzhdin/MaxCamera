# Paired PPC4 writer candidate integration

The qualified baseline remains v12 bit720ed91c/firmwareca156be0 in RAM,
stopped/released with original runtime restored. The prior RGB4x CSI SLBF fault
is retained. No offline simulation here establishes a repaired board or FPS.

## Measured feasibility

At200MHz, isolated Xilinx writer PPC2 uses2692LUT/3188FF/6BRAM36/1DSP;
PPC4 uses3712LUT/4693FF/8BRAM36/1DSP. A two-input interconnect comparison
(writer128/256-bit, reader32-bit, PS128-bit/49-bit address) adds1623LUT/1988FF,
including wider crossbar/reader upsizer and PS-side downsizer. Its low2GiB map
omits the unchanged production MMU and writer PCIe/QSPI windows, so this is a
width-change estimate rather than full-system fit. Production maps are checked
separately for exact preservation.

Fast packer adds9LUT; zero-padding adapter costs34LUT. Qualified routed v12
removed downsizer costs17LUT/57FF; old subset costs0. Net additive estimate is
+2669LUT,+3436FF,+2BRAM36,+0DSP, leaving722LUT/43817FF/24BRAM/5DSP.
Placement/constant propagation may change these totals. No full routed fit or
hold closure follows from isolated post-synthesis reports.

Fourteen packer throughput cases and the existing RGB/BGR/partial/reset/stall
oracle pass. Exact generated Xilinx PPC4 RTL plus adapter passes10 direct-byte
memory cases (row lengths1/2/3/5/6/7/17/66/2592/7776) and3 paired RGB packer cases
(66/3840/7776). Independent memory checks cover useful bytes, zero partial groups,
32-byte stride, AW/W stalls, burst framing and frame guards. Actual interconnect,
PS DDR, CSI/VPSS scheduling and GVSP are outside this memory model.

Evidence: `diagnostics/writer-ppc-isolated-20261102` (sealed), with source,
generated vendor RTL, reports, successful tests and retained preparation errors.

## One coherent v13 candidate

Signature0x4953500D and capability524287 distinguish the PPC4 writer from v12.
Only the paired fast-ready packer, new zero-padding writer adapter, writer PPC,
necessary derived interconnect widths and signature change. Existing ISP blocks,
clock/reset/control, DDR/PCIe/QSPI maps and reader restrictions remain intact.
The4-to2-byte AXIS downsizer and2-to6-byte subset are replaced by4-to12-byte
expansion: each source byte becomes the low component of one24-bit Y8 sample;
absent lanes become zero. SOF/EOL, effective destination bit0 and backpressure
are preserved. Writer AXIS96-bit and AXI256-bit feed the unchanged128-bit PS port.
This does not increase theoretical PS bandwidth.

Firmware must use localConfig.PixPerClk4, HActive rounded to4 bytes, and stride/
address aligned32 bytes only for the matching new capability. GVSP retains true
row bytes. The installed L2 driver enforces HActive%PPC==0 and alignment2*PPC*4.
Generated writer RTL ignores TKEEP for shortening a row; padding is mandatory.
A66-byte RGB row needs HActive68/stride96; old stride80 is unsafe. Existing raw
buffers are64-byte aligned and maximumRGB row7776 fits their allocation.

Sealed firmware candidate0.3.15-ppc4-dev:
`diagnostics/rgb-writer-feasibility-20261102/firmware-0.3.15-ppc4-dev.elf`, SHA256
`b2e516d6332f7fe047bfc9c65d308a5353628b38a0570214f690b7a8c39758ac`.
The earlier42c18279 pre-description ELF was never loaded and must not be used.

`tools/check_writer_v13_bd.py` preserves all old ISP cells/configuration, external
ports, scalar nets, clock/reset/control and addressing. It checks exact old/new
stream edges and emitted writer/converter XCI widths, pins the reviewed generated
interconnect, and rejects six deliberate graph mutations. Production packer and
adapter RTL pass their functional oracles. `tools/build_fpga_color_v13.tcl`
performs one full synthesis/place/route with strict resource/timing/route/DRC/skew
gates before bitstream/XSA export. It does not access the board. A failed full
build is preserved; no ISP deletion or FPS reduction is permitted to force fit.

## Bounded hardware sequence after all offline gates

Refresh exclusive idle state and complete original runtime/identity snapshot on
qualified v12, preserving its exact paired rollback inputs. Retain receive-only
UART through the paired RAM load. Verify PS initialization compatibility, v13
signature/capability, new firmware identity and healthy readiness before capture.
No Flash, boot image or UserSet save is involved.

Use the original discriminator profile: fullRAW2592x1944, sensor row35.55us,
manual exposure100/gain16; output1280x960 RGB with center640x480 ROI for2x and
320x240 ROI for4x. Preserve20us GevSCPD for these1280x960 tests;40us pacing can
itself limit14FPS RGB transport. Keep strict receiver/socket-drop evidence
separate from CSI/drop/recovery. Do not repeat the known oldRGB4x fault simply
to obtain another baseline. Preserve prior first-fault evidence and compare
against qualified2x and YUYV4x data/profile under identical requested cadence.

After a clean principal RGB4x test, regress2x, YUYV4x, remap, tone/live-black,
logical stream LED and native/RAW transitions as appropriate. The camera API minimum is160x120 and RAW10 width increment16. Include a
162-pixel FPGA-color RGB row (486 payload bytes, writer488/stride512), native
Mono/Bayer width162, RAW10p width160 (200 payload/stride224) and RAW10LE16
width160. The22-pixel/66-byte case is an isolated driver/RTL boundary test only;
the public camera API rejects it. RAW10p group crossings are tested without
lowering its advertised width increment. Native RGB with
FPGA disabled uses two sensor bytes per pixel and CPU conversion; it is a distinct
writer layout from the486-byte FPGA-color case and must be recorded accurately.
Use deterministic variation where an exact oracle is claimed. No threshold
relaxation or retry-until-green on an actual hardware fault.

End any fault by preserving the first error/counters/UART and using the qualified
quiescence/reset/rollback contract; never reset an active AXI master alone. At
completion restore exact original fields/tables/selectors/default/mask, stop and
release control, close owned UART/JTAG server, and record the actual running pair.

## Bounded v13 acceptance result (2026-11-02)

The single coherent candidate passes routed gates:45832LUT/49942FF/126BRAM36/
235DSP, leaving1400LUT/44522FF/24BRAM36/5DSP. WNS+0.429ns,
WHS+0.011ns, WPWS+0.145ns; zero failing endpoints,92996/92996 routed nets,
zero routing errors and no DRC errors/critical warnings. Six PS init files are
byte-identical to qualified v12. No ISP feature or advertised FPS was removed.

The original discriminator profile now passes: fullRAW2592x1944, centered
320x240 ROI ->1280x960 RGB, sensor80MHz/HTS2844/VTS1968 (35.55us row), exposure100,
gain16 and20us packet delay. First candidate acquisition delivers57 good frames
at14.293804FPS, zero bad/missing/duplicate/malformed packets, socket drops, new
CSI errors, dropped frames or recovery.23 reordered packets were reassembled
without loss. One saved final RGB frame matches independent separable six-tap
Q12 convolution exactly (maximum error0). Source ColorBars is stable after the
first frame. Pixel-oracle scope is that saved frame; integrity/counts/FPS cover
the bounded stream. The prior failed v11 RGB4x evidence remains preserved.

| Regression | Good frames | Delivered FPS | Saved-frame oracle |
| --- | ---: | ---: | --- |
| RGB2x |57|14.291490|Exact v12 and independent Q12 |
| YUYV4x |57|14.291689|Exact v12 |
| RGB162x120,486-byte row |58|14.294190|Exact v12 |
| Mono162x120 |120|29.992786|Exact v12 |
| Bayer8,162x120 |36|8.933785|Exact v12 |
| RAW10p,160x120 |58|14.294237|Exact v12 |
| RAW10LE16,160x120 |58|14.293371|Exact v12 |
| Remap bypass,640x480 |57|14.293769|Exact v12 |
| Remap identity |57|14.292958|Exact bypass |
| Remap+5/+7 |57|14.293451|Independent shift/black borders |
| Remap-5/-7 |57|14.292913|Independent shift/black borders |

All rows pass strict packet/error/recovery checks; original0.5% relative FPS
threshold remains unchanged. Short-run differences are not endurance or broad
maximum-FPS qualification. Packed RAW10 independently decodes to the matching
RAW10LE16 capture for all19200 pixels on both v12 and v13.

Bad remap CRC/enable refusal, active mutation rejection, read-only protection,
geometry invalidation and15 named GenICam node accesses pass. Tone high/low/
restart match exact integer oracles and first-frame bypass; live black level
and logical stream LED/locks pass. FPS remains14.292291..14.293948 across these
regressions. LED evidence is logical state, not optical observation.

Three preparation failures are preserved separately: the first baseline helper
left native timing after changing routing and was stopped by a timing guard
before capture; width22 was rejected by the existing160x120 API minimum before
capture; and an old host uploader refused v13 before map data writes. Corrected
helpers restore the proven timing sequence, use supported162/160 widths and
explicitly accept only matching v12/v13 capability pairs. No hardware-fault
retry, API-limit relaxation or threshold reduction was used.

Final original runtime, tables, selectors/default/mask and camera identity are
verified exactly, except the intended firmware version change. Owner0, Ready1,
Busy0, recovery/CSI/drop0, remap disabled/empty/status0. UART logger closed and
restored termios; owned hw_server closed. No Flash, boot image or UserSet save.
The bounded-qualified v13 pair remains in RAM:

- Bitstream SHA256 `d897a5eb2282bbc944c532b5996d5ca1076d1c002cb7d4743158b05ad19248ea`.
- XSA SHA256 `4c93e54d70d55478270bc8fac51d27af4ddfc06063d39f681255c45753b3d34e`.
- Firmware SHA256 `b2e516d6332f7fe047bfc9c65d308a5353628b38a0570214f690b7a8c39758ac`.

Full FPGA evidence: `diagnostics/fpga-ppc4-v13-20261102`.
Board evidence: `diagnostics/ppc4-v13-board-20261102`.
Qualified v12 paired rollback remains preserved separately.

This result qualifies the recorded320x240 ->1280x960 profile, not every4x
geometry accepted by the planner. Even ideal one-pixel/clock service needs
4*output_width/200MHz per source row:25.6us at1280 columns,51.84us at2592,
which exceeds the recorded35.55us sensor row for wide output. Buffering and
actual scheduling matter despite average frame bandwidth. No broad geometry
restriction or advertised-rate reduction was introduced, and no additional wide
failure experiment was run. Optical calibration, HDR exposure association and
trained classification accuracy are outside this acceptance result.
