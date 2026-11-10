# Shared GigE Vision and web console candidate

The camera's existing Ethernet IP now has an optional TCP80 HTTP endpoint.
R5 remains the sole owner of GEM3, lwIP, GVCP and GVSP. A53 Linux has no Ethernet
netif and never accesses GEM registers. USB transport is not required.

This is an experimental, explicitly enabled build. Offline tests and successful
compilation are not evidence of lossless video under HTTP load. Keep physical
board observations distinct from CPU emulation and host tests.

## Behavior and resource limits

- R5 serves `index.html`, `style.css` and `app.js` from generated ROM assets.
  The browser uses relative API URLs on the same camera IP.
- R5 forwards only GET `/api/status` and POST `/api/config`, `/api/profiles`,
  `/api/gallery` to the existing Linux console routes. Token, Origin and Host
  checks are preserved. Only the numeric current camera IP is accepted as Host.
  The unauthenticated HTTP console is for a trusted development network, not
  Internet exposure. Tokens prevent cross-origin browser writes; they are not
  user authentication or transport encryption.
- Four TCP clients,1460-byte receive window,2920-byte send buffer per client,
  no out-of-order queue,8192-byte maximum headers, one Linux command slot.
  Since0.3.18-webqueue-dev, concurrent status GETs wait in the existing client
  headers and are dispatched oldest-first when the shared slot is free. No
  extra upload slots are allocated. Busy POSTs return503; quarantine and
  sequence exhaustion still fail closed.
  The0.3.19 candidate reserves16 lwIP TCP PCBs for four application clients
  plus closing/retransmitting connections; it still has four HTTP request
  contexts and one Linux slot. This is bounded connection-lifetime headroom,
  not additional video bandwidth or unrestricted clients.
  Request bodies can contain the existing4 MiB maximum reference upload.
  Responses are limited to128 KiB. Connections close after one response; no
  pipelining, chunked requests, Expect/Continue, ambiguous Content-Length or
  absolute request URLs are supported.
- All POSTs are rejected with409 while acquisition is active. A partially
  received upload is cancelled before publication if acquisition starts.
  The Linux backend checks acquisition state again when dispatching a command.
  GenICam ownership additionally protects configuration inside R5.
- New response payload is enqueued at most512 bytes per500 us when idle or
  2000 us while streaming. This is a ceiling for new application payload,
  not total wire bandwidth: TCP ACKs, headers and retransmissions add traffic.
  The paced poll runs after the existing video service burst.
- HTTP sends wait for at least16 free TX descriptors. The GEM link-output path
  also refuses IPv4 TCP traffic below that reserve, including lwIP timer-driven
  retransmissions. GVCP/GVSP retain their existing paths. This is descriptor
  backpressure, not Ethernet QoS or a guarantee against saturated-link losses.
- An incomplete client expires after60 seconds. Linux completion has a10-second
  deadline after publication. Neither timeout nor browser disconnection frees
  a published request slot until the exact Linux completion arrives. A failed
  backend may leave the slot quarantined until reboot; static pages remain
  available. The network event loop never waits for Linux.

The existing no-reset-of-one-processor rule remains: reboot A53 and R5 together.
There is no automatic worker restart. Source additions do not change saved
UserSets, flash contents, IP provisioning or USB modes.

## Reserved-memory ABI

All addresses are inside the existing16 MiB no-map carveout
`0x1f000000–0x1fffffff`. The AI mailbox and web configuration extension are
unchanged. In web-enabled R5 builds the MPU mapping covers the full reservation.
R5 firmware load segments still end below `0x10000000`.

| Region | Address | Capacity |
| --- | --- | --- |
| HTTP control |0x1f004000|4096 bytes mapped, first16 words reserved|
| Request |0x1f010000|4 MiB +8192 bytes|
| Response |0x1f500000|128 KiB|

Control words are little-endian32-bit values:

