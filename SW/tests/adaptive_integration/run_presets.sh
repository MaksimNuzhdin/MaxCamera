#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
export CCACHE_DISABLE=1
out=build/tests/adaptive-integration
mkdir -p "$out"
old=diagnostics/fpga-color-v9-20261027/source/gige
fixture=tests/adaptive_integration/fixtures/isp9-preset.bin
python3 - <<'CHECK'
from pathlib import Path
import hashlib,json
r=Path('tests/adaptive_integration/fixtures')
assert hashlib.sha256((r/'isp9-preset.bin').read_bytes()).hexdigest()==json.loads((r/'provenance.json').read_text())['fixture_sha256']
CHECK
# When retained historical sources are available, independently regenerate.
# Clean source distributions use the small immutable genuine legacy fixture.
if [[ -f "$old/GigePreset.c" ]];then
 cc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -I"$old" \
  tests/adaptive_integration/make_legacy.c "$old/GigePreset.c" "$old/GigeImage.c" -o "$out/make_legacy"
 "$out/make_legacy" "$out/isp9-preset.bin"
 cmp "$fixture" "$out/isp9-preset.bin"
fi
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Igige \
 tests/adaptive_integration/test_presets.c gige/GigePreset.c gige/GigeImage.c -o "$out/test_presets"
ASAN_OPTIONS=detect_leaks=0 "$out/test_presets" "$fixture"
