#!/usr/bin/env bash
set -eu
export CCACHE_DISABLE=1
cd "$(dirname "$0")/../.."
test_dir=$(mktemp -d /tmp/gpio-sccb.XXXXXX)
trap 'rm -rf "$test_dir"' EXIT
flags=(-Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Itests/gpio_sccb/stubs -I.)
gcc -std=c11 "${flags[@]}" -c AxiGpioSccb.c -o "$test_dir/driver.o"
g++ -std=c++14 "${flags[@]}" tests/gpio_sccb/test_gpio.cpp "$test_dir/driver.o" -o "$test_dir/test"
ASAN_OPTIONS=detect_leaks=0 "$test_dir/test"
g++ -std=c++14 "${flags[@]}" -x c++ AxiGpioSccb.c tests/gpio_sccb/test_gpio.cpp -o "$test_dir/test-cpp"
ASAN_OPTIONS=detect_leaks=0 "$test_dir/test-cpp"
