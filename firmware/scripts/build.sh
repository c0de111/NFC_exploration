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
  PICO_BOARD      Default board if --board is not provided (default: pico_w)
  NFC_AUTO_POWER_OFF_MS  Auto power-off timeout passed to CMake (default: 10000)
  --clean         Remove firmware/build and exit (no configure/build)
EOF
}

BOARD="${PICO_BOARD:-pico_w}"
AUTO_POWER_OFF_MS="${NFC_AUTO_POWER_OFF_MS:-10000}"
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

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

if $CLEAN; then
  rm -rf "$BUILD_DIR"
  echo "Removed $BUILD_DIR"
  exit 0
fi

patch_pioasm_headers

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "PICO_SDK_PATH=$PICO_SDK_PATH"
echo "PICO_BOARD=$BOARD"
echo "NFC_AUTO_POWER_OFF_MS=$AUTO_POWER_OFF_MS"

cmake -DPICO_BOARD="$BOARD" -DNFC_AUTO_POWER_OFF_MS="$AUTO_POWER_OFF_MS" "$ROOT_DIR"
make -j"$(nproc 2>/dev/null || echo 4)"

echo
echo "Build outputs:"
ls -1 nfc_harness.* 2>/dev/null || true

# Picotool-free UF2 generation using uf2conv.py (downloaded/cached locally if missing).
UF2_TOOL="$ROOT_DIR/scripts/uf2conv.py"
UF2_FAMILIES="$ROOT_DIR/scripts/uf2families.json"
ensure_uf2_tool() {
  if [[ -f "$UF2_TOOL" && -f "$UF2_FAMILIES" ]]; then
    return
  fi
  echo "Fetching uf2conv.py + uf2families.json (picotool-free UF2 conversion)..."
  mkdir -p "$(dirname "$UF2_TOOL")"
  curl -fsSL -o "$UF2_TOOL" https://raw.githubusercontent.com/microsoft/uf2/master/utils/uf2conv.py
  curl -fsSL -o "$UF2_FAMILIES" https://raw.githubusercontent.com/microsoft/uf2/master/utils/uf2families.json
  chmod +x "$UF2_TOOL"
}

if [[ -f "nfc_harness.bin" ]]; then
  if ensure_uf2_tool; then
    echo "Converting BIN to UF2 via uf2conv.py..."
    python3 "$UF2_TOOL" --base 0x10000000 --family 0xe48bff56 \
      --output nfc_harness.uf2 nfc_harness.bin || echo "UF2 conversion failed"
  fi
else
  echo "UF2 not generated (nfc_harness.bin missing)."
fi
