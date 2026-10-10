#pragma once
#include "SlovakRadarManifest.h"
#include <cstdint>

namespace czechRadar {
constexpr size_t MAX_FRAMES = 15;
struct Frame {
  char name[48] = {};
  time_t time = 0;
  slovakRadar::Image image;
};
inline const char *product(bool masked) { return masked ? "maxz-masked" : "maxz"; }
inline bool parse(const char *json, bool masked, time_t now, Frame *frames, size_t &count) {
  using namespace slovakRadar;
  count = 0;
  if (now < 1700000000) return false;
  cJSON *root = cJSON_Parse(json);
  const auto *items = field(root, "frames");
  bool valid = number(field(root, "schema"), 1) &&
      equals(field(root, "product"), product(masked)) &&
      equals(field(root, "format"), "CHMI-original-PNG") &&
      number(field(root, "stale_after_seconds"), 1800) && cJSON_IsArray(items) &&
      cJSON_GetArraySize(items) > 0 && cJSON_GetArraySize(items) <= 15;
  time_t previous = 0;
  for (int i = 0; valid && i < cJSON_GetArraySize(items); ++i) {
    const auto *item = cJSON_GetArrayItem(items, i);
    const auto *stamp = field(item, "time");
    valid = cJSON_IsNumber(stamp) && stamp->valuedouble > previous &&
        stamp->valuedouble >= 1700000000 && stamp->valuedouble <= now + 300 &&
        floor(stamp->valuedouble) == stamp->valuedouble;
    if (!valid) break;
    Frame &out = frames[i];
    out.time = static_cast<time_t>(stamp->valuedouble);
    struct tm utc = {};
    gmtime_r(&out.time, &utc);
    strftime(out.name, sizeof(out.name), "pacz2gmaps3.z_max3d.%Y%m%d.%H%M.0.png", &utc);
    const auto *image = field(item, "image");
    const auto *hash = field(image, "sha256");
    const auto *bytes = field(image, "bytes");
    valid = equals(field(item, "name"), out.name) && utc.tm_sec == 0 &&
        cJSON_IsString(hash) && strlen(hash->valuestring) == 64 &&
        cJSON_IsNumber(bytes) && bytes->valuedouble >= 33 && bytes->valuedouble <= 131072 &&
        floor(bytes->valuedouble) == bytes->valuedouble;
    if (!valid) break;
    for (const char *p = hash->valuestring; *p; ++p)
      if (!(*p >= '0' && *p <= '9') && !(*p >= 'a' && *p <= 'f')) valid = false;
    snprintf(out.image.path, sizeof(out.image.path), "/v1/cz/%s/frames/%s", product(masked), out.name);
    valid = valid && equals(field(image, "url"), out.image.path);
    strcpy(out.image.sha256, hash->valuestring);
    out.image.bytes = static_cast<size_t>(bytes->valuedouble);
    previous = out.time;
    if (valid) ++count;
  }
  valid = valid && count > 0 && now - previous <= 1800;
  cJSON_Delete(root);
  if (!valid) count = 0;
  return valid;
}
// Worker-owned circuit breaker. Unsigned elapsed arithmetic survives millis rollover.
struct Backend {
  bool fallback = false;
  uint32_t failedAt = 0;
  void failed(uint32_t now) { fallback = true; failedAt = now; }
  bool probeDue(uint32_t now) const { return fallback && uint32_t(now - failedAt) >= 3600000UL; }
  void recovered() { fallback = false; }
};
}  // namespace czechRadar
