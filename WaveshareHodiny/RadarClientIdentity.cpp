#include "RadarClientIdentity.h"
#include "FirmwareBuild.h"

#include <HTTPClient.h>
#include <esp_random.h>
#include <nvs.h>
#include <cstdio>
#include <cstring>
#include <atomic>

namespace {
char preparedId[37] = {};
std::atomic<bool> identityReady{false};
bool validId(const char *id) {
  if (strlen(id) != 36 || id[14] != '4' ||
      (id[19] != '8' && id[19] != '9' && id[19] != 'a' && id[19] != 'b'))
    return false;
  for (size_t i = 0; i < 36; ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (id[i] != '-') return false;
    } else if (!((id[i] >= '0' && id[i] <= '9') ||
                 (id[i] >= 'a' && id[i] <= 'f'))) {
      return false;
    }
  }
  return true;
}

bool loadIdentity(char (&id)[37]) {
  // clockcfg survives ordinary development uploads and OTA. SettingsStore
  // exports only its own registered snapshot, not this separate namespace.
  nvs_handle_t handle;
  if (nvs_open_from_partition("clockcfg", "radar-client", NVS_READWRITE,
                              &handle) != ESP_OK)
    return false;
  size_t length = sizeof(id);
  const esp_err_t result = nvs_get_str(handle, "id", id, &length);
  if (result == ESP_OK && length == sizeof(id) && validId(id)) {
    nvs_close(handle);
    return true;
  }
  // A read error must not silently replace an existing identity.
  if (result != ESP_OK && result != ESP_ERR_NVS_NOT_FOUND &&
      result != ESP_ERR_NVS_INVALID_LENGTH) {
    nvs_close(handle);
    return false;
  }
  uint8_t random[16];
  // Called after Wi-Fi is connected, with the hardware entropy source active.
  esp_fill_random(random, sizeof(random));
  random[6] = (random[6] & 0x0f) | 0x40;
  random[8] = (random[8] & 0x3f) | 0x80;
  static constexpr char HEX_DIGITS[] = "0123456789abcdef";
  size_t pos = 0;
  for (size_t i = 0; i < sizeof(random); ++i) {
    if (i == 4 || i == 6 || i == 8 || i == 10) id[pos++] = '-';
    id[pos++] = HEX_DIGITS[random[i] >> 4];
    id[pos++] = HEX_DIGITS[random[i] & 15];
  }
  id[pos] = '\0';
  bool saved = nvs_set_str(handle, "id", id) == ESP_OK &&
               nvs_commit(handle) == ESP_OK;
  char verified[37] = {};
  length = sizeof(verified);
  saved = saved && nvs_get_str(handle, "id", verified, &length) == ESP_OK &&
          length == sizeof(verified) && strcmp(verified, id) == 0;
  nvs_close(handle);
  return saved;
}
}  // namespace

bool prepareRadarClientIdentity() {
  if (identityReady.load(std::memory_order_acquire)) return true;
  char id[37] = {};
  if (!loadIdentity(id)) return false;
  memcpy(preparedId, id, sizeof(preparedId));
  identityReady.store(true, std::memory_order_release);
  return true;
}

void addRadarClientHeaders(HTTPClient &http) {
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  http.addHeader("X-Radar-Firmware", FIRMWARE_VERSION);
  if (identityReady.load(std::memory_order_acquire))
    http.addHeader("X-Radar-Device-ID", preparedId);
}
