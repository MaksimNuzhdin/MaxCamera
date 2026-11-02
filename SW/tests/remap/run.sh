#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
export CCACHE_DISABLE=1
out=$(mktemp -d /tmp/maxcamera-remap-map.XXXXXX)
trap 'rm -rf "$out"' EXIT
gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Igige \
  gige/GigeRemapMap.c tests/remap/test_map.c -o "$out/test"
ASAN_OPTIONS=detect_leaks=0 "$out/test"
gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Igige \
  gige/GigeRemapMap.c gige/GigeRemapControl.c tests/remap/test_control.c -o "$out/control"
ASAN_OPTIONS=detect_leaks=0 "$out/control"
gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Igige \
  gige/GigeRemapReader.c tests/remap/test_reader.c -o "$out/reader"
ASAN_OPTIONS=detect_leaks=0 "$out/reader"
python3 experiments/remap_integer/test_map.py
python3 tests/remap/test_upload.py
python3 tests/remap/test_poll.py
