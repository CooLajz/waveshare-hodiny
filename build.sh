#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
WIFI_PROFILE="${1:-home}"
if [[ "$WIFI_PROFILE" != "home" && "$WIFI_PROFILE" != "work" ]]; then
  echo "Použití: ./build.sh [home|work]" >&2
  exit 1
fi
BUILD_PATH="$ROOT_DIR/.arduino/build-waveshare-hodiny-develop"
OUTPUT_DIR="$ROOT_DIR/build/waveshare-hodiny-develop"
source "$ROOT_DIR/tools/arduino_toolchain.sh"
if [[ -z "$ARDUINO_CLI_BIN" || -z "$PYTHON_BIN" ]]; then
  echo "Chybí arduino-cli nebo python3 v PATH." >&2
  exit 1
fi
if [[ "$WIFI_PROFILE" == "work" ]]; then
  "$PYTHON_BIN" "$ROOT_DIR/tools/generate_secrets.py" --work-wifi
else
  "$PYTHON_BIN" "$ROOT_DIR/tools/generate_secrets.py"
fi
"$PYTHON_BIN" "$ROOT_DIR/tools/validate_weather_icon_parity.py"
"$PYTHON_BIN" "$ROOT_DIR/tools/generate_web_assets.py"
"$ARDUINO_CLI_BIN" \
  --config-file "$ARDUINO_CONFIG_FILE" \
  compile \
  "${ARDUINO_TOOL_PROPERTIES[@]}" \
  --fqbn "$ARDUINO_DEVELOPMENT_FQBN" \
  --build-property 'compiler.c.extra_flags=-MMD -c -DLV_CONF_PATH=ClockLvglConfig.h' \
  --build-property 'compiler.cpp.extra_flags=-MMD -c -DLV_CONF_PATH=ClockLvglConfig.h -DWAVESHARE_DEVELOPMENT_BUILD=1' \
  --build-property 'upload.maximum_size=6291456' \
  --build-path "$BUILD_PATH" \
  --output-dir "$OUTPUT_DIR" \
  "$ROOT_DIR/WaveshareHodiny"
