#include "SettingsStore.h"

#include <Preferences.h>
#include <esp_heap_caps.h>
#include <mbedtls/sha256.h>
#include <mbedtls/platform_util.h>
#include <cmath>

namespace {
enum Kind : uint8_t { Blob, Byte, UInt, Float, Text };
struct Key { const char *space; const char *name; Kind kind; uint16_t maximum; };
// Append only: encrypted backups use these stable IDs, not C++ struct layout.
const Key keys[] = {
    {"clock-config", "config", Blob, 4096},
    {"clock-look", "style", Byte, 1},
    {"clock-look", "tone", UInt, 4},
    {"clock-look", "hand-tone", UInt, 4},
    {"clock-look", "accent-color", UInt, 4},
    {"clock-look", "accents", Byte, 1},
    {"clock-look", "outline-hands", Byte, 1},
    {"clock-look", "mono-values", Byte, 1},
    {"clock-look", "values-above", Byte, 1},
    {"clock-look", "date-format", Byte, 1},
    {"clock-look", "date-color", UInt, 4},
    {"clock-look", "weather-color", UInt, 4},
    {"clock-look", "retroBarCount", Byte, 1},
    {"clock-look", "retroBarSrc", Byte, 1},
    {"clock-look", "retroBarMin", Float, 4},
    {"clock-look", "retroBarMax", Float, 4},
    {"clock-look", "retroLeft", Byte, 1},
    {"clock-look", "retroRight", Byte, 1},
    {"clock-look", "time12h", Byte, 1},
    {"clock-look", "retroWxRaster", Byte, 1},
    {"clock-look", "retroGhost", Byte, 1},
    {"clock-look", "retroBg", UInt, 4},
    {"clock-look", "retroFg", UInt, 4},
    {"clock-look", "retroDigitsA", Byte, 1},
    {"clock-look", "retroDigitsB", Byte, 1},
    {"clock-look", "screenSlide", Byte, 1},
    {"web-mode", "mode", Byte, 1},
    {"web-auth", "credential", Blob, 56},
    {"control-api", "secret", Text, 33},
    {"save-state", "receipt", Text, 33},
    {"clock-look", "retroFixedDay", Byte, 1},
    {"clock-look", "retroDateFmt", Byte, 1},
};
constexpr size_t KEY_COUNT = sizeof(keys) / sizeof(keys[0]);
struct Image {
  size_t length = 0;
  uint8_t data[SETTINGS_IMAGE_CAPACITY];
};
Image *committed = nullptr;
Image *working = nullptr;
bool initialized = false;
bool storageAccessible = false;
bool transaction = false;
bool transactionFailed = false;
uint8_t selectedSlot = 255;
constexpr uint8_t MAGIC[] = {'W', 'H', 'S', 1};
constexpr size_t DISK_HEADER = 4 + 32;

int keyId(const char *space, const char *name) {
  if (!space || !name) return -1;
  for (size_t i = 0; i < KEY_COUNT; ++i)
    if (!strcmp(space, keys[i].space) && !strcmp(name, keys[i].name)) return i;
  return -1;
}

bool validImage(const uint8_t *data, size_t length) {
  if (length > SETTINGS_IMAGE_CAPACITY) return false;
  bool seen[KEY_COUNT] = {};
  for (size_t pos = 0; pos < length;) {
    if (length - pos < 3) return false;
    const uint8_t id = data[pos];
    const size_t size = data[pos + 1] | (size_t(data[pos + 2]) << 8);
    pos += 3;
    if (id >= KEY_COUNT || seen[id] || !size || size > keys[id].maximum ||
        size > length - pos) return false;
    seen[id] = true;
    const Kind kind = keys[id].kind;
    if ((kind == Byte && size != 1) ||
        ((kind == UInt || kind == Float) && size != 4)) return false;
    if (kind == Text && (data[pos + size - 1] != 0 ||
                        memchr(data + pos, 0, size - 1))) return false;
    if (kind == UInt) {
      uint32_t color; memcpy(&color, data + pos, 4);
      if (color > 0xFFFFFF) return false;
    }
    if (kind == Float) {
      float value; memcpy(&value, data + pos, 4);
      if (!std::isfinite(value)) return false;
    }
    if (kind == Byte) {
      const uint8_t value = data[pos];
      switch (id) {
        case 1: case 26: if (value > 2) return false; break;
        case 9: if (value > 5) return false; break;
        case 31: if (value > 13) return false; break; // retroDateFmt
        case 12: if (value < 5 || value > 50) return false; break;
        case 13: case 16: case 17: if (value > 4) return false; break;
        case 20: if (value > 50) return false; break;
        case 23: case 24: if (value < 1 || value > 4) return false; break;
        default: if (value > 1) return false;
      }
    }
    pos += size;
  }
  return true;
}

const uint8_t *findValue(const Image &image, int id, size_t &size) {
  for (size_t pos = 0; pos < image.length;) {
    size = image.data[pos + 1] | (size_t(image.data[pos + 2]) << 8);
    if (image.data[pos] == id) return image.data + pos + 3;
    pos += 3 + size;
  }
  size = 0;
  return nullptr;
}

bool replaceValue(Image &image, int id, const void *value, size_t length) {
  if (id < 0 || length > keys[id].maximum) return false;
  size_t oldSize = 0;
  const uint8_t *old = findValue(image, id, oldSize);
  const size_t offset = old ? size_t(old - image.data) - 3 : image.length;
  const size_t removed = old ? oldSize + 3 : 0;
  const size_t added = length ? length + 3 : 0;
  if (image.length - removed + added > sizeof(image.data)) return false;
  memmove(image.data + offset + added, image.data + offset + removed,
          image.length - offset - removed);
  image.length = image.length - removed + added;
  if (length) {
    image.data[offset] = id;
    image.data[offset + 1] = length & 255;
    image.data[offset + 2] = length >> 8;
    memcpy(image.data + offset + 3, value, length);
  }
  return true;
}

bool loadSlot(Preferences &disk, uint8_t slot, Image &image) {
  const char *key = slot == 0 ? "slot0" : "slot1";
  const size_t size = disk.getBytesLength(key);
  if (size < DISK_HEADER || size > DISK_HEADER + SETTINGS_IMAGE_CAPACITY) return false;
  auto *buffer = static_cast<uint8_t *>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!buffer) return false;
  uint8_t digest[32];
  const bool valid = disk.getBytes(key, buffer, size) == size &&
      memcmp(buffer, MAGIC, sizeof(MAGIC)) == 0 &&
      mbedtls_sha256(buffer + DISK_HEADER, size - DISK_HEADER, digest, 0) == 0 &&
      memcmp(digest, buffer + 4, sizeof(digest)) == 0 &&
      validImage(buffer + DISK_HEADER, size - DISK_HEADER);
  if (valid) {
    image.length = size - DISK_HEADER;
    memcpy(image.data, buffer + DISK_HEADER, image.length);
  }
  mbedtls_platform_zeroize(buffer, size);
  free(buffer);
  return valid;
}

