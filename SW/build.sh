#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
: "${VITIS_ROOT:?Set VITIS_ROOT to AMD Vitis 2024.2}"
: "${MAXCAMERA_PLATFORM:?Set MAXCAMERA_PLATFORM to the exported standalone_psu_cortexr5_0 domain}"
# Vitis 2024.2 bundles its CMake/Python runtime; avoid a host CMake mismatch.
set +u
source "$VITIS_ROOT/settings64.sh"
source "$VITIS_ROOT/tps/lnx64/lopper-1.1.0/env/bin/activate"
set -u
export LD_LIBRARY_PATH="$VITIS_ROOT/tps/lnx64/cmake-3.24.2/libs/Ubuntu:$VITIS_ROOT/tps/lnx64/python-3.8.3/lib:${LD_LIBRARY_PATH:-}"
export ESW_REPO="$VITIS_ROOT/data/embeddedsw"
cmake_bin=${CMAKE_BIN:-$VITIS_ROOT/tps/lnx64/cmake-3.24.2/bin/cmake}
"$cmake_bin" -S src -B build/firmware -G Ninja \
 -DCMAKE_TOOLCHAIN_FILE="$MAXCAMERA_PLATFORM/cortexr5_toolchain.cmake" \
 -DCMAKE_MODULE_PATH="$MAXCAMERA_PLATFORM" \
 -DCMAKE_INCLUDE_PATH="$MAXCAMERA_PLATFORM/include" \
 -DCMAKE_LIBRARY_PATH="$MAXCAMERA_PLATFORM/lib" \
 -DGIGE_VITIS_ROOT="$VITIS_ROOT" \
 -DMAX_CAMERA_WEB_HTTP="${MAX_CAMERA_WEB_HTTP:-OFF}" \
 -DMAX_CAMERA_AI_IPC="${MAX_CAMERA_AI_IPC:-OFF}" \
 -DMAX_CAMERA_NATIVE_Y8_CHECK=OFF -DMAX_CAMERA_YUV422_CHECK=OFF \
 -DMAX_CAMERA_GIGE_GPIO_SCCB=ON -DMAX_CAMERA_GIGE_CSI_BYTES=ON \
 -DMAX_CAMERA_GIGE_RAW10_HW=ON -DMAX_CAMERA_GIGE_FPGA_ISP=ON \
 -DMAX_CAMERA_DIAGNOSTIC_BARS=OFF -DMAX_CAMERA_FLASH_PROBE=OFF
"$cmake_bin" --build build/firmware
