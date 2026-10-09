#pragma once

#include <cJSON.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace slovakRadar {
constexpr char ORIGIN[] = "https://radar.nebovidy.cz:18443";
constexpr char MANIFEST_URL[] = "https://radar.nebovidy.cz:18443/v2/sk/manifest.json";
constexpr size_t MAX_FRAMES = 10;
constexpr size_t PATH_CAPACITY = 96;
constexpr size_t FRAME_LIMIT = 446086;
struct Image {
  char path[PATH_CAPACITY] = {};
  char sha256[65] = {};
  size_t bytes = 0;
};
struct Frame {
  time_t time = 0;
  Image image;
};
inline const cJSON *field(const cJSON *object, const char *name) {
  return cJSON_GetObjectItemCaseSensitive(object, name);
}
inline bool equals(const cJSON *item, const char *value) {
  return cJSON_IsString(item) && strcmp(item->valuestring, value) == 0;
}
inline bool number(const cJSON *item, double value) {
  return cJSON_IsNumber(item) && item->valuedouble == value;
}
inline bool image(const cJSON *object, const char *id, Image &out) {
  const auto *path = field(object, "url");
  const auto *hash = field(object, "sha256");
  const auto *bytes = field(object, "bytes");
  if (!cJSON_IsString(path) || !cJSON_IsString(hash) || strlen(hash->valuestring) != 64 ||
      !cJSON_IsNumber(bytes) || bytes->valuedouble < 2236 || bytes->valuedouble > FRAME_LIMIT ||
      floor(bytes->valuedouble) != bytes->valuedouble) return false;
  for (const char *p = hash->valuestring; *p; ++p)
    if (!(*p >= '0' && *p <= '9') && !(*p >= 'a' && *p <= 'f')) return false;
  char expected[PATH_CAPACITY];
  snprintf(expected, sizeof(expected), "/v2/sk/frames/%s-%.16s.nrd", id, hash->valuestring);
  if (strcmp(expected, path->valuestring) != 0) return false;
  strcpy(out.path, expected);
  strcpy(out.sha256, hash->valuestring);
  out.bytes = static_cast<size_t>(bytes->valuedouble);
  return true;
}
// Fixed projection contract: reject changed geometry instead of misplacing rain.
inline bool parse(const char *json, time_t now, Frame *frames, size_t &count) {
  count = 0;
  if (now < 1700000000) return false;
  cJSON *root = cJSON_Parse(json);
  const auto *bbox = field(root, "bbox");
  const auto *items = field(root, "frames");
  bool valid = number(field(root, "schema"), 2) &&
      equals(field(root, "format"), "NRD2") && equals(field(root, "codec"), "row-packbits-v1") &&
      number(field(root, "palette_id"), 1) && number(field(root, "geometry_id"), 1) &&
      number(field(root, "nodata_index"), 255) && number(field(root, "max_frame_bytes"), FRAME_LIMIT) &&
      equals(field(root, "palette_sha256"), "1665bd5c489e86c0bd4311f64bfcb7191576a645a3b84c0d4c7ff9f3e5629355") && equals(field(root, "product"), "SHMU-ZMAX") &&
      equals(field(root, "projection"), "EPSG:3857") && equals(field(root, "pixel_reference"), "outer_edges") &&
      number(field(root, "width"), 800) && number(field(root, "height"), 550) &&
      cJSON_IsArray(bbox) && cJSON_GetArraySize(bbox) == 4 && number(cJSON_GetArrayItem(bbox, 0), 13.6) &&
      number(cJSON_GetArrayItem(bbox, 1), 46.05) && number(cJSON_GetArrayItem(bbox, 2), 23.79) &&
      number(cJSON_GetArrayItem(bbox, 3), 50.7) && cJSON_IsArray(items) &&
      cJSON_GetArraySize(items) > 0 && cJSON_GetArraySize(items) <= static_cast<int>(MAX_FRAMES);
  const auto *stale = field(root, "stale_after_seconds");
  valid = valid && cJSON_IsNumber(stale) && stale->valuedouble > 0 && stale->valuedouble <= 1800;
  time_t previous = 0;
  for (int i = 0; valid && i < cJSON_GetArraySize(items); ++i) {
    const auto *item = cJSON_GetArrayItem(items, i);
    const auto *id = field(item, "id");
    const auto *stamp = field(item, "time");
    valid = cJSON_IsString(id) && strlen(id->valuestring) == 14 && cJSON_IsNumber(stamp) &&
        stamp->valuedouble > previous && stamp->valuedouble <= now + 300 &&
        stamp->valuedouble >= 1700000000 && floor(stamp->valuedouble) == stamp->valuedouble;
    if (!valid) break;
    Frame &frame = frames[i];
    frame.time = static_cast<time_t>(stamp->valuedouble);
    struct tm utc = {};
    gmtime_r(&frame.time, &utc);
    char expectedId[16];
    strftime(expectedId, sizeof(expectedId), "%Y%m%d%H%M%S", &utc);
    valid = strcmp(id->valuestring, expectedId) == 0 &&
        image(field(item, "image"), expectedId, frame.image);
    previous = frame.time;
    if (valid) ++count;
  }
  valid = valid && count > 0 && now - previous <= stale->valuedouble;
  cJSON_Delete(root);
  if (!valid) count = 0;
  return valid;
}
}  // namespace slovakRadar
