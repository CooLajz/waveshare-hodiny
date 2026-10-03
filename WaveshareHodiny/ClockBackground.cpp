#include "ClockBackground.h"
#include "DisplayDriver.h"
#include <FFat.h>
#include "SettingsStore.h"
#include <esp_heap_caps.h>
#include <esp_partition.h>
#include <esp_rom_crc.h>
#include <esp_system.h>
#include <wear_levelling.h>

namespace {
ClockBackgroundOptions options;
ClockBackgroundOptions previewOptions;
bool previewActive = false;
uint8_t *pixels = nullptr;
bool pending = false;
uint32_t uploadChecksum = 0;
uint32_t retryPixelsAt = 0;
size_t received = 0;
String uploadToken;
uint32_t touchedAt = 0;
uint32_t revision = 1;
bool mounted = false;
uint32_t displayReleaseAt = 0;
void releaseDisplaySoon() { displayReleaseAt = millis() + 300; }
struct StorageDisplayGuard {
  bool ready;
  StorageDisplayGuard() : ready(displayDriverBeginStorageTransfer()) {}
  ~StorageDisplayGuard() { releaseDisplaySoon(); }
};
const char *path(uint8_t slot) { return slot ? "/clock-bg-1.rgb" : "/clock-bg-0.rgb"; }
void cancelUpload() {
  pending = false; received = 0; uploadToken = ""; uploadChecksum = 0;
  releaseDisplaySoon();
}
bool mountStorage() {
  if (mounted) return true;
  if (FFat.begin(false, "/ffat", 2, "ffat")) { mounted = true; return true; }
  // Mounting an empty FAT partition already initializes wear-levelling
  // metadata. Check logical sectors (not the metadata) before formatting.
  // Never erase an unknown or damaged filesystem containing user bytes.
  const auto *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
      ESP_PARTITION_SUBTYPE_DATA_FAT, "ffat");
  if (!partition) return false;
  wl_handle_t handle = WL_INVALID_HANDLE;
  if (wl_mount(partition, &handle) != ESP_OK) return false;
  bool empty = true;
  uint8_t block[512];
  const size_t size = wl_size(handle);
  for (size_t offset = 0; empty && offset < size; offset += sizeof(block)) {
    if (wl_read(handle, offset, block, sizeof(block)) != ESP_OK) { empty = false; break; }
    for (uint8_t byte : block) if (byte != 0xff) { empty = false; break; }
    delay(0);
  }
  wl_unmount(handle);
  if (!empty || !size) return false;
  if (!FFat.format()) return false;
  mounted = FFat.begin(false, "/ffat", 2, "ffat");
  return mounted;
}
bool persist(const ClockBackgroundOptions &next) {
  SettingsPreferences prefs;
  if (!prefs.begin("clock-bg", false, "clockcfg")) return false;
  const bool ok = prefs.putBytes("options", &next, sizeof(next)) == sizeof(next);
  prefs.end();
  if (ok) { options = next; previewActive = false; ++revision; }
  return ok;
}
uint8_t *allocatePixels() {
  return static_cast<uint8_t *>(heap_caps_malloc(CLOCK_BACKGROUND_BYTES,
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
}
int hexDigit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}
}
void clockBackgroundBegin() {
  SettingsPreferences prefs;
  if (prefs.begin("clock-bg", true, "clockcfg")) {
    ClockBackgroundOptions stored;
    if (prefs.getBytesLength("options") == sizeof(stored) &&
        prefs.getBytes("options", &stored, sizeof(stored)) == sizeof(stored) &&
        clockBackgroundOptionsValid(stored))
      options = stored;
    prefs.end();
  }
}
File clockBackgroundOpenImage() {
  if (!options.present || !mountStorage()) return File();
  File file = FFat.open(path(options.slot), FILE_READ);
  if (!file || file.size() != CLOCK_BACKGROUND_BYTES) return File();
  return file;
}
void clockBackgroundSetResident(bool wanted) {
  const auto &active = clockBackgroundActiveOptions();
  wanted = wanted && active.present && active.enabled && active.opacity;
  if (!wanted) {
    if (pixels) { heap_caps_free(pixels); pixels = nullptr; ++revision; }
    retryPixelsAt = 0;
    return;
  }
  if (pixels || pending || (retryPixelsAt && static_cast<int32_t>(millis() - retryPixelsAt) < 0)) return;
  retryPixelsAt = millis() + 10000;
  File file = clockBackgroundOpenImage();
  if (!file) return;
  auto *loaded = allocatePixels();
  if (!loaded) return;
  if (file.read(loaded, CLOCK_BACKGROUND_BYTES) != CLOCK_BACKGROUND_BYTES ||
      esp_rom_crc32_le(0, loaded, CLOCK_BACKGROUND_BYTES) != options.checksum) {
    heap_caps_free(loaded); return;
  }
  pixels = loaded; retryPixelsAt = 0; ++revision;
}