bool bootstrapLegacy() {
  committed->length = 0;
  for (size_t id = 0; id < KEY_COUNT; ++id) {
    Preferences old;
    if (!old.begin(keys[id].space, true, "clockcfg")) continue;
    if (!old.isKey(keys[id].name)) { old.end(); continue; }
    uint8_t value[4096] = {};
    size_t size = 0;
    switch (keys[id].kind) {
      case Blob:
        size = old.getBytesLength(keys[id].name);
        if (size > keys[id].maximum || old.getBytes(keys[id].name, value, sizeof(value)) != size) {
          old.end(); return false;
        }
        break;
      case Byte: value[0] = old.getUChar(keys[id].name); size = 1; break;
      case UInt: { const uint32_t v = old.getUInt(keys[id].name); memcpy(value, &v, 4); size = 4; break; }
      case Float: { const float v = old.getFloat(keys[id].name); memcpy(value, &v, 4); size = 4; break; }
      case Text: {
        const String v = old.getString(keys[id].name);
        size = v.length() + 1;
        if (size > keys[id].maximum) { old.end(); return false; }
        memcpy(value, v.c_str(), size);
        break;
      }
    }
    old.end();
    const bool ok = replaceValue(*committed, id, value, size);
    mbedtls_platform_zeroize(value, sizeof(value));
    if (!ok) return false;
  }
  return true;
}

