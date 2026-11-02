#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
# Official AMD release, commit 80bc4808a99ab76ab304d80640047c2a64ae47dd.
# Retain the license with the headers. Do not modify third-party source.
archive=${1:-/tmp/maxcamera-vitis-libraries-2024.2.tar.gz}
expected=5fd9e52642116d0bcc180cafe7bb265f02ada1fec57aa9ba1911dfc75ab2dfe7
if [[ ! -f "$archive" ]]; then
    curl -fsSL https://codeload.github.com/Xilinx/Vitis_Libraries/tar.gz/refs/tags/v2024.2_rel -o "$archive"
fi
echo "$expected  $archive" | sha256sum -c -
mkdir -p build/deps
tar -xzf "$archive" -C build/deps \
    Vitis_Libraries-2024.2_rel/vision/L1/include \
    Vitis_Libraries-2024.2_rel/LICENSE.txt
