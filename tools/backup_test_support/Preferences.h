#pragma once
#include "Arduino.h"
#include <map>
#include <vector>
#include <stdexcept>
namespace fakeNvs {
inline std::map<std::string, std::vector<uint8_t>> values;
enum Fault { None, SlotWrite, SlotRead, SelectorWrite, PowerAfterSlot, PowerAfterSelector };
inline Fault fault = None;
inline bool slotWritten = false;
}
class Preferences {
  std::string prefix;
 public:
  bool begin(const char *space, bool = false, const char *partition = "nvs") {
    prefix = std::string(partition) + "/" + space + "/"; return true;
  }
  void end() {}
  bool isKey(const char *key) { return fakeNvs::values.count(prefix + key); }
  size_t getBytesLength(const char *key) { auto it=fakeNvs::values.find(prefix+key); return it==fakeNvs::values.end()?0:it->second.size(); }
  size_t getBytes(const char *key, void *out, size_t capacity) {
    auto it=fakeNvs::values.find(prefix+key);
    if(it==fakeNvs::values.end()||it->second.size()>capacity)return 0;
    memcpy(out,it->second.data(),it->second.size());
    if(fakeNvs::fault==fakeNvs::SlotRead&&fakeNvs::slotWritten&&std::string(key).find("slot")==0)
      static_cast<uint8_t *>(out)[0]^=1;
    return it->second.size();
  }
  size_t putBytes(const char *key, const void *data, size_t size) {
    const bool slot=std::string(key).find("slot")==0;
    const bool selector=std::string(key)=="active";
    if((slot&&fakeNvs::fault==fakeNvs::SlotWrite)||(selector&&fakeNvs::fault==fakeNvs::SelectorWrite))return 0;
    fakeNvs::values[prefix+key]=std::vector<uint8_t>(static_cast<const uint8_t *>(data),static_cast<const uint8_t *>(data)+size);
    if(slot)fakeNvs::slotWritten=true;
    if((slot&&fakeNvs::fault==fakeNvs::PowerAfterSlot)||(selector&&fakeNvs::fault==fakeNvs::PowerAfterSelector))throw std::runtime_error("simulated power loss");
    return size;
  }
  uint8_t getUChar(const char *key,uint8_t fallback=0){uint8_t v;return getBytes(key,&v,1)==1?v:fallback;}
  uint32_t getUInt(const char *key,uint32_t fallback=0){uint32_t v;return getBytes(key,&v,4)==4?v:fallback;}
  float getFloat(const char *key,float fallback=0){float v;return getBytes(key,&v,4)==4?v:fallback;}
  bool getBool(const char *key,bool fallback=false){return getUChar(key,fallback)!=0;}
  String getString(const char *key,const char *fallback=""){char v[4096];auto size=getBytes(key,v,sizeof(v));return size&&v[size-1]==0?String(v):String(fallback);}
  size_t putUChar(const char *key,uint8_t v){return putBytes(key,&v,1);}
  size_t putUInt(const char *key,uint32_t v){return putBytes(key,&v,4);}
  size_t putFloat(const char *key,float v){return putBytes(key,&v,4);}
  size_t putString(const char *key,const String &v){return putBytes(key,v.c_str(),v.length()+1)?v.length():0;}
  bool remove(const char *key){return fakeNvs::values.erase(prefix+key)>0;}
};
