#include <cassert>
#include <fstream>
#include <sstream>
#include <string>
#include "../WaveshareHodiny/CzechRadarManifest.h"
int main(int argc, char **argv) {
  assert(argc == 2);
  std::ifstream input(argv[1]); std::ostringstream out; out << input.rdbuf();
  const std::string json = out.str();
  cJSON *root = cJSON_Parse(json.c_str()); assert(root);
  const auto *items = slovakRadar::field(root,"frames");
  const time_t now = slovakRadar::field(cJSON_GetArrayItem(items,cJSON_GetArraySize(items)-1),"time")->valuedouble + 60;
  czechRadar::Frame frames[15]; size_t count = 0;
  assert(czechRadar::parse(json.c_str(),false,now,frames,count) && count == 15);
  assert(!czechRadar::parse(json.c_str(),true,now,frames,count));
  assert(!czechRadar::parse(json.c_str(),false,now+1801,frames,count));
  assert(!czechRadar::parse(json.c_str(),false,0,frames,count));
  auto reject = [&] {
    char *text = cJSON_PrintUnformatted(root);
    assert(!czechRadar::parse(text,false,now,frames,count) && count == 0); cJSON_free(text);
  };
  cJSON *image = cJSON_GetObjectItem(cJSON_GetArrayItem(items,0),"image");
  cJSON_ReplaceItemInObject(image,"url",cJSON_CreateString("https://evil.invalid/file")); reject();
  cJSON_Delete(root);root=cJSON_Parse(json.c_str());
  image=cJSON_GetObjectItem(cJSON_GetArrayItem(cJSON_GetObjectItem(root,"frames"),0),"image");
  cJSON_ReplaceItemInObject(image,"bytes",cJSON_CreateNumber(131073));reject();
  cJSON_Delete(root);root=cJSON_Parse(json.c_str());
  cJSON_AddItemToArray(cJSON_GetObjectItem(root,"frames"),cJSON_CreateObject());reject();cJSON_Delete(root);
  czechRadar::Backend backend;
  assert(!backend.fallback && !backend.probeDue(0));
  const uint32_t rollover = UINT32_MAX-1000;
  backend.failed(rollover);
  assert(backend.fallback && !backend.probeDue(uint32_t(rollover+3599999UL)));
  assert(backend.probeDue(uint32_t(rollover+3600000UL)));
  backend.failed(uint32_t(rollover+3600000UL));
  assert(!backend.probeDue(uint32_t(rollover+3600001UL)));
  backend.recovered();assert(!backend.fallback);
  puts("PASS: CZ manifest bounds, source/URL contract, freshness, 15 frames and hourly fallback across rollover");
}