size_t writeValue(int id, const void *value, size_t length) {
  if (!initialized || id < 0) return 0;
  const bool own = !transaction;
  if (own && !settingsTransactionBegin()) return 0;
  const bool ok = replaceValue(*working, id, value, length);
  if (!ok) transactionFailed = true;
  if (own) {
    if (!ok) { settingsTransactionAbort(); return 0; }
    if (!settingsTransactionCommit()) return 0;
  }
  return ok ? length : 0;
}
}  // namespace

bool settingsStoreBegin() {
  // An explicit replacement transaction may be recovering an invalid selected
  // slot. Its validated staging image is available to the normal decoders.
  if (initialized || transaction) return true;
  storageAccessible = false;
  if (!committed) committed = static_cast<Image *>(heap_caps_calloc(1, sizeof(Image), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!working) working = static_cast<Image *>(heap_caps_calloc(1, sizeof(Image), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!committed || !working) return false;
  Preferences disk;
  if (!disk.begin("settings-v1", false, "clockcfg")) return false;
  storageAccessible = true;
  selectedSlot = disk.getUChar("active", 255);
  // With a marker, never fall back to stale legacy values or an uncommitted slot.
  const bool ok = selectedSlot <= 1 ? loadSlot(disk, selectedSlot, *committed)
                                    : selectedSlot == 255 && bootstrapLegacy();
  disk.end();
  initialized = ok;
  return ok;
}

bool settingsTransactionBegin() {
  if (transaction || !settingsStoreBegin()) return false;
  *working = *committed;
  transaction = true;
  transactionFailed = false;
  return true;
}
bool settingsTransactionActive() { return transaction; }
void settingsTransactionAbort() {
  if (working) mbedtls_platform_zeroize(working, sizeof(*working));
  transaction = false;
  transactionFailed = false;
}

bool settingsTransactionCommit() {
  if (!transaction || transactionFailed || !validImage(working->data, working->length)) {
    settingsTransactionAbort(); return false;
  }
  const size_t size = DISK_HEADER + working->length;
  auto *buffer = static_cast<uint8_t *>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!buffer) { settingsTransactionAbort(); return false; }
  memcpy(buffer, MAGIC, sizeof(MAGIC));
  memcpy(buffer + DISK_HEADER, working->data, working->length);
  bool ok = mbedtls_sha256(working->data, working->length, buffer + 4, 0) == 0;
  Preferences disk;
  ok = ok && disk.begin("settings-v1", false, "clockcfg");
  const uint8_t next = selectedSlot == 0 ? 1 : 0;
  const char *key = next == 0 ? "slot0" : "slot1";
  if (ok) ok = disk.putBytes(key, buffer, size) == size;
  if (ok) {
    // Verify the inactive copy byte-for-byte before the atomic selector write.
    memset(buffer, 0, size);
    ok = disk.getBytes(key, buffer, size) == size &&
        memcmp(buffer + DISK_HEADER, working->data, working->length) == 0;
    uint8_t digest[32];
    ok = ok && memcmp(buffer, MAGIC, 4) == 0 &&
        mbedtls_sha256(working->data, working->length, digest, 0) == 0 &&
        memcmp(buffer + 4, digest, 32) == 0;
  }
  if (ok) {
    disk.putUChar("active", next);
    ok = disk.getUChar("active", 255) == next;
  }
  disk.end();
  mbedtls_platform_zeroize(buffer, size);
  free(buffer);
  if (ok) { *committed = *working; selectedSlot = next; initialized = true; }
  settingsTransactionAbort();
  return ok;
}

bool settingsExport(uint8_t *output, size_t capacity, size_t &length) {
  if (!settingsStoreBegin()) return false;
  const Image &image = transaction ? *working : *committed;
  if (capacity < image.length) return false;
  length = image.length;
  memcpy(output, image.data, length);
  return true;
}
bool settingsImport(const uint8_t *input, size_t length) {
  if (!validImage(input, length) || transaction) return false;
  // A complete replacement does not need a readable source image. If the
  // partition and staging memory are available, let authenticated restore
  // validate the new image before committing it to the inactive slot.
  if (!settingsStoreBegin() && !storageAccessible) return false;
  transaction = true;
  transactionFailed = false;
  memset(working, 0, sizeof(*working));
  working->length = length;
  memcpy(working->data, input, length);
  return true;
}

bool SettingsPreferences::begin(const char *name, bool readOnly, const char *partition) {
  if (!name || strcmp(partition, "clockcfg") || !settingsStoreBegin()) return false;
  namespace_ = name;
  readOnly_ = readOnly;
  return true;
}
size_t SettingsPreferences::getBytesLength(const char *key) {
  size_t length = 0;
  if (namespace_) findValue(transaction ? *working : *committed, keyId(namespace_, key), length);
  return length;
}
size_t SettingsPreferences::getBytes(const char *key, void *output, size_t capacity) {
  if (!namespace_) return 0;
  size_t length = 0;
  const uint8_t *value = findValue(transaction ? *working : *committed, keyId(namespace_, key), length);
  if (!value || capacity < length) return 0;
  memcpy(output, value, length);
  return length;
}
size_t SettingsPreferences::putBytes(const char *key, const void *data, size_t length) {
  return readOnly_ ? 0 : writeValue(keyId(namespace_, key), data, length);
}
uint8_t SettingsPreferences::getUChar(const char *key, uint8_t fallback) {
  uint8_t value; return getBytes(key, &value, 1) == 1 ? value : fallback;
}
uint32_t SettingsPreferences::getUInt(const char *key, uint32_t fallback) {
  uint32_t value; return getBytes(key, &value, 4) == 4 ? value : fallback;
}
float SettingsPreferences::getFloat(const char *key, float fallback) {
  float value; return getBytes(key, &value, 4) == 4 ? value : fallback;
}
bool SettingsPreferences::getBool(const char *key, bool fallback) { return getUChar(key, fallback ? 1 : 0) != 0; }
String SettingsPreferences::getString(const char *key, const char *fallback) {
  char value[64];
  const size_t size = getBytes(key, value, sizeof(value));
  return size && value[size - 1] == 0 ? String(value) : String(fallback);
}
size_t SettingsPreferences::putUChar(const char *key, uint8_t value) { return putBytes(key, &value, 1); }
size_t SettingsPreferences::putUInt(const char *key, uint32_t value) { return putBytes(key, &value, 4); }
size_t SettingsPreferences::putFloat(const char *key, float value) { return putBytes(key, &value, 4); }
size_t SettingsPreferences::putBool(const char *key, bool value) { return putUChar(key, value ? 1 : 0); }
size_t SettingsPreferences::putString(const char *key, const String &value) {
  return putBytes(key, value.c_str(), value.length() + 1) == value.length() + 1 ? value.length() : 0;
}
bool SettingsPreferences::remove(const char *key) {
  if (readOnly_) return false;
  const bool own = !settingsTransactionActive();
  if (own && !settingsTransactionBegin()) return false;
  const bool ok = replaceValue(*working, keyId(namespace_, key), nullptr, 0);
  if (!ok) transactionFailed = true;
  return own ? settingsTransactionCommit() : ok;
}

#ifdef SETTINGS_STORE_HOST_TEST
void settingsStoreTestReset() {
  if (committed) { mbedtls_platform_zeroize(committed, sizeof(*committed)); free(committed); }
  if (working) { mbedtls_platform_zeroize(working, sizeof(*working)); free(working); }
  committed = working = nullptr;
  initialized = storageAccessible = transaction = transactionFailed = false;
  selectedSlot = 255;
}
#endif
