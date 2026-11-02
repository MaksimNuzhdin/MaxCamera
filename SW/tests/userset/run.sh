#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
export CCACHE_DISABLE=1
out=$(mktemp -d /tmp/maxcamera-userset.XXXXXX)
trap 'rm -rf "$out"' EXIT
cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Igige gige/GigeUserStore.c tests/userset/test_store.c -o "$out/test"
ASAN_OPTIONS=detect_leaks=0 "$out/test"
c++ -x c++ -std=c++14 -Wall -Wextra -Werror -Igige gige/GigeUserStore.c tests/userset/test_store.c -o "$out/test-cpp"
"$out/test-cpp"

cc -std=c11 -Wall -Wextra -Werror -Igige gige/GigeTiming.c tests/userset/test_timing.c -o "$out/timing"
"$out/timing"

for source in GigeUserStore GigePreset GigeVision GigeImage; do
    cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g -Igige -c "gige/$source.c" -o "$out/$source.o"
done
c++ -std=c++14 -Wall -Wextra -Werror -fsanitize=address,undefined -g -Itests/userset/stubs -Igige \
    tests/userset/test_commands.cpp gige/GigeUserSets.cpp "$out/GigeUserStore.o" "$out/GigePreset.o" "$out/GigeVision.o" "$out/GigeImage.o" -o "$out/commands"
ASAN_OPTIONS=detect_leaks=0 "$out/commands"
