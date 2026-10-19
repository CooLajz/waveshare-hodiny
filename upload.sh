#!/bin/zsh
set -euo pipefail

ROOT_DIR="${0:A:h}"
source "$ROOT_DIR/tools/arduino_toolchain.sh"
BUILD_PATH="$ROOT_DIR/.arduino/build-waveshare-hodiny-develop"
PORT="${1:-${(j:\n:)$(ls /dev/cu.usbmodem* 2>/dev/null)}}"
if [[ -z "$PORT" || "$PORT" == *$'\n'* ]]; then
  print -u2 "Nelze jednoznačně vybrat displej. Předejte port jako první argument."
  exit 1
fi

"$ARDUINO_CLI_BIN" \
  --config-file "$ARDUINO_CONFIG_FILE" \
  upload --fqbn "$ARDUINO_DEVELOPMENT_FQBN" \
  "${ARDUINO_UPLOAD_PROPERTIES[@]}" \
  --build-path "$BUILD_PATH" \
  --port "$PORT" \
  "$ROOT_DIR/WaveshareHodiny"
