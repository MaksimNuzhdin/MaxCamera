#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
export CCACHE_DISABLE=1
out=build/tests/adaptive-integration
mkdir -p "$out"
cc -std=c11 -Wall -Wextra -Werror -DGIGE_POLYPHASE_SUPPORT=1 \
 -fsanitize=address,undefined -Igige tests/adaptive_integration/test_controls.c \
 gige/GigeFpgaIsp.c gige/GigeFpgaOutput.c gige/GigeImage.c -o "$out/test_controls"
ASAN_OPTIONS=detect_leaks=0 "$out/test_controls"
cc -std=c11 -Wall -Wextra -Werror -DGIGE_POLYPHASE_SUPPORT=1 \
 -DTEST_SIGNATURE=GIGE_FPGA_V12_SIGNATURE -fsanitize=address,undefined -Igige \
 tests/adaptive_integration/test_controls.c gige/GigeFpgaIsp.c \
 gige/GigeFpgaOutput.c gige/GigeImage.c -o "$out/test_v12_controls"
ASAN_OPTIONS=detect_leaks=0 "$out/test_v12_controls"
cc -std=c11 -Wall -Wextra -Werror -DGIGE_POLYPHASE_SUPPORT=1 \
 -DTEST_SIGNATURE=GIGE_FPGA_V13_SIGNATURE -fsanitize=address,undefined -Igige \
 tests/adaptive_integration/test_controls.c gige/GigeFpgaIsp.c \
 gige/GigeFpgaOutput.c gige/GigeImage.c -o "$out/test_v13_controls"
ASAN_OPTIONS=detect_leaks=0 "$out/test_v13_controls"
bash tests/adaptive_integration/run_presets.sh
if [[ -f ../HW/build/hls/camera_adaptive_tone/solution/impl/ip/component.xml ]]; then
 python3 tools/check_adaptive_tone_abi.py
else
 echo "SKIP exported HLS ABI comparison: run HW/build_hls.tcl first"
fi
python3 tests/leds/test_reset_mapping.py
