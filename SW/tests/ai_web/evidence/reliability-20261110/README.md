# November10 evening reliability development

These records precede overnight completion. They retain failed attempts and
version/receiver distinctions; no completed overnight or hour PASS is claimed.
The initial matrix read an unlatched AI sequence, so its AI assertions failed.
That harness defect is distinct from actual HTTP503 and the retained interleaving
receiver failure. Later trials use explicit result snapshots and generation tags.

The original single-frame receiver declared162 missing payloads after the next
leader; all632 packet IDs for each of the two frames exist in its trace.
The published deterministic-pixel fixture preserves their observed packet order,
not original image bytes. Four-frame reassembly, whole-frame gap detection,
trailer/window failures and wraparound are tested independently. This does not
reclassify the November9 hour with449 socket drops.

The0.3.18 queue passed short concurrent API tests but had9 URL-level failures in
one two-minute loaded20us window, with1716 complete video frames. The100us
window had1716 good frames and7761 HTTP200 responses. The0.3.19 candidate adds
TCP PCB headroom; its initial30-second four-client test had2409 HTTP200 replies.
Longer gates and the separately supervised night campaign retain their own scope.

The source export builds with external Vitis/BSP dependencies. Host verification
includes18 native UDP tests under ASan/UBSan, exact observed-order replay,
13 Python web/load tests and C HTTP sanitizer checks, plus GigE regressions.
New MicroSD artifacts are local candidates with FAT/readback checks; physical
card boot/persistence remains unqualified. No new raw captures, runtime/model
binaries or enrollment data are published.

## Further bounded checks

The initial0.3.19 gate kept all HTTP requests successful but its100us loaded
window lost3 packets in the host socket (857 good,1 incomplete); that failure
remains recorded. The next gate uses exact statistics accumulated per packet
and native receiver affinity on CPU8. Both60-second100us face windows pass858
frames, zero bad/missing/socket drops, advancing generation-matched AI results,
and3704 HTTP200 replies under four-reader load. This is not an hour result.

An early-request controlled-loss trial passes100 frames with500 byte-exact
recovered leader/payload/trailer packets and401 HTTP200 requests. The earlier
late-request attempt completed40 frames before PACKET_UNAVAILABLE and is
retained locally. Early and trailer-triggered request timing are different scopes.

The full-array YUV pacing study is mixed. At5us,53 frames in10 seconds pass at
5.211289 FPS with no socket loss. At1/10/15us packets were lost;20us delivered
about half nominal FPS. No universal pacing guarantee or full-array endurance.
The original overnight preflight stopped with verified restoration after its
failures; the later frozen-source overnight-v2 campaign is separate and live.
