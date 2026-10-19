#!/usr/bin/env bash
# Shared by development, release and upload; ROOT_DIR is set by the caller.
ARDUINO_CONFIG_FILE="${ARDUINO_CONFIG_FILE:-$ROOT_DIR/WaveshareHodiny/local/arduino-cli.yaml}"
if [[ ! -f "$ARDUINO_CONFIG_FILE" ]]; then
  ARDUINO_CONFIG_FILE="$ROOT_DIR/arduino-cli.yaml"
fi
ARDUINO_BOARD_FQBN='esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=custom,PSRAM=opi,USBMode=hwcdc'
ARDUINO_DEVELOPMENT_FQBN="$ARDUINO_BOARD_FQBN,CDCOnBoot=default"
ARDUINO_RELEASE_FQBN="$ARDUINO_BOARD_FQBN,CDCOnBoot=cdc"
ARDUINO_TOOL_PROPERTIES=()
ARDUINO_UPLOAD_PROPERTIES=()
if [[ "$(uname -s)" == Darwin && "$(uname -m)" == arm64 ]]; then
  PYTHON_BIN="${PYTHON_BIN:-/Library/Developer/CommandLineTools/usr/bin/python3}"
  NATIVE_TOOLS_DIR="${NATIVE_TOOLS_DIR:-$ROOT_DIR/.arduino/native-tools}"
  PYTHON_TAG="$("$PYTHON_BIN" -c 'import sys; print("py%d%d" % sys.version_info[:2])')"
  ESPTOOL_DIR="$NATIVE_TOOLS_DIR/esptool-$PYTHON_TAG"
  NATIVE_TOOLS_DIR="$NATIVE_TOOLS_DIR" PYTHON_BIN="$PYTHON_BIN" bash "$ROOT_DIR/tools/setup_native_arduino_tools.sh" --quiet
  ARDUINO_CLI_BIN="${ARDUINO_CLI_BIN:-$NATIVE_TOOLS_DIR/arduino-cli/arduino-cli}"
  export PYTHONPATH="$ESPTOOL_DIR${PYTHONPATH:+:$PYTHONPATH}"
  "$PYTHON_BIN" -c 'import esptool; assert esptool.__version__ == "4.6"'
  "$NATIVE_TOOLS_DIR/ctags/ctags" --version >/dev/null
  ARDUINO_TOOL_PROPERTIES=(
    --build-property "tools.esptool_py.path=$ESPTOOL_DIR/bin"
    --build-property tools.esptool_py.cmd=esptool.py
    --build-property "runtime.tools.ctags.path=$NATIVE_TOOLS_DIR/ctags"
    --build-property "tools.ctags.path=$NATIVE_TOOLS_DIR/ctags"
  )
  ARDUINO_UPLOAD_PROPERTIES=(
    --upload-property "tools.esptool_py.path=$ESPTOOL_DIR/bin"
    --upload-property tools.esptool_py.cmd=esptool.py
  )
else
  PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || true)}"
  ARDUINO_CLI_BIN="${ARDUINO_CLI_BIN:-$(command -v arduino-cli || true)}"
fi
