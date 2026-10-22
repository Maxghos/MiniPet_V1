#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build docs
"${CXX:-g++}" -std=c++17 -O2 -Wall -Wextra -Werror -Iinclude \
  src/FaceRenderer.cpp tools/render_faces.cpp -o build/render_faces
./build/render_faces
