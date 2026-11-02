#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")/../.."
export CCACHE_DISABLE=1
out="$(mktemp -d /tmp/ov5640-tests.XXXXXX)"
trap 'rm -rf "$out"' EXIT
gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -I. Ov5640.c tests/ov5640/test_ov5640.c -o "$out/c-test"
ASAN_OPTIONS=detect_leaks=0 "$out/c-test"
g++ -x c++ -std=c++14 -Wall -Wextra -Werror -I. Ov5640.c tests/ov5640/test_ov5640.c -o "$out/cpp-test"
"$out/cpp-test"
gcc -std=c11 -Wall -Wextra -Werror -DOV5640_ENABLE_OTP_PROGRAM=1 -I. -c Ov5640.c -o "$out/manufacturing.o"

gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Itests/i2c/stubs -I. Ov5640.c Ov5640MaxIic.c tests/ov5640/test_adapter.c -o "$out/adapter-test"
ASAN_OPTIONS=detect_leaks=0 "$out/adapter-test"
python3 tests/ov5640/test_native_y8_check.py
