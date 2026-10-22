#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
json_dir=".pio/libdeps/esp32s3/ArduinoJson/src"
if [[ ! -d "$json_dir" ]]; then
  echo "Falta ArduinoJson. Ejecuta primero: pio pkg install -e esp32s3" >&2
  exit 1
fi
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -Iinclude -I"$json_dir" src/PetCore.cpp src/FaceRenderer.cpp test/core/test_main.cpp \
  -o build/test_core
./build/test_core "$@"
python3 tools/test_timezone.py