| Word | Producer | Meaning |
| --- | --- | --- |
|0–1|R5|Magic0x31505448 (`HTP1`), version1|
|2|R5|Request sequence, published last with DMB|
|3|Linux|Matching response sequence, published last with DMB|
|4|R5|Exact request byte length|
|5|Linux|Exact response byte length|
|6|R5|Current camera IPv4 address in host numeric order|
|7|R5|Acquisition active/busy flag|
|8|Linux|Backend loop heartbeat, diagnostic only|

R5 owns request memory until publication. Linux reads the complete immutable
request and writes an immutable response before committing word3. R5 releases
that slot only after the response has been copied into lwIP-owned TCP buffers,
or after consuming a completion for a disconnected/timed-out browser. TCP ACKs
then govern the lifetime of lwIP's private copies. Sequence exhaustion fails
closed. Device-mapped A53 payload access uses aligned native32-bit scalar loops
and a padded tail; Python does not bulk-copy into Device memory.

## Isolated build

```sh
MAX_CAMERA_AI_IPC=ON MAX_CAMERA_WEB_HTTP=ON bash build.sh
python3 tools/build_ai_rootfs.py --ethernet-web --out build/ethernet-web-linux
python3 tools/build_ai_sd_boot.py --out build/ethernet-web-linux \
  --r5 build/firmware/MaxCameraApp.elf --bit /path/to/paired-v13.bit
MAX_CAMERA_AI_ROOTFS=build/ethernet-web-linux/rootfs.cpio.gz \
  MAX_CAMERA_AI_QEMU_LOG=build/ethernet-web-linux/qemu-selftest.log \
  bash tools/test_ai_linux_qemu.sh
```

The rootfs flag starts `ai.http_transport` with no Linux socket or network driver.
Only this opt-in build mounts SD read/write for explicit profiles/reference
uploads. Without SD, preset storage is under `/run` and lost on reboot.
The default build keeps TCP/HTTP disabled and SD read-only. The USB gadget
candidate and Ethernet backend are mutually exclusive rootfs options.

`tools/run_ai_linux_jtag.tcl` accepts `MAX_CAMERA_AI_LINUX_DIR` for these isolated
boot artifacts, `MAX_CAMERA_AI_R5` for the new ELF and `MAX_CAMERA_AI_BL31` for
the existing relocated TF-A ELF. It performs a volatile system/RAM load; do not
run it while another user is acquiring or using the board. No flash/SD image is
programmed. Save raw boot/capture evidence before making hardware claims.

## Validation and limitations

`bash tests/ai_web/run.sh` runs HTTP route and native shared-memory surrogate
tests, then raw TCP callback tests under AddressSanitizer/UndefinedBehaviorSanitizer.
These cover framing, fragmented headers, pending-slot isolation, disconnect and
timeout quarantine, write denial during acquisition, TX pacing/backpressure,
exact HTTP lengths, concurrent read queuing with immutable response isolation,
queued-reader disconnect/backend-timeout handling, token/Host preservation and volatile profile/config behavior.
The controlled TCP boundary does not replace real wire-level interoperability.
Existing GigE and AI runtime regressions should also pass.

QEMU validates the A53 kernel/runtime, actual inference, console route dependency
closure and native HTTP copy helpers on normal emulated memory. It does not
validate R5/A53 physical cache attributes, GEM behavior or simultaneous traffic.

A bounded hardware trial should compare the same video mode with HTTP idle
versus repeated asset/status reads, verify write rejection during acquisition,
and record complete frames, lost packets, CSI errors, drops and delivered FPS.
Repeat at maximum intended resolution/rate, multiple clients, resends and a
saturated link before claiming full performance isolation. A640x480 trial alone
would not qualify all modes or sustained/endurance performance.

Implementation reference: [lwIP raw TCP API](https://www.nongnu.org/lwip/2_1_x/group__tcp__raw.html).

The current unattended procedure is documented in
[the night campaign guide](../../../docs/NIGHT_CAMPAIGN.md). A running campaign
is provisional; inspect its completed phase results and restoration records.
