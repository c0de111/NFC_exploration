#!/usr/bin/env bash
set -euo pipefail

export PICO_SDK_PATH="${PICO_SDK_PATH:-$HOME/pico/pico-sdk}"

# Work around pico-sdk 2.1.0 + GCC 15 host build failure (pioasm lacks <cstdint>)
# This patches the SDK headers in place, idempotently, so fresh clones/build dirs work.
patch_pioasm_headers() {
  local sdk="$PICO_SDK_PATH"
  local files=(
    "$sdk/tools/pioasm/pio_types.h"
    "$sdk/tools/pioasm/output_format.h"
  )
  for f in "${files[@]}"; do
    [[ -f "$f" ]] || continue
    if ! grep -q '<cstdint>' "$f"; then
      echo "Patching $f to include <cstdint> (fixes GCC 15 build of pioasm)..."
      perl -0777 -i -pe 's/(#include[^\n]*\n)(?=(\n|struct))/\1#include <cstdint>\n/ if ! /<cstdint>/' "$f"
    fi
  done
}

usage() {
  cat <<'EOF'
Usage: ./build.sh [--board <pico|pico_w>] [--clean]

Builds the RP2040/Pico NFC harness firmware using the Pico SDK.

Environment:
  PICO_SDK_PATH   Path to pico-sdk (default: $HOME/pico/pico-sdk)
  PICO_BOARD      Default board if --board is not provided (default: pico)
EOF
}

BOARD="${PICO_BOARD:-pico}"
CLEAN=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --board)
      [[ $# -ge 2 ]] || { echo "Missing value for --board" >&2; exit 2; }
      BOARD="$2"
      shift 2
      ;;
    --clean)
      CLEAN=true
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

patch_pioasm_headers

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

if $CLEAN; then
  rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "PICO_SDK_PATH=$PICO_SDK_PATH"
echo "PICO_BOARD=$BOARD"

cmake -DPICO_BOARD="$BOARD" "$SCRIPT_DIR"
make -j"$(nproc 2>/dev/null || echo 4)"

echo
echo "Build outputs:"
ls -1 nfc_harness.* 2>/dev/null || true

