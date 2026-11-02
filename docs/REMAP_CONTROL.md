# Bounded distortion-map control

Status: firmware0.3.14-remap-dev and corrected v12 are RAM-loaded and have passed
bounded camera acceptance. Combined routing passes at200 MHz with43,841 LUT,
124 BRAM36,235 DSP and setup/hold/pulse slack+0.477/+0.010/+0.145 ns.
Identity, +/-5 horizontal and +/-7 vertical synthetic translation maps pass
exact pixels and borders at approximately14.294 FPS. Each map run has57 complete
frames with no packet loss, malformed frames or new CSI/recovery events. This is
not optical calibration or endurance qualification. The prior0.3.13/v11 pair
remains the verified rollback.

The accepted profile operates on RGB8 after VPSS and before CLAHE, with unchanged
output canvas, nearest-neighbor samples, at most 640x480 and source-row displacement
at most 7 in either direction. Invalid/out-of-image coordinates become black.
The bypass path supports native limits 2592x1944; RAW/native routes do not invoke
the color remapper. Existing scaler and other ISP modes are not restricted to 640x480.

The first v12 RAM board run passed bypass pixel equivalence, but enabled identity
acquisition failed and was not promoted. Saved rejected DMA pixels matched the
reference. The upstream TSTRB contract mismatch was reproduced offline and fixed
only at the VPSS output boundary, preserving KEEP/SOF/EOL checks. The corrected
paired-image retest passes. Original failed acquisition evidence is retained.
See contract review (archived development reference; see [verification scope](VERIFICATION.md)).
Firmware emits one `GIGE REMAP rejected` UART record before recovery clears live
status. A zero status read after recovery does not erase the original failure.

## Map preparation and provenance

Generate inverse source coordinates using `tools/distortion_maps.py` and genuine
intrinsics/distortion for the intended lens and image geometry. The map compiler
preserves calibration/geometry IDs and source hashes. An identity or synthetic
shift is useful for transport/pixel tests but is not a calibrated lens correction.

```sh
python3 experiments/remap_integer/compile_map.py FLOAT_MAP_DIRECTORY COMPILED_DIRECTORY
python3 tools/upload_distortion_map.py COMPILED_DIRECTORY
```

The second command validates locally and does not contact a camera. The binary
contains little-endian 32-bit words: bit 31 valid, bits 9:0 source x, bits 18:10 source
y, bits 30:19 zero. Zero is a black-border sample. CRC32 is over the exact binary
bytes, using the conventional reflected polynomial 0xEDB88320 and initial/final
XOR 0xFFFFFFFF. The report also carries SHA256. Unsupported maps are rejected;
coordinates are not silently clamped to the supported displacement.

## Volatile GenICam interface

All nodes are vendor-specific in `FpgaImageProcessing`, not SFNC standard nodes.
`FpgaDistortionAvailable` is false on v11 and older supported images. Firmware
must recognize signature 0x4953500C and capability 131072 before remap MMIO access.

| Nodes | Operation |
| --- | --- |
| `FpgaDistortionEnable` | Stopped-only enable; requires a validated matching map and FPGA color route |
| `FpgaDistortionMapWidth`, `FpgaDistortionMapHeight` | Staging dimensions; must equal current output |
| `FpgaDistortionExpectedCrc32` | Expected checksum before Begin |
| `FpgaDistortionMapBegin` | Start a new sequential upload in the inactive bank |
| `FpgaDistortionMapIndex`, `FpgaDistortionMapData` | Explicit zero-based word index and value; identical earlier-word retries accepted |
| `FpgaDistortionMapCommit` | Begin incremental validation after all words arrived |
| `FpgaDistortionMapAbort` | Cancel disabled staging state; never proves reader quiescence |
| `FpgaDistortionMapState` | Empty, Uploading, Validating, Ready or Error |
| `FpgaDistortionWordsUploaded`, `FpgaDistortionWordsValidated` | Exact transfer/validation progress |
| `FpgaDistortionMapError` | None, InvalidCoordinate or ChecksumMismatch |
| `FpgaDistortionHardwareStatus` | Last invocation status: geometry1, map2, AXIS framing4; retained until setup/new invocation |

