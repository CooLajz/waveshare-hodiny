#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_PATH="$ROOT_DIR/.arduino/build-waveshare-hodiny-develop"
OUTPUT_DIR="$ROOT_DIR/build/waveshare-hodiny-develop"
ARDUINO_CLI_BIN="${ARDUINO_CLI_BIN:-$(command -v arduino-cli || true)}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || true)}"
if [[ -z "$ARDUINO_CLI_BIN" || -z "$PYTHON_BIN" ]]; then
  echo "Chybí arduino-cli nebo python3 v PATH." >&2
  exit 1
fi
ARDUINO_CONFIG_FILE="${ARDUINO_CONFIG_FILE:-$ROOT_DIR/WaveshareHodiny/local/arduino-cli.yaml}"
if [[ ! -f "$ARDUINO_CONFIG_FILE" ]]; then
  ARDUINO_CONFIG_FILE="$ROOT_DIR/arduino-cli.yaml"
fi
"$PYTHON_BIN" "$ROOT_DIR/tools/generate_secrets.py"
"$PYTHON_BIN" "$ROOT_DIR/tools/validate_weather_icon_parity.py"
"$ARDUINO_CLI_BIN" \
  --config-file "$ARDUINO_CONFIG_FILE" \
  compile \
  --fqbn esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=custom,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=default \
  --build-property 'compiler.c.extra_flags=-MMD -c -DLV_CONF_PATH=ClockLvglConfig.h' \
  --build-property 'compiler.cpp.extra_flags=-MMD -c -DLV_CONF_PATH=ClockLvglConfig.h -DWAVESHARE_DEVELOPMENT_BUILD=1' \
  --build-property 'upload.maximum_size=6291456' \
  --build-path "$BUILD_PATH" \
  --output-dir "$OUTPUT_DIR" \
  "$ROOT_DIR/WaveshareHodiny"
