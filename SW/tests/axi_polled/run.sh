#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")/../.."
export CCACHE_DISABLE=1
out="$(mktemp -d /tmp/axi-iic-tests.XXXXXX)"
trap 'rm -rf "$out"' EXIT
gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Itests/axi_polled/stubs -Itests/i2c/stubs -Isrc -I. src/AxiIicPolled.c tests/axi_polled/test_polled.c -o "$out/test"
ASAN_OPTIONS=detect_leaks=0 "$out/test"
g++ -x c++ -std=c++14 -Wall -Wextra -Werror -Itests/axi_polled/stubs -Itests/i2c/stubs -Isrc -I. src/AxiIicPolled.c tests/axi_polled/test_polled.c -o "$out/test-cpp"
"$out/test-cpp"
