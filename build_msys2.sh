#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${MSYSTEM:-}" ]]; then
  echo "Run this script from an MSYS2 MINGW64/UCRT64 shell."
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${1:-$ROOT_DIR/build_cmd}"
GENERATOR="${GENERATOR:-Ninja}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

cpu_count() {
  if [[ -n "${NUMBER_OF_PROCESSORS:-}" ]]; then
    echo "$NUMBER_OF_PROCESSORS"
  elif command -v nproc >/dev/null 2>&1; then
    nproc
  else
    echo 8
  fi
}

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G "$GENERATOR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR" -j "$(cpu_count)"
