#pragma once
#include <Arduino.h>
#include "SettingsStore.h"

constexpr size_t BACKUP_FILE_CAPACITY = 10000;
constexpr size_t BACKUP_PASSWORD_CAPACITY = 257;
constexpr unsigned BACKUP_KDF_ITERATIONS = 10000;

struct BackupMetadata {
  char firmware[64] = {};
  uint32_t configSchema = 0;
  uint64_t createdAt = 0;
  unsigned kdfIterations = BACKUP_KDF_ITERATIONS;
};

bool backupPasswordValid(const char *password);
// Run these CPU-bound functions on the backup worker, not the UI task.
bool backupEncrypt(const uint8_t *plain, size_t length, const char *password,
                   const BackupMetadata &metadata, char *file, size_t capacity);
bool backupDecrypt(const char *file, size_t length, const char *password,
                   BackupMetadata &metadata, uint8_t *plain, size_t capacity,
                   size_t &plainLength);
