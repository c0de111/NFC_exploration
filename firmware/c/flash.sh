#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

ELF="${1:-$BUILD_DIR/nfc_harness.elf}"
OPENOCD_SPEED="${OPENOCD_SPEED:-5000}"

usage() {
  cat <<'EOF'
Usage: ./flash.sh [path/to/nfc_harness.elf]

Flashes the firmware via SWD using OpenOCD + a CMSIS-DAP probe.

Environment:
  OPENOCD_SPEED   SWD speed in kHz (default: 5000)
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

if [[ ! -f "$ELF" ]]; then
  echo "ELF not found: $ELF" >&2
  echo "Build first: ./build.sh" >&2
  exit 1
fi

openocd --debug=0 \
  -f interface/cmsis-dap.cfg \
  -f target/rp2040.cfg \
  -c "adapter speed ${OPENOCD_SPEED}" \
  -c "program ${ELF} verify reset exit"

