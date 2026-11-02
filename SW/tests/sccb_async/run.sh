#!/usr/bin/env bash
set -euo pipefail
export CCACHE_DISABLE=1
cd "$(dirname "$0")/../.."
out=$(mktemp -d /tmp/maxcamera-sccb-async.XXXXXX)
trap 'rm -rf "$out"' EXIT
flags=(-Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer -Itests/gpio_sccb/stubs -I.)
sources=(AxiGpioSccb.c AxiGpioSccbAsync.c gige/GigeSensorTelemetry.c)
objects=()
for source in "${sources[@]}"; do
    object="$out/$(basename "$source").o"
    gcc -std=c11 "${flags[@]}" -c "$source" -o "$object"
    objects+=("$object")
done
g++ -std=c++14 "${flags[@]}" tests/sccb_async/test.cpp "${objects[@]}" -o "$out/test"
ASAN_OPTIONS=detect_leaks=0 "$out/test"
g++ -std=c++14 "${flags[@]}" -x c++ "${sources[@]}" tests/sccb_async/test.cpp -o "$out/test-cpp"
ASAN_OPTIONS=detect_leaks=0 "$out/test-cpp"

g++ -std=c++14 "${flags[@]}" tests/sccb_async/test_access.cpp "${objects[@]}" -o "$out/access"
ASAN_OPTIONS=detect_leaks=0 "$out/access"
