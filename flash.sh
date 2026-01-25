#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

PORT="${AVR_PORT:-/dev/ttyUSB0}"
PART="${AVR_PART:-t202}"

bold='\e[1m'; cyan='\e[36m'; red='\e[31m'; reset='\e[0m'

usage() {
  echo -e "${bold}Usage:${reset} ./flash.sh <nfc_harness> [hex-path]"
  echo "  - Hex defaults to build/<target>/<target>.hex"
  echo "  - Build first with: ./build.sh"
  echo "  - Override port via AVR_PORT=/dev/ttyUSB1"
  echo "  - Override part via AVR_PART=t202"
}

if [[ $# -lt 1 || "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  if [[ $# -lt 1 ]]; then
    exit 1
  else
    exit 0
  fi
fi

TARGET="$1"
shift
HEX="${1:-build/${TARGET}/${TARGET}.hex}"

if [[ ! -f "$HEX" ]]; then
  echo -e "${red}Firmware image '$HEX' not found.${reset} Build it first (e.g. ./build.sh)." >&2
  exit 1
fi

echo -e "${bold}${cyan}Flashing:${reset} '$HEX'"
echo -e "${bold}Target:${reset} ${TARGET}"
echo -e "${bold}Port:${reset} ${PORT}"
echo -e "${bold}Part:${reset} ${PART}"
echo

echo "Make sure:"
echo "  • Programmer + target share GND"
echo "  • UPDI is connected (often via ~1 kΩ)"
echo "  • Target is powered (3.3–5 V)"
echo

set -x
avrdude -p "$PART" -c serialupdi -P "$PORT" -b 115200 -U "flash:w:$HEX"
