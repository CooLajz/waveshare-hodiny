#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_PATH="$ROOT_DIR/.arduino/build-waveshare-hodiny-release"
VERSION="${1:?Pouziti: ./build-release.sh SEMVER}"
RELEASE_CHANNEL="${RELEASE_CHANNEL:-internal}"
if [[ "$RELEASE_CHANNEL" != "internal" && "$RELEASE_CHANNEL" != "public" ]]; then
  echo "RELEASE_CHANNEL musí být internal nebo public." >&2
  exit 1
fi
OUTPUT_SUFFIX="$VERSION"
GENERATE_SECRETS_ARGS=(--release)
if [[ "$RELEASE_CHANNEL" == "public" ]]; then
  OUTPUT_SUFFIX="$VERSION-public"
  GENERATE_SECRETS_ARGS=(--release --public-release)
fi
OUTPUT_DIR="$ROOT_DIR/build/waveshare-hodiny-release/$OUTPUT_SUFFIX"
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
mkdir -p "$OUTPUT_DIR"

"$PYTHON_BIN" "$ROOT_DIR/tools/generate_secrets.py" "${GENERATE_SECRETS_ARGS[@]}"
"$PYTHON_BIN" "$ROOT_DIR/tools/generate_release_version.py" "$VERSION"
"$PYTHON_BIN" "$ROOT_DIR/tools/validate_weather_icon_parity.py"

"$ARDUINO_CLI_BIN" \
  --config-file "$ARDUINO_CONFIG_FILE" \
  compile --clean --verbose \
  --fqbn esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=custom,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc \
  --build-property 'compiler.c.extra_flags=-MMD -c -DLV_CONF_PATH=ClockLvglConfig.h' \
  --build-property 'compiler.cpp.extra_flags=-MMD -c -DLV_CONF_PATH=ClockLvglConfig.h -DFIRMWARE_RELEASE=1' \
  --build-property 'upload.maximum_size=6291456' \
  --build-path "$BUILD_PATH" \
  --output-dir "$OUTPUT_DIR" \
  "$ROOT_DIR/WaveshareHodiny" 2>&1 \
  | tee "$OUTPUT_DIR/build.log" \
  | awk '/^FQBN:|Sketch uses|Global variables|Creating esp32s3 image|Successfully created|merge_bin|error:|Error during build/ { print; fflush() }'

ESP32_PLATFORM_DIR=$(sed -n "s/^Using core 'esp32' from platform in folder: //p" \
  "$OUTPUT_DIR/build.log" | head -n 1)
BOOT_APP0_BIN="$ESP32_PLATFORM_DIR/tools/partitions/boot_app0.bin"
if [[ -z "$ESP32_PLATFORM_DIR" || ! -f "$BOOT_APP0_BIN" ]]; then
  echo "Z verbose výstupu buildu se nepodařilo určit boot_app0.bin použité ESP32 platformy." >&2
  exit 1
fi

PACKAGE_COMMAND=(
  "$PYTHON_BIN" "$ROOT_DIR/tools/prepare_release_package.py"
  "$VERSION"
  "$BUILD_PATH/WaveshareHodiny.ino.bootloader.bin"
  "$BUILD_PATH/WaveshareHodiny.ino.partitions.bin"
  "$BOOT_APP0_BIN"
  "$BUILD_PATH/WaveshareHodiny.ino.bin"
  "$OUTPUT_DIR/WaveshareHodiny.ino.bin"
  "$OUTPUT_DIR/package"
)
if [[ "$RELEASE_CHANNEL" == "public" ]]; then
  PACKAGE_COMMAND+=(/waveshare-hodiny/firmware)
fi
"${PACKAGE_COMMAND[@]}"

printf 'RELEASE_VERSION=%s\n' "$VERSION"
printf 'RELEASE_CHANNEL=%s\n' "$RELEASE_CHANNEL"
printf 'RELEASE_BUILD_DIR=%s\n' "$OUTPUT_DIR"
printf 'RELEASE_PACKAGE_DIR=%s\n' "$OUTPUT_DIR/package"
