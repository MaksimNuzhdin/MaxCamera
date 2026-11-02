#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
export CCACHE_DISABLE=1
out=${MAXCAMERA_HDR_TEST_OUT:-/tmp/maxcamera-hdr-tests}
mkdir -p "$out"
for name in merge session worker; do
 sources=(experiments/hdr/bracket_merge.c "experiments/hdr/test_${name}.c")
 if [[ "$name" == session ]]; then sources+=(experiments/hdr/bracket_session.c); fi
 cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g "${sources[@]}" -o "$out/test_$name"
 ASAN_OPTIONS=detect_leaks=0 "$out/test_$name"
done
cc -std=c11 -Wall -Wextra -Werror -c experiments/hdr/bracket_merge.c -o "$out/merge.o"
cc -std=c11 -Wall -Wextra -Werror -c experiments/hdr/bracket_session.c -o "$out/session.o"
g++ -std=c++17 -Wall -Wextra -Werror experiments/hdr/test_session_link.cpp "$out/session.o" "$out/merge.o" -o "$out/test_link"
"$out/test_link"

python3 experiments/hdr/test_plateaus.py
