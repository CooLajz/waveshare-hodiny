#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_DIR="$(mktemp -d "${TMPDIR:-/tmp}/waveshare-wifi-tests.XXXXXX")"
trap 'rm -rf "$TEST_DIR"' EXIT
"${CXX:-clang++}" -std=c++17 -O1 -g -fsanitize=address,undefined \
  -DFIRMWARE_RELEASE=1 -I"$ROOT_DIR/tools/wifi_test_support" \
  -I"$ROOT_DIR/tools/backup_test_support" \
  "$ROOT_DIR/tools/test_wifi_provisioning.cpp" \
  "$ROOT_DIR/WaveshareHodiny/WifiProvisioning.cpp" -o "$TEST_DIR/test-wifi"
for scenario in late-router scan improv-success improv-failure promotion-failure portal-save empty; do
  "$TEST_DIR/test-wifi" "$scenario"
done
