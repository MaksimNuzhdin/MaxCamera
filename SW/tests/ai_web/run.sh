#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
python3 tools/generate_web_rom.py
python3 -m unittest discover -s tests/ai_web -v
lwip_dir=${GIGE_LWIP_DIR:-/path/to/Vitis/2024.2/data/embeddedsw/ThirdParty/sw_services/lwip220_v1_0/src/lwip-2.2.0}
mkdir -p build/tests
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
    -DMAX_CAMERA_WEB_HTTP=1 -I gige -I gige/lwip -I "$lwip_dir/src/include" \
    tests/ai_web/test_http.c gige/GigeHttp.c -o build/tests/test_http
ASAN_OPTIONS=detect_leaks=0 build/tests/test_http
