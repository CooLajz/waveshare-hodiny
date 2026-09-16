#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
CACHE="${NATIVE_TOOLS_DIR:-$ROOT_DIR/.arduino/native-tools}"
PYTHON_BIN="${PYTHON_BIN:-/Library/Developer/CommandLineTools/usr/bin/python3}"
CTAGS_COMMIT=abc8fca7499f44c725122881cd380a88c37abe0e
PYTHON_TAG="$("$PYTHON_BIN" -c 'import sys; print("py%d%d" % sys.version_info[:2])')"
ESPTOOL_DIR="$CACHE/esptool-$PYTHON_TAG"
mkdir -p "$CACHE"
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
printf 'Nativní Arduino nástroje jsou připravené v .arduino/native-tools.\n'
