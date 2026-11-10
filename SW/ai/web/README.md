# MaxCamera AI Studio

Local, buildless English web console for the A53 Linux runtime.
The optional [shared GigE/Ethernet transport](ETHERNET.md) serves it through
R5 on TCP80 at the camera's existing IP, without USB or a Linux Ethernet device. No cloud assets,
external JavaScript, Node.js, package installation, or host inference are needed.
The UI uses the eight capabilities and actual parameter limits in `ai/runtime.py`
and `gige/GigeAI.c`. This implementation has offline verification only; no new
camera/USB/SD hardware qualification is claimed.

## Preview without a board

From the application root:

```sh
python3 -m ai.web_service --preview --bind 127.0.0.1 --port 18765 \
  --storage /tmp/maxcamera-web-preview
```

Open `http://127.0.0.1:18765`. The explicit offline mode offers configuration,
profile save/load/export/import, and reference-image staging. It never starts
inference, advertises a connected camera, or invents detections. Preview storage
is on this computer. Uploaded references are not validated for faces without the
real worker. Real mode fails closed if `/dev/mem` or the R5 extension is absent.

## User workflow

1. Choose supported detectors, score, face cosine threshold, and sampling period.
2. Apply while no GenICam client owns the camera. R5 acknowledges the entire
   configuration atomically; availability, ranges and ownership are checked there.
3. Acquire FPGA RGB8/BGR8 video with the existing GenICam client. That client then
   owns configuration; web writes are denied during its session. The web console
   does not start acquisition, alter transport, save UserSets, or take GVCP control.
4. Save named profiles and export a JSON backup. Loading/importing never applies
   settings automatically. A reboot starts with AI disabled.
5. Add JPEG/PNG reference images under explicit English identity labels. Stored
   images are staged, not proof of enrollment. The worker bounds dimensions,
   decodes images, requires exactly one face and reloads the gallery between
   inference calls. Diagnostics distinguish saved files, enrolled references and
   rejected references. No worker restart or model reload occurs. Models remain
   loaded after the disposable boot weights are released.

Web uploads are limited to 4 MiB each, dimensions to 12 megapixels / 8192 pixels,
and uploaded gallery storage to 32 MiB / 256 reference images. Existing local references up to 32 MiB
remain supported within the same decoded dimension bounds. Recognition labels
become public results on the existing GenICam interface; do not enroll unintended
identities. The console does not expose a terminal, arbitrary file access, model
upload, or enrollment images over HTTP. Application-specific recognition/OCR
accuracy is still unqualified. Plate/object preset directories remain reserved.

The console contains diagnostics rather than a live video player. Video remains
on GVSP. Worker health and gallery counts are reported through an atomic runtime
file; they do not establish image accuracy or throughput.

## R5 bridge

The version-1 mailbox retains its existing frame/result layout. Reserved word14
contains the optional `WEB1` capability (`0x31424557`). Old R5 firmware is rejected.
The extension is beyond all 32 object records (words96–607):

| Word | Producer | Meaning |
| --- | --- | --- |
|640|R5|State bits: worker alive=1, GVCP owner=2, web lease=4|
|644|Web|Command sequence, committed last with DMB|
|645|R5|Acknowledged command sequence|
|646|R5|Status:0 success,0x8002 invalid,0x8006 owned,0x8007 unavailable|
|647–650|Web|Mask, minimum score, sample period us, face cosine threshold|
|651|Web|Heartbeat, every250 ms|
|652–653|R5|Live error code, last inference time us|

R5 applies commands in its normal poll loop. The existing configuration generation
brackets all four settings. A GVCP session has priority and inherits the currently
applied settings; its writes and control-loss reset retain the existing behavior.
Web commands cannot renew a GVCP session. A web-owned idle configuration expires
if the service heartbeat stops for3 seconds. A live service retains its lease even
when the browser closes. Disabling or timing out never revokes a capture-buffer
lease. A command timeout reports an unknown outcome and refuses to overwrite a
still-pending command. Worker restarts remain forbidden; reboot both processors
if the worker fails.