const ClockBackgroundOptions &clockBackgroundOptions() { return options; }
const ClockBackgroundOptions &clockBackgroundActiveOptions() {
  return previewActive ? previewOptions : options;
}
bool clockBackgroundPreview(bool enabled, uint8_t opacity, uint8_t shadow, uint8_t shadowSize, uint8_t shadowSpread, bool clockOnly) {
  if (pending || displayDriverStorageTransferActive() || opacity > 100 || shadow > 100 || shadowSize > 5 || shadowSpread > 5) return false;
  ClockBackgroundOptions next = options;
  next.clockOnly = clockOnly;
  next.enabled = enabled; next.opacity = opacity; next.shadow = shadow;
  next.shadowSize = shadowSize; next.shadowSpread = shadowSpread;
  if (!memcmp(&next, &clockBackgroundActiveOptions(), sizeof(next))) return true;
  previewOptions = options;
  previewOptions.clockOnly = clockOnly;
  previewOptions.enabled = enabled;
  previewOptions.opacity = opacity;
  previewOptions.shadow = shadow;
  previewOptions.shadowSize = shadowSize; previewOptions.shadowSpread = shadowSpread;
  previewActive = true;
  ++revision;
  return true;
}
const uint8_t *clockBackgroundPixels() { return pixels; }
uint32_t clockBackgroundRevision() { return revision; }
bool clockBackgroundStart(String &token) {
  clockBackgroundLoop();
  if (pending) return false;
  pending = true;
  if (!displayDriverBeginStorageTransfer() || !mountStorage()) {
    cancelUpload();
    return false;
  }
  displayReleaseAt = 0;
  char value[33];
  snprintf(value, sizeof(value), "%08lx%08lx%08lx%08lx", (unsigned long)esp_random(),
      (unsigned long)esp_random(), (unsigned long)esp_random(), (unsigned long)esp_random());
  uploadToken = value; token = uploadToken; touchedAt = millis(); received = 0;
  return true;
}
bool clockBackgroundChunk(const String &token, size_t offset, const String &hex) {
  if (!pending || token != uploadToken || offset != received || hex.isEmpty() ||
      hex.length() % 2 || hex.length() > 16384 || hex.length() / 2 > CLOCK_BACKGROUND_BYTES - received)
    return false;
  // Validate before writing; only one 512-byte scratch block is needed.
  for (char c : hex) if (hexDigit(c) < 0) return false;
  File file = FFat.open(path(1 - options.slot), received ? FILE_APPEND : FILE_WRITE);
  if (!file || (received && file.size() != received)) return false;
  uint8_t block[512];
  for (size_t base = 0; base < hex.length() / 2; base += sizeof(block)) {
    const size_t count = min(sizeof(block), hex.length() / 2 - base);
    for (size_t i = 0; i < count; ++i)
      block[i] = (hexDigit(hex[(base+i)*2]) << 4) | hexDigit(hex[(base+i)*2+1]);
    if (file.write(block, count) != count) { cancelUpload(); return false; }
    uploadChecksum = esp_rom_crc32_le(uploadChecksum, block, count);
  }
  file.flush(); received += hex.length() / 2; touchedAt = millis(); return true;
}
bool clockBackgroundCommit(const String &token, const ClockBackgroundOptions *requested) {
  if (!pending || token != uploadToken || received != CLOCK_BACKGROUND_BYTES) return false;
  ClockBackgroundOptions next = requested ? *requested : options;
  if (!clockBackgroundOptionsValid(next)) return false;
  next.slot = 1 - options.slot; next.present = 1;
  if (!requested) next.enabled = 1;
  next.checksum = uploadChecksum;
  File file = FFat.open(path(next.slot), FILE_READ);
  bool ok = file && file.size() == CLOCK_BACKGROUND_BYTES;
  uint8_t block[512]; uint32_t checksum = 0;
  while (ok && file.available()) {
    const size_t count = file.read(block, sizeof(block));
    if (!count) { ok = false; break; }
    checksum = esp_rom_crc32_le(checksum, block, count);
  }
  file.close();
  if (!ok || checksum != next.checksum || !persist(next)) return false;
  heap_caps_free(pixels); pixels = nullptr; retryPixelsAt = 0;
  cancelUpload(); return true;
}

