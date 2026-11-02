#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p build/tests
cc -std=c11 -Wall -Wextra -Werror -Itests/leds/stubs -Idrivers/LedsController \
    tests/leds/test_leds.c drivers/LedsController/LedsController.c -o build/tests/test_leds
build/tests/test_leds
