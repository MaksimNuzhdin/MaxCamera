#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
python3 tests/gige/check_profile.py
python3 tests/gige/check_standard_profile.py
if [[ -d build/aravis/usr/lib/x86_64-linux-gnu/girepository-1.0 ]]; then
    env GI_TYPELIB_PATH=build/aravis/usr/lib/x86_64-linux-gnu/girepository-1.0 LD_LIBRARY_PATH=build/aravis/usr/lib/x86_64-linux-gnu /usr/bin/python3 tests/gige/check_formulas.py
else
    echo 'SKIP: formula execution requires the local Aravis runtime'
fi
mkdir -p build/tests
cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -Igige tests/gige/test_fault_evidence.c -o build/tests/test_fault_evidence
ASAN_OPTIONS=detect_leaks=0 build/tests/test_fault_evidence
cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -Igige tests/gige/test_protocol.c gige/GigeVision.c gige/GigeImage.c -o build/tests/test_gige
ASAN_OPTIONS=detect_leaks=0 build/tests/test_gige

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -Igige -I. tests/gige/test_sensor_features.c gige/GigeSensorFeatures.c gige/GigePreset.c gige/GigeImage.c Ov5640.c -o build/tests/test_sensor_features
ASAN_OPTIONS=detect_leaks=0 build/tests/test_sensor_features
cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Igige \
    tests/gige/test_image.c gige/GigeImage.c -o build/tests/test_image
ASAN_OPTIONS=detect_leaks=0 build/tests/test_image
cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Igige \
    -DMAX_CAMERA_GIGE_CSI_BYTES=1 tests/gige/test_native_image.c gige/GigeVision.c gige/GigeImage.c -o build/tests/test_native_image
ASAN_OPTIONS=detect_leaks=0 build/tests/test_native_image
cc -std=c11 -Wall -Wextra -Werror -Wno-misleading-indentation -g -fsanitize=address,undefined -Igige \
    -DMAX_CAMERA_GIGE_CSI_BYTES=1 tests/gige/test_resend.c gige/GigeVision.c gige/GigeImage.c -o build/tests/test_resend
ASAN_OPTIONS=detect_leaks=0 build/tests/test_resend
cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Igige \
    tests/gige/test_device_config.c gige/GigeVision.c gige/GigeImage.c -o build/tests/test_device_config
ASAN_OPTIONS=detect_leaks=0 build/tests/test_device_config
cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Igige \
    tests/gige/test_boundaries.c gige/GigeVision.c gige/GigeImage.c -o build/tests/test_boundaries
ASAN_OPTIONS=detect_leaks=0 build/tests/test_boundaries

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Igige \
    -DMAX_CAMERA_GIGE_CSI_BYTES=1 tests/gige/test_timestamp.c gige/GigeVision.c gige/GigeImage.c -o build/tests/test_timestamp
ASAN_OPTIONS=detect_leaks=0 build/tests/test_timestamp
