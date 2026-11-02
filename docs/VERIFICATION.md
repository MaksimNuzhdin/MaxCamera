# Verification scope and provenance

Original-workspace bounded qualification on2026-11-02: firmware0.3.15-ppc4-dev,
FPGA v13, ALINX AXU2CGB/AN5641. Published source packaging is not a new board test.

| Artifact | SHA256 |
|---|---|
| Qualified ELF | b2e516d6332f7fe047bfc9c65d308a5353628b38a0570214f690b7a8c39758ac |
| Qualified bitstream | d897a5eb2282bbc944c532b5996d5ca1076d1c002cb7d4743158b05ad19248ea |
| Qualified XSA | 4c93e54d70d55478270bc8fac51d27af4ddfc06063d39f681255c45753b3d34e |
| Original board evidence manifest | 2651ea7d5c81dd92487dcf71ce0c38fc46edadb161c00ee0c9ae6d97d78cc700 |
| Original FPGA evidence manifest | 29c496b6f054f87de44c40b4e4a41d4fd76676a4dad236c9653694b54e5e3ac9 |

RGB4x320x240→1280x960:57 frames at14.293804 FPS,20 us packet spacing, no
incomplete/missing/socket-drop/CSI/recovery failures. One saved frame matches Q12
exactly; seven saved format regression frames match v12. Remap, tone, live black,
logical LED and lifecycle checks pass. LED evidence is not visual observation.
Original runtime restored, camera stopped/released. No persistent writes.

Original routed resources:45832/47232 LUT,126/150 BRAM36,235/240 DSP at200 MHz;
WNS+0.429 ns, WHS+0.011 ns, WPWS+0.145 ns. These figures apply to the sealed
original image, not an unqualified regenerated implementation.

Export checks: R5 compilation/link, native protocol suite, remap/SCCB/HDR/map
regressions and language checks. GenApi schema/Aravis runtime validation is
optional and not rerun without the external runtime. Vivado fresh-project source
recreation/BD validation uses existing HLS exports; no full HLS/routing/board test
was performed for packaging. Read the included validation summary for exact scope.

Wider4x geometries, optical calibration, end-to-end HDR, AI inference and endurance
are not qualified. The image gallery is genuine v10 capture evidence with its own
labels and limitations, not v13 optical testing. Raw captures, full diagnostics,
bitstreams and rollback pairs remain preserved locally but are not committed.
Historical guides may name those archived evidence paths; such references are
provenance identifiers rather than files promised in this source-only repository.
