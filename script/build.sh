#!/usr/bin/env bash
set -Eeuo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

export CC=clang CXX=clang++

if command -v ninja >/dev/null 2>&1; then
    CMAKE_GENERATOR="Ninja"
else
    CMAKE_GENERATOR="Unix Makefiles"
fi

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -G "${CMAKE_GENERATOR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" -j "$(nproc)" --target csv2iyx run_query
