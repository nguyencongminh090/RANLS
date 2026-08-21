#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${1:-$ROOT_DIR/build_cmd}"
GENERATOR="${GENERATOR:-Ninja}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

cpu_count() {
  if command -v nproc >/dev/null 2>&1; then
    nproc
  elif command -v sysctl >/dev/null 2>&1; then
    sysctl -n hw.ncpu
  else
    echo 4
  fi
}

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G "$GENERATOR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR" -j "$(cpu_count)"

# Set RUN_TESTS=1 to also run the model/engine unit tests (rapfi-gui-tests via
# ctest) after building. Off by default so a plain ./build.sh keeps building
# only the application, matching prior behavior.
if [[ "${RUN_TESTS:-0}" == "1" ]]; then
  ctest --test-dir "$BUILD_DIR" --output-on-failure
fi
