#!/bin/zsh
set -euo pipefail

ROOT_DIR="${0:A:h}"
BUILD_PATH="$ROOT_DIR/.arduino/build-waveshare-hodiny-develop"
OUTPUT_DIR="$ROOT_DIR/build/waveshare-hodiny-develop"
ARDUINO_CONFIG_FILE="${ARDUINO_CONFIG_FILE:-$ROOT_DIR/WaveshareHodiny/local/arduino-cli.yaml}"
if [[ ! -f "$ARDUINO_CONFIG_FILE" ]]; then
  ARDUINO_CONFIG_FILE="$ROOT_DIR/arduino-cli.yaml"
fi
/usr/bin/python3 "$ROOT_DIR/tools/generate_secrets.py"
/opt/homebrew/bin/arduino-cli \
  --config-file "$ARDUINO_CONFIG_FILE" \
  compile \
  --fqbn esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=custom,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=default \
  --build-property 'compiler.c.extra_flags=-MMD -c -DLV_CONF_PATH=ClockLvglConfig.h' \
  --build-property 'compiler.cpp.extra_flags=-MMD -c -DLV_CONF_PATH=ClockLvglConfig.h -DWAVESHARE_DEVELOPMENT_BUILD=1' \
  --build-property 'upload.maximum_size=6291456' \
  --build-path "$BUILD_PATH" \
  --output-dir "$OUTPUT_DIR" \
  "$ROOT_DIR/WaveshareHodiny"