bool clockBackgroundCancel(const String &token) {
  if (!pending) return true;
  if (token != uploadToken) return false;
  cancelUpload();
  return true;
}
bool clockBackgroundSaveOptions(bool enabled, uint8_t opacity, uint8_t shadow, uint8_t shadowSize, uint8_t shadowSpread, bool remove, bool clockOnly) {
  if (pending || opacity > 100 || shadow > 100 || shadowSize > 5 || shadowSpread > 5) return false;
  ClockBackgroundOptions next = options;
  next.clockOnly = clockOnly;
  next.enabled = enabled; next.opacity = opacity; next.shadow = shadow; next.shadowSize = shadowSize; next.shadowSpread = shadowSpread;
  if (remove) next.present = 0;
  if (!memcmp(&next, &options, sizeof(next))) {
    if (previewActive) { previewActive = false; ++revision; }
    return true;
  }
  StorageDisplayGuard guard;
  if (!guard.ready) return false;
  if (!persist(next)) return false;
  if (remove) {
    heap_caps_free(pixels); pixels = nullptr;
    if (mounted) { FFat.remove(path(0)); FFat.remove(path(1)); }
    cancelUpload();
  }
  return true;
}
void clockBackgroundLoop() {
  if (pending && millis() - touchedAt > 60000) cancelUpload();
  if (!pending && displayReleaseAt &&
      static_cast<int32_t>(millis() - displayReleaseAt) >= 0) {
    if (displayDriverEndStorageTransfer()) displayReleaseAt = 0;
    else releaseDisplaySoon();
  }
}

bool clockBackgroundOptionsValid(const ClockBackgroundOptions &value) {
  return value.magic == 0x42474B31 && value.slot <= 1 && value.present <= 1 &&
      value.enabled <= 1 && value.opacity <= 100 && value.shadow <= 100 &&
      value.shadowSize <= 5 && value.shadowSpread <= 5 && value.clockOnly <= 1;
}
bool clockBackgroundBackupRead(size_t offset, uint8_t *data, size_t length) {
  if (!options.present || offset > CLOCK_BACKGROUND_BYTES || length > CLOCK_BACKGROUND_BYTES - offset || !mountStorage()) return false;
  File file = FFat.open(path(options.slot), FILE_READ);
  return file && file.size() == CLOCK_BACKGROUND_BYTES && file.seek(offset) && file.read(data, length) == length;
}
bool clockBackgroundRestoreWrite(size_t offset, const uint8_t *data, size_t length) {
  if (pending || offset > CLOCK_BACKGROUND_BYTES || length > CLOCK_BACKGROUND_BYTES - offset) return false;
  if (!offset) {
    if (!displayDriverBeginStorageTransfer()) return false;
    displayReleaseAt = 0;
  }
  if (!mountStorage()) return false;
  File file = FFat.open(path(1 - options.slot), offset ? FILE_APPEND : FILE_WRITE);
  // FILE_WRITE truncates on open; Arduino may cache the pre-truncation stat.
  if (!file || (offset && file.size() != offset)) return false;
  const bool ok = file.write(data, length) == length;
  file.flush();
  return ok;
}
bool clockBackgroundStageRestore(ClockBackgroundOptions next, size_t imageBytes) {
  if (!clockBackgroundOptionsValid(next) || imageBytes != (next.present ? CLOCK_BACKGROUND_BYTES : 0)) return false;
  if (next.present) {
    next.slot = 1 - options.slot;
    File file = FFat.open(path(next.slot), FILE_READ);
    if (!file || file.size() != CLOCK_BACKGROUND_BYTES) return false;
    uint8_t block[512]; uint32_t checksum = 0;
    size_t read = 0;
    while (read < CLOCK_BACKGROUND_BYTES) {
      const size_t count = file.read(block, sizeof(block));
      if (!count) return false;
      checksum = esp_rom_crc32_le(checksum, block, count); read += count;
    }
    if (checksum != next.checksum) return false;
  }
  SettingsPreferences prefs;
  return settingsTransactionActive() && prefs.begin("clock-bg") &&
      prefs.putBytes("options", &next, sizeof(next)) == sizeof(next);
}
void clockBackgroundRestoreEnd() { releaseDisplaySoon(); }

bool clockBackgroundStageOptions(const ClockBackgroundOptions &next) {
  SettingsPreferences prefs;
  return settingsTransactionActive() && clockBackgroundOptionsValid(next) &&
      prefs.begin("clock-bg") && prefs.putBytes("options", &next, sizeof(next)) == sizeof(next);
}
void clockBackgroundAdoptOptions(const ClockBackgroundOptions &next) {
  if (memcmp(&options, &next, sizeof(next)) || previewActive) {
    options = next; previewActive = false; ++revision;
  }
}
