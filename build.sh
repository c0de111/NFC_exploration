#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

targets=(nfc_harness)

for target in "${targets[@]}"; do
  make APP="$target" "$@"
done
