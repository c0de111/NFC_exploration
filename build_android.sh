#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEFAULT_SDK_DIR="/home/nicolas/Android/Sdk"

has_sdk_override=0
for ((i=1; i<=$#; i++)); do
  arg="${!i}"
  if [[ "$arg" == "-h" || "$arg" == "--help" ]]; then
    exec "${ROOT_DIR}/android/scripts/build_and_sync_apk.sh" --help
  fi
  if [[ "$arg" == "--sdk-dir" ]]; then
    has_sdk_override=1
    break
  fi
done

if [[ "$has_sdk_override" -eq 0 && ! -d "$DEFAULT_SDK_DIR" ]]; then
  cat >&2 <<EOF
Default Android SDK path not found: $DEFAULT_SDK_DIR

Either install the SDK there or pass a path explicitly:
  ./build_android.sh --sdk-dir /path/to/Android/Sdk
EOF
  exit 1
fi

# Default workflow: build tap app and copy APK into ~/Sync for sideload.
exec "${ROOT_DIR}/android/scripts/build_and_sync_apk.sh" --app tap --sdk-dir "${DEFAULT_SDK_DIR}" --sync-dir "${HOME}/Sync" "$@"
