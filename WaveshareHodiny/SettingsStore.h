#pragma once

#include <Arduino.h>

// Only application settings live here. Wi-Fi, OTA and device identity stay in
// their original partitions. Key IDs and value types are a versioned contract.
constexpr size_t SETTINGS_IMAGE_CAPACITY = 6144;
constexpr uint32_t SETTINGS_IMAGE_VERSION = 1;

bool settingsStoreBegin();
bool settingsTransactionBegin();
bool settingsTransactionActive();
bool settingsTransactionCommit();
void settingsTransactionAbort();
bool settingsExport(uint8_t *output, size_t capacity, size_t &length);
// Stage a complete replacement, never merge it with the destination device.
bool settingsImport(const uint8_t *input, size_t length);

class SettingsPreferences {
 public:
  bool begin(const char *name, bool readOnly = false,
             const char *partition = "clockcfg");
  void end() {}
  size_t getBytesLength(const char *key);
  size_t getBytes(const char *key, void *output, size_t capacity);
  size_t putBytes(const char *key, const void *data, size_t length);
  uint8_t getUChar(const char *key, uint8_t fallback = 0);
  uint32_t getUInt(const char *key, uint32_t fallback = 0);
  float getFloat(const char *key, float fallback = 0);
  bool getBool(const char *key, bool fallback = false);
  String getString(const char *key, const char *fallback = "");
  size_t putUChar(const char *key, uint8_t value);
  size_t putUInt(const char *key, uint32_t value);
  size_t putFloat(const char *key, float value);
  size_t putBool(const char *key, bool value);
  size_t putString(const char *key, const String &value);
  bool remove(const char *key);

 private:
  const char *namespace_ = nullptr;
  bool readOnly_ = true;
};
