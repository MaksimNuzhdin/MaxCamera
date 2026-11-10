# Offline AI IPC proposal contract

Run: `python3 -m unittest discover -s tests/ai_ipc_contract -v`.

This reference oracle tests selected design semantics only. It is not linked into
firmware, does not access DDR/GPIO, and does not create GenApi nodes. Its RGB8
format ID is a private proposal enum, not a PFNC value. Descriptor offsets are
relative to a future dedicated carveout, never unrestricted physical addresses.
The wire layouts are exploratory, not a stable published ABI.

Covered: exact little-endian header bytes, length/version/bounds validation,
immutable result latching, configuration/session rejection, no pending queue,
lease retention on disable/timeout, and fail-closed generation wrap. Indexed
object access, frame-coordinate bounds, timestamp age, admission of exact offered
sequences, access-session ownership and worker cancellation acknowledgements need
additional design/implementation tests before production use. Python atomicity
is not evidence for CPU barriers, cache visibility or torn-read prevention.

Hardware publication protocol proposed for later implementation:
1. Reserve disjoint control, snapshot and worker regions in every linker/OS map;
   exclude them from general allocation. Align payload slots and separate producer
   and consumer control words by cache line. Validate dimensions/stride/length.
2. R5 observes DMA completion, performs required DMA/cache synchronization and
   acquires a capture lease before publishing a descriptor. Pin selection must
   exclude both GVSP and worker-held buffers. Never publish an in-flight writer.
3. Publish descriptor/payload visibility before the READY generation using the
   architecture-required barrier. Consumer acquires control before invalidating
   payload cache lines and reading. A noncacheable control page still needs
   ordering barriers. Exact memory attributes require BSP/interconnect review.
4. Worker copies/resizes into its exclusive snapshot, acknowledges the exact
   lease after all reads complete, then infers. Payload cache cleaning is needed
   for CPU-written buffers before another noncoherent agent reads them.
5. Worker result publication follows the same clean/barrier/commit discipline.
   R5 validates a coherent generation snapshot, copies a bounded result into its
   own memory, then serves a latched GenICam view. Never trust a raw pointer.
6. Timeout disables new offers; it does not make an old reader safe to overwrite.
   Confirm quiescence before releasing leases or changing core/session mappings.

The first hardware experiment should test this publication protocol with a
checksum worker and canaries, not a model, after separately approved startup.