## Optional USB transport candidate

Host USB inspection was performed on November 10, 2026. The AXU2CGA/B manual,
page15, establishes that all four USB-A connectors are downstream HOST ports
behind the onboard USB hub. They do not expose a device/OTG connection to a PC.
Switching DWC3 to peripheral mode does not bypass this physical hub topology.
The candidate scripts below are for a separately verified device-capable hardware
connection; they are not a supported recipe for the four AXU2CGA/B USB-A ports.

The host's existing NCM network at192.168.7.1/30 belongs to a BeagleBone Black,
with peer192.168.7.2. It is not the camera. The proposed camera address conflicts
on this host and must not be assigned without choosing a separate subnet.
No host network, flash, SD card, or camera boot configuration was changed.
See `diagnostics/usb-host-inspection-20261110/README.md` for inspection scope.
Default rootfs/boot builds keep USB disabled and preserve the read-only SD policy.

The installed vendor kernel configuration has built-in DWC3 dual-role,
USB gadget/configfs, ECM and RNDIS. The existing boot builder disables both USB
wrappers. Use only a board-verified device/OTG port; USB–UART/JTAG bridges cannot
provide a Linux gadget connection. Verify the connector wiring, VBUS/device role,
reset GPIO, pinmux, FSBL clocks and controller ownership first. Enabling a DWC3
node alone does not prove a working physical port. Linux GPIO/PHY controllers are
not automatically enabled because R5 resource ownership must be preserved.

For a future candidate, `tools/build_ai_rootfs.py` accepts `--usb-mode ecm|rndis`,
`--usb-vid`, `--usb-pid`, `--usb-serial`, `--usb-udc`. Supply an assigned VID/PID and
unique serial, plus the verified sysfs controller name. The boot builder separately
accepts `--usb-device-node` with the verified `snps,dwc3` child path. Keep these
options consistent; USB remains opt-in. Rebuild R5, rootfs, BOOT.BIN and the SD
image together. Do not replace a known-good image until board qualification.

At boot, `linux/ai/usb-network` creates one ECM (typical Linux/macOS) or RNDIS
(Windows; driver availability must be verified) function. It assigns the camera
`192.168.7.2/24` and serves DHCP `192.168.7.10–20` on `usb0`. It advertises no
router/DNS and never bridges R5 Ethernet. A host using DHCP can then open
`http://192.168.7.2`; hosts without automatic USB driver/DHCP support need manual
configuration. Check subnet conflicts before using that address.

Only the optional USB build mounts SD read/write for presets/profiles; BOOT.BIN
is never rewritten by the console. Keep a card backup and power down cleanly.
Without SD, runtime storage is volatile and the UI states this. The web server
binds to the USB address only. It has no password/TLS and is intended for a directly
attached trusted host, not a LAN/Internet listener. Same-origin writes require a
random boot token, expected Host and matching Origin; static assets use CSP and
no-store. No CORS is enabled. These protections are not host authentication.

The rootfs integration packages all console assets and startup scripts. Network
startup failure leaves the AI worker and serial diagnostic shell available.
Web process failure is not silently restarted; an expired R5 lease disables its
configuration. USB host enumeration/DHCP, SD writes, simultaneous video/AI/web
load and R5 target builds remain required hardware/deployment checks.

Sources: [Linux configfs gadget documentation](https://docs.kernel.org/usb/gadget_configfs.html),
[AMD DWC3 Linux driver guidance](https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18841729/).

## Offline validation

```sh
python3 -m unittest discover -s tests/ai_web -v
python3 -m unittest discover -s tests/ai_runtime -v
cc -std=c11 -Wall -Wextra -Werror -I gige \
  tests/ai_runtime/test_control.c gige/GigeAI.c -o /tmp/maxcamera-ai-control
/tmp/maxcamera-ai-control
python3 tools/check_project_language.py
```
