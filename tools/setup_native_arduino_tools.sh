#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
CACHE="${NATIVE_TOOLS_DIR:-$ROOT_DIR/.arduino/native-tools}"
PYTHON_BIN="${PYTHON_BIN:-/Library/Developer/CommandLineTools/usr/bin/python3}"
CTAGS_COMMIT=abc8fca7499f44c725122881cd380a88c37abe0e
# Keep the compiler orchestrator identical for development, release and upload.
ARDUINO_CLI_VERSION=1.3.1
ARDUINO_CLI_SHA256=723f8ddaf4875e87a20380c88c15ff9d55916c3c80cb432f1f980ea32cec2341
PYTHON_TAG="$("$PYTHON_BIN" -c 'import sys; print("py%d%d" % sys.version_info[:2])')"
ESPTOOL_DIR="$CACHE/esptool-$PYTHON_TAG"
mkdir -p "$CACHE"
CLI_DIR="$CACHE/arduino-cli"
if [[ ! -x "$CLI_DIR/arduino-cli" ]] || [[ "$("$CLI_DIR/arduino-cli" version)" != *"Version: $ARDUINO_CLI_VERSION "* ]]; then
  CLI_STAGE_DIR="$(mktemp -d "$CACHE/arduino-cli-stage.XXXXXX")"
  trap 'rm -rf "$CLI_STAGE_DIR"' EXIT
  CLI_ARCHIVE="$CLI_STAGE_DIR/arduino-cli.tar.gz"
  curl -fsSL --retry 2 --connect-timeout 15 --max-time 120 \
    "https://github.com/arduino/arduino-cli/releases/download/v$ARDUINO_CLI_VERSION/arduino-cli_${ARDUINO_CLI_VERSION}_macOS_ARM64.tar.gz" \
    -o "$CLI_ARCHIVE"
  printf '%s  %s\n' "$ARDUINO_CLI_SHA256" "$CLI_ARCHIVE" | shasum -a 256 -c -
  tar -xzf "$CLI_ARCHIVE" -C "$CLI_STAGE_DIR" arduino-cli
  [[ "$("$CLI_STAGE_DIR/arduino-cli" version)" == *"Version: $ARDUINO_CLI_VERSION "* ]]
  mkdir -p "$CLI_DIR"
  mv "$CLI_STAGE_DIR/arduino-cli" "$CLI_DIR/arduino-cli"
fi
if [[ ! -x "$ESPTOOL_DIR/bin/esptool.py" ]]; then
  "$PYTHON_BIN" -m pip install --disable-pip-version-check --target "$ESPTOOL_DIR" 'esptool==4.6'
fi
if [[ ! -x "$CACHE/ctags/ctags" ]]; then
  SOURCE="$CACHE/ctags-source"
  if [[ ! -d "$SOURCE/.git" ]]; then
    /Library/Developer/CommandLineTools/usr/bin/git clone --depth 1 --branch 5.8-arduino11 https://github.com/arduino/ctags.git "$SOURCE"
  fi
  [[ "$(/Library/Developer/CommandLineTools/usr/bin/git -C "$SOURCE" rev-parse HEAD)" == "$CTAGS_COMMIT" ]]
  # Old Arduino ctags defines a reserved name also used by modern Apple headers.
  perl -pi -e 's/\b__unused__\b/CTAGS_UNUSED/g' "$SOURCE"/*.c "$SOURCE"/*.h
  (
    cd "$SOURCE"
    export DEVELOPER_DIR=/Library/Developer/CommandLineTools
    export SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
    CC=/Library/Developer/CommandLineTools/usr/bin/clang CFLAGS="-O2 -isysroot $SDKROOT" ./configure
    make -j4
  )
  mkdir -p "$CACHE/ctags"
  cp "$SOURCE/ctags" "$CACHE/ctags/ctags"
fi
if [[ "${1:-}" != "--quiet" ]]; then
  printf 'Nativní Arduino nástroje jsou připravené v .arduino/native-tools.\n'
fi
