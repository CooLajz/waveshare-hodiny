#!/bin/zsh
set -euo pipefail

ROOT_DIR="${0:A:h}"
BUILD_PATH="$ROOT_DIR/.arduino/build-waveshare-hodiny-develop"
ARDUINO_CONFIG_FILE="${ARDUINO_CONFIG_FILE:-$ROOT_DIR/WaveshareHodiny/local/arduino-cli.yaml}"
if [[ ! -f "$ARDUINO_CONFIG_FILE" ]]; then
  ARDUINO_CONFIG_FILE="$ROOT_DIR/arduino-cli.yaml"
fi
PORT="${1:-${(j:\n:)$(ls /dev/cu.usbmodem* 2>/dev/null)}}"
if [[ -z "$PORT" || "$PORT" == *$'\n'* ]]; then
  print -u2 "Nelze jednoznačně vybrat displej. Předejte port jako první argument."
  exit 1
fi

/opt/homebrew/bin/arduino-cli \
  --config-file "$ARDUINO_CONFIG_FILE" \
  upload --fqbn esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=custom,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=default \
  --build-path "$BUILD_PATH" \
  --port "$PORT" \
  "$ROOT_DIR/WaveshareHodiny"