Set dimensions/checksum, Begin, then write each Index/Data pair, Commit and wait
for Ready. Only then enable. Validation processes at most 64 words per idle board
poll and never runs during acquisition; this is a work bound, not measured timing.
Acquisition refuses Uploading/Validating. Structural geometry/route/interpolation
changes, preset load and sensor reset invalidate the map and disable correction.
Incomplete uploads are cancelled when the control lease expires. A completed map
can remain available after the uploader releases control. Maps and enable are not
stored in UserSets or Flash. Loading a UserSet requires a fresh map upload.

With exclusive camera handback and a qualified matching image, an explicit upload
can use:

```sh
python3 tools/upload_distortion_map.py COMPILED_DIRECTORY --upload --ip CAMERA_IP --bind HOST_IP --enable
```

The client requires stopped/drained/unlocked state and exact output dimensions.
It does not change dimensions, acquire frames, reset, save settings or overwrite
an existing volatile map. On failure retain the error; do not blindly repeat an
uncertain transaction. Inspect state and ownership first. Normal uploader exit
releases only its acquired control lease. Optical provenance must match the
configured sensor ROI/scaler path; firmware's generation token cannot prove that
the supplied calibration describes the actual lens.

## Memory, cache and lifecycle

Two dedicated 64-byte-aligned DDR banks total 2457600 bytes. They are statically
linked, separate from six camera DMA buffers and four conversion buffers. The
old PL-exposed bank remains protected through cancellation/failure until explicit
quiescence. A validated bank is cache-cleaned with a completion barrier, programmed
while idle and leased before reader start. Map memory is immutable during capture.

Each color frame arms its destination then starts one remap invocation; auto-restart
is disabled. DMA completion alone cannot publish a frame: the polling loop also
waits for AP_IDLE, reads final status, rejects errors and only then rearms. The
original timeout remains active while waiting. Normal stop drains the invocation
before sensor gating and lease release. A pending map reader blocks video-only
reset/recovery; a qualified system warm reset may be required for a stuck DDR or
stream invocation. A stop acknowledgement or lost controller is not quiescence.

See [FPGA contract](REMAP_V12_INTEGRATION.md) for offsets/reset/DDR wiring and
[capture gates](OFFLINE_CAPTURE_GATES.md) for baseline and remaining hardware work.
Run `bash tests/remap/run.sh` for map/control/reader sanitizer tests, compiler and
client tests, and the actual production poll function under deterministic MMIO.
Those tests do not measure camera FPS, DDR arbitration, optical quality or endurance.

## Bounded board evidence

Corrected-image evidence (archived development reference; see [verification scope](VERIFICATION.md))
records exact paired hashes and per-case settings. Identity measured14.293566 FPS;
+5/+7 measured14.294451 FPS; -5/-7 measured14.293981 FPS. Bypass comparison also
passed640x480 and1280x960 at about14.293 FPS, plus2592x1944 at an explicit1 FPS.
These measurements use the sensor test pattern, fixed exposure/gain and transient
packet spacing; they are not full-resolution maximum-rate or natural-scene claims.

Bad CRC, enabling an empty map, active map writes and read-only status writes are
rejected. Retained-map stop/start, stopped disable and width-change invalidation
pass. Adaptive-tone reference/history, live-black and logical LED regressions
pass on the same image; LED evidence is logical, not a visual observation.
Original runtime settings, tables and selectors were restored; acquisition is
stopped and control released. Maps remain volatile; no UserSet save, Flash or
boot change was performed. Lens-specific coefficients and compatible geometry
must be supplied separately before calling the output optically corrected.
