#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
MBEDTLS_DIR="${1:?Pass the path to a host-built mbedTLS 3.x source tree}"
TEST_DIR="$(mktemp -d "${TMPDIR:-/tmp}/waveshare-backup-tests.XXXXXX")"
trap 'rm -rf "$TEST_DIR"' EXIT
"${CXX:-clang++}" -std=c++17 -O1 -g -fsanitize=address,undefined \
  -DSETTINGS_STORE_HOST_TEST -I"$ROOT_DIR/tools/backup_test_support" \
  -I"$MBEDTLS_DIR/include" \
  "$ROOT_DIR/tools/test_configuration_backup.cpp" \
  "$ROOT_DIR/WaveshareHodiny/ConfigurationBackup.cpp" \
  "$ROOT_DIR/WaveshareHodiny/SettingsStore.cpp" \
  "$ROOT_DIR/WaveshareHodiny/ClockConfig.cpp" \
  "$MBEDTLS_DIR/library/libmbedcrypto.a" -o "$TEST_DIR/test-backup"
"$TEST_DIR/test-backup"
