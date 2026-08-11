#!/bin/zsh
set -euo pipefail

ROOT_DIR="${0:A:h}"
/usr/bin/python3 "$ROOT_DIR/tools/capture_screenshot.py" "$@"
