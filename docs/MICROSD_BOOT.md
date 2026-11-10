# MicroSD image: Ethernet web console and A53 analytics

Prepared November10,2026 for AXU2CGB + AN5641, the paired v13 FPGA and the
experimental0.3.17-qa-sccb-dev R5 source with AI and HTTP enabled. The boot
payload is the tested RAM candidate; its SHA256 identifies the exact build.
Physical SD boot, cold-start compatibility and persistence are still unqualified.

## Files and verification

The local distribution is under `build/ethernet-web-linux/`:

- `maxcamera-ethernet-web-20261110.img`: complete MBR disk image,513 MiB.
- `MICROSD-SHA256SUMS`: SHA256 for that complete image.
- `BOOT.BIN`: FSBL, PMUFW, paired FPGA/R5, relocated TF-A, Linux Image, DTB,
  initramfs and the EL2 entry stub. This image bypasses U-Boot deliberately.
- `rootfs-manifest.json`, `boot-manifest.json`, `SHA256SUMS`: build provenance.

The FAT32 partition starts at sector2048 and contains BOOT.BIN, HASHES.TXT,
README.TXT and PRESETS/FACES, PRESETS/PLATES, PRESETS/OBJECTS. Only FACES is
consumed as reference enrollment; the other two folders remain reserved.
No identities or user photographs are supplied. The packaged filesystem is
checked with `fsck.fat -n`; BOOT.BIN is read from its final disk-image chain and
hash-compared with the input. These checks do not emulate board SD boot.

```sh
cd build/ethernet-web-linux
sha256sum -c MICROSD-SHA256SUMS
```

Use a blank card of at least1 GB with a known-good backup. Write the complete
`.img` to the whole intended removable device with your normal disk-imaging
utility, verify the write and eject safely. Copying the `.img` file into an
existing filesystem is not disk imaging. This task does not select, erase or
write a physical device. Check the board manual for the actual revision's SD
boot-switch settings; do not infer switch positions from JTAG boot.

## First boot and user access

1. Insert the prepared card with power removed and select the documented SD
   boot mode. Connect the normal Gigabit Ethernet interface and approved power.
2. Determine the camera's actual DHCP address through your router or GigE
   client.10.0.0.10 was the November10 development address, not a fixed setting.
3. Open `http://CAMERA_IP/`. The UI should say GigE Ethernet and AI worker Online.
   The web console offers settings and diagnostics, not a shell or SSH service.
4. While idle, select AI features and thresholds, then apply. Start FPGA
   RGB8/BGR8 acquisition with a GenICam client. That client owns settings during
   its session; web modifications are rejected while streaming.
5. Stop/drain and release control afterward. AI defaults off after boot. Loading
   a saved web profile fills the editor; applying it is explicit.

The four USB-A connectors are HOST ports behind a hub. An ordinary cable from
one of them to a PC cannot provide this web connection. USB-UART/JTAG remains
separate diagnostic access. No USB gadget is enabled in this SD candidate.

## Presets and power loss

This opt-in web build mounts the card read/write for profiles/reference images.
Profiles are stored in PRESETS/web-profiles.json. Reference images are stored in
PRESETS/FACES/identity-label and validated for exactly one face by the worker.
Only explicit uploads enroll identities; names become recognition result labels.
With no mounted SD, the UI announces volatile runtime storage.

Finish writes and use a verified clean shutdown/unmount procedure before
removing power or the card. A browser shutdown button, power-loss-safe FAT
storage, physical persistence testing and verified serial-console routing are
not delivered as qualified features. Never assume a RAM-loaded session proves
what will cold-boot from a card. Preserve a known-good boot source for rollback.

## Rebuild

Follow [the Ethernet build guide](../SW/ai/web/ETHERNET.md) to build the R5/rootfs
and BOOT.BIN together. Then:

```sh
python3 tools/package_ai_microsd.py --out build/ethernet-web-linux \
  --image-name maxcamera-ethernet-web-20261110.img --date 2026-11-10 \
  --readme build/ethernet-web-linux/SD-README.txt
```

In the portable repository, run the equivalent tools from SW/ after compiling
with `MAX_CAMERA_AI_IPC=ON MAX_CAMERA_WEB_HTTP=ON bash build.sh`. Vendor SDK,
Linux/model/runtime licenses and a fresh-machine rebuild remain separate gates;
this local engineering image is not a completed third-party redistribution audit.
