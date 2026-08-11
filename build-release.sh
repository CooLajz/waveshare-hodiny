#!/bin/zsh
set -euo pipefail

ROOT_DIR="${0:A:h}"
BUILD_PATH="$ROOT_DIR/.arduino/build-waveshare-hodiny-release"
VERSION="${1:?Pouziti: ./build-release.sh SEMVER}"
OUTPUT_DIR="$ROOT_DIR/build/waveshare-hodiny-release/$VERSION"
ARDUINO_CONFIG_FILE="${ARDUINO_CONFIG_FILE:-$ROOT_DIR/WaveshareHodiny/local/arduino-cli.yaml}"
if [[ ! -f "$ARDUINO_CONFIG_FILE" ]]; then
  ARDUINO_CONFIG_FILE="$ROOT_DIR/arduino-cli.yaml"
fi
ARDUINO_DATA_DIR=$(/opt/homebrew/bin/arduino-cli \
  --config-file "$ARDUINO_CONFIG_FILE" config dump --format json \
  | /usr/bin/python3 -c 'import json,sys; print(json.load(sys.stdin)["config"]["directories"]["data"])')
ESP32_CORE_VERSION="3.0.2"

mkdir -p "$OUTPUT_DIR"

/usr/bin/python3 "$ROOT_DIR/tools/generate_secrets.py" --release
/usr/bin/python3 "$ROOT_DIR/tools/generate_release_version.py" "$VERSION"

/opt/homebrew/bin/arduino-cli \
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

/usr/bin/python3 "$ROOT_DIR/tools/prepare_release_package.py" \
  "$VERSION" \
  "$BUILD_PATH/WaveshareHodiny.ino.bootloader.bin" \
  "$BUILD_PATH/WaveshareHodiny.ino.partitions.bin" \
  "$ARDUINO_DATA_DIR/packages/esp32/hardware/esp32/$ESP32_CORE_VERSION/tools/partitions/boot_app0.bin" \
  "$BUILD_PATH/WaveshareHodiny.ino.bin" \
  "$OUTPUT_DIR/WaveshareHodiny.ino.bin" \
  "$OUTPUT_DIR/package"

print "RELEASE_VERSION=$VERSION"
print "RELEASE_BUILD_DIR=$OUTPUT_DIR"
print "RELEASE_PACKAGE_DIR=$OUTPUT_DIR/package"
