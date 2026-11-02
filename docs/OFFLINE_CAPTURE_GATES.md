# Bounded capture integration and diagnostic gates

The latest released camera is FW0.3.15-ppc4-dev and FPGA v13 after bounded
November 2 acceptance and exact runtime restoration. The v11 diagnostics below
are historical evidence; v13 repairs the tested320x240-to1280x960 RGB4x profile. Remaining
in-scope RAM/FPGA tests, temporary runtime changes, start/stop and conditional warm
reset/rollback are authorized. Flash, boot changes and persistent UserSet saves
remain excluded. HDR still requires an actually suitable stable-light scene.

## HDR acquisition: what the current driver actually establishes

`Ov5640_SetExposure` writes manual AE/AG selection, exposure lines and gain via
`Ov5640_GroupUpdate`. While stopped, GroupUpdate writes the registers directly.
While streaming, it checks group state, writes group0, ends the group and launches
it through 0x3212. Neither branch returns a sensor-frame identifier or proves
which exposure was integrated into a captured frame. Cached register readback is
not that proof. The current driver reserves group3 for embedded SOF but does not
implement a qualified exposure-metadata channel.

`GigeCamera_Poll` rotates six DMA buffers, excluding the completed buffer and
the current GVSP source. `arm()` forgets old resend ownership before reuse.
Timestamps are captured at DMA arm, not optical exposure or observed sensor SOF.
Generic CSI short packets are currently drained and discarded. Mapping those
packets to completed buffers would itself require overflow, loss and wrap tests;
their data must not simply be labeled an exposure sequence.

The tested `experiments/hdr` session and bounded merge worker are useful but not
production HDR. Wiring their verified flag to successful SCCB writes would make
untruthful frame metadata. Wiring their buffers into the current rotation without
lease-aware arm/reset/recovery logic would risk overwrite. No such hook was added.

The smallest acquisition experiment requires a stable illuminated static scene,
fixed RAW10 geometry, fixed gain and disabled automatic exposure. Save original
runtime settings; do not change persistent UserSets. Capture short and long manual
exposure plateaus with one commanded transition at a time, log SCCB completion,
DMA arm/completion and any available qualified sensor frame events, and retain
each raw frame. Repeat stopped-start and streaming group transitions separately.
Use nonsaturated, above-black scene patches to establish response and settling;
sensor test bars cannot prove exposure response. Results qualify only the tested
mode, timing and transition direction. If no reliable association is established,
keep `effective_exposure_verified=false` and do not publish an HDR result.

Only after that gate should production implement dedicated/leased buffers, cache
barriers, geometry/config generations, explicit cancellation drain and bounded
merge scheduling. Full-resolution pair+radiance/flags storage is 45,349,632 bytes;
linker address range is not free-memory evidence. Fixed scene HDR may improve
recoverable dynamic range; motion handling, calibration and real-time FPS remain
separate limitations. No dataset or training is required.

## 4x: instrument the first failure, not repeated polls

The production-source candidate adds `g_camera_fault_evidence` and a tested
RAM-only recorder. It retains acquisition geometry/generation, the first existing
CSI IRQ/error mask, existing writer control, frame error union, capture index,
completed count, DMA-arm time and software poll time. A separate timeout record
cannot overwrite the first event. Starting another acquisition preserves the
previous fault-bearing acquisition; subsequent clean acquisitions do not erase
it. This is bounded evidence, not an unlimited history.

No additional MMIO read, polling cadence, FIFO setting, timeout or advertised
frame rate changed. One clock read and a small RAM copy occur at the first error;
this may affect execution after that error and is not cycle-accurate tracing.
The timeout path prints retained first-error fields after declaring the fault,
avoiding UART traffic during the causal high-rate interval. The RAM structure is
not a GenICam register interface or an atomic cross-core snapshot. Do not assume
JTAG DDR reads see dirty CPU cache. Prefer the CPU-emitted serial evidence; verify
the serial route before attempting the test. Earlier absent UART bytes remain an
observability limitation until a known startup message is actually captured.

Executed November 2 diagnostic procedure (historical test scope):

1. Exclusive camera ownership, original runtime snapshot and preserved sealed v11
   rollback; confirm a usable serial log before acquisition. RAM-load only the
   diagnostic firmware with compatible v11. The existing loader performs system
   reset/FSBL startup; this temporarily interrupts streaming. No Flash, boot or
   UserSet writes.
2. One 2x RGB8 control: centered640x480 ROI to1280x960, same2592x1944 sensor mode
   and recorded35,550 ns row period. Use the qualified20 us packet-spacing profile
   to separate host UDP loss from camera errors.
3. One 4x RGB8 attempt: centered320x240 ROI to1280x960, same sensor cadence. Preserve
   first-error mask before recovery. XCSI_ISR_SLBF_MASK (0x00040000) identifies a
   stream-line-buffer-full observation; protocol/ECC/DPHY errors are different
   evidence. A mask does not reveal encrypted FIFO occupancy or the first stalled
   downstream block. CSI counter increments are error-bearing polls, not frames.
4. One discriminating4x YUYV8 case, if the preceding recovery is clean: retain the
   same RAW10 sensor/scaler geometry but use the existing two-byte output mode
   (`GIGE_PIXEL_YUYV8`) instead of three-byte RGB8. This lowers downstream byte
   demand without inventing a sensor HTS write. Record exact readbacks because
   format changes affect the output converter. Success would support downstream
   pressure; it would not qualify RGB4x or prove a particular FIFO fix.
5. Restore settings and verify the original2x control, then stop/release. A single
   bounded session of roughly15 minutes is sufficient for these discriminants.
   Conditional warm reset/compatible rollback must be included in approval; stop
   if serial evidence or clean recovery is unavailable. Do not rerun until green.

If both formats fail without a discriminating mask, the next gate is separately
designed AXIS/CSI instrumentation, not guessed encrypted internals. Lowering VTS-
derived average FPS already failed to resolve the original4x case and did not
change row cadence; it is not a supported fix. This proposal requests neither a
new FPGA image nor broad endurance testing.

## November 2 observed discriminator

The saved bundle `diagnostics/4x-firstfault-board-20261102/` contains original
settings, exact ELF/FPGA hashes, UART startup/first-error output, captures and final
restoration. RGB2x delivered57 complete frames at14.293395 FPS. RGB4x delivered
no packets and retained CSI error0x00040000 before one recovery; cleanup's protocol
ACCESS_DENIED followed lease revocation and was not an execution-policy denial.
YUYV4x with the same ROI/scaler geometry delivered57 frames at14.293775 FPS with
no new CSI/drop/recovery/socket errors. Final baseline RGB2x pixels match the
initial control. Camera was stopped/released and original UART settings restored.

This establishes format-dependent pressure, not the first stalled downstream
block or a repair. RGB4x remains unqualified. A wider frame writer alone does not
remove the preceding two-byte/clock packer ceiling. Any buffering or sensor-readout
change needs explicit geometry/cadence analysis and preserved-path tests; no
arbitrary timeout or advertised-FPS change is justified by this evidence.

## Prepared HDR experiment (not executed)

The [stable-light procedure](HDR_EXPOSURE_EXPERIMENT.md) defines the next bounded
scene-dependent trial and the offline RAW plateau checker. Session/merge/worker
sanitizers and six Python screening tests pass; these do not establish effective
sensor exposure. HDR remains unexposed and unlinked. No camera changes were made
for this preparation; scene readiness is the next external dependency.
