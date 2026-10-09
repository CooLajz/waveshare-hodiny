#include <cassert>
#include <fstream>
#include <sstream>
#include <string>
#include "../WaveshareHodiny/SlovakRadarManifest.h"

int main(int argc, char **argv) {
  assert(argc == 2);
  std::ifstream input(argv[1]);
  std::ostringstream contents; contents << input.rdbuf();
  const std::string original = contents.str();
  cJSON *root = cJSON_Parse(original.c_str()); assert(root);
  const auto *items = slovakRadar::field(root, "frames");
  const time_t now = static_cast<time_t>(slovakRadar::field(cJSON_GetArrayItem(items, cJSON_GetArraySize(items)-1), "time")->valuedouble) + 600;
  slovakRadar::Frame frames[10]; size_t count;
  assert(slovakRadar::parse(original.c_str(), now, frames, count) && count == static_cast<size_t>(cJSON_GetArraySize(items)));
  assert(!slovakRadar::parse(original.c_str(), now+1801, frames, count) && count == 0);
  assert(!slovakRadar::parse(original.c_str(), 0, frames, count));
  auto rejects = [&](const char *from, const char *to) {
    std::string changed = original; size_t at = changed.find(from); assert(at != std::string::npos);
    changed.replace(at, strlen(from), to);
    assert(!slovakRadar::parse(changed.c_str(), now, frames, count) && count == 0);
  };
  rejects("row-packbits-v1", "unknown");
  rejects("1665bd5c", "00000000");
  rejects("EPSG:3857", "EPSG:4326");
  rejects("outer_edges", "pixel_centers");
  rejects("13.6", "13.7");
  rejects("23.79", "23.8");
  rejects("800", "801");
  rejects("/v2/sk/frames/", "https://evil.invalid/");
  rejects("/v2/sk/frames/", "/v2/sk/frames/../");
  const char *firstId = slovakRadar::field(cJSON_GetArrayItem(items, 0), "id")->valuestring;
  rejects(firstId, "20200101000000");
  cJSON_Delete(root);
  for (const char *bad : {"{}", "null", "[]", "{", "{\"frames\":null}"})
    assert(!slovakRadar::parse(bad, now, frames, count));
  puts("PASS: SK manifest, time, geometry, path restrictions and malformed inputs");
}
