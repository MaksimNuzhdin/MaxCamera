#!/usr/bin/env bash
# Build the relocated EL3 runtime needed for a JTAG boot in hardware boot mode0.
set -euo pipefail
cd "$(dirname "$0")/.."
source_dir=${MAX_CAMERA_TFA_SOURCE:-/tmp/maxcamera-tfa-src}
mirror=${MAX_CAMERA_TFA_MIRROR:-/path/to/petalinux-project/build/downloads/git2/github.com.Xilinx.arm-trusted-firmware.git}
revision=14cea4616b6edaceabb607c7c92332436a1699e5
mkdir -p "$source_dir"
if [[ ! -f "$source_dir/Makefile" ]]; then
    git --git-dir="$mirror" archive "$revision" | tar -x -C "$source_dir"
fi
make -C "$source_dir" -j4 PLAT=zynqmp \
    CROSS_COMPILE="${MAX_CAMERA_AARCH64_CROSS:-/path/to/Vitis/2024.2/gnu/aarch64/lin/aarch64-none/bin/aarch64-none-elf-}" \
    PRELOADED_BL33_BASE=0x20000000 XILINX_OF_BOARD_DTB_ADDR=0x22000000 \
    ZYNQMP_CONSOLE=cadence1 BUILD_BASE="$PWD/build/ai-linux/tfa-uart1" bl31
