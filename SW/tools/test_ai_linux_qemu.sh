#!/usr/bin/env bash
# CPU/system emulation only; does not emulate the camera, PL or physical IPC.
set -euo pipefail
cd "$(dirname "$0")/.."
qemu=${MAX_CAMERA_QEMU:-/home/maxim/workarea/petalinux_2024_2/sysroots/x86_64-petalinux-linux/usr/bin/qemu-system-aarch64}
kernel=${MAX_CAMERA_LINUX_IMAGE:-/home/maxim/workarea/axu2cgb/axu2cgb_fpga/images/linux/Image}
rootfs=${MAX_CAMERA_AI_ROOTFS:-build/ai-linux/rootfs.cpio.gz}
log=${MAX_CAMERA_AI_QEMU_LOG:-build/ai-linux/qemu-selftest.log}
timeout 1200 "$qemu" -machine virt -cpu cortex-a53 -smp 1 -m 752M \
    -nographic -no-reboot -kernel "$kernel" -initrd "$rootfs" \
    -append 'console=ttyAMA0,115200 rdinit=/init maxcamera.selftest=1 cma=0 swiotlb=128 rootfstype=ramfs' \
    > "$log" 2>&1
python3 - "$log" <<'PY'
import json
import re
import sys
from pathlib import Path
text=Path(sys.argv[1]).read_text()
match=re.search(r'\{"status": "PASS".*\}',text)
if not match or 'MAXCAMERA_SELFTEST_EXIT=0' not in text or 'Power down' not in text:
    raise SystemExit('A53 Linux self-test failed; inspect '+sys.argv[1])
print(json.dumps(json.loads(match.group()),indent=2))
PY
