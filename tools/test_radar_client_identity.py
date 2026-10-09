#!/usr/bin/env python3
"""Exercise the actual client header implementation with transactional NVS stubs."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='radar-identity-') as temp:
    tmp = Path(temp)
    (tmp / 'HTTPClient.h').write_text('''#pragma once
#include <map>
#include <string>
constexpr int HTTPC_DISABLE_FOLLOW_REDIRECTS=0;
class HTTPClient { public:
 std::map<std::string,std::string> headers; int redirects=-1;
 void addHeader(const char* key,const char* value){headers[key]=value;}
 void setFollowRedirects(int value){redirects=value;}
};
''')
    (tmp / 'esp_random.h').write_text('''#pragma once
#include <cstddef>
#include <cstdint>
void esp_fill_random(void*,size_t);
''')
    (tmp / 'nvs.h').write_text('''#pragma once
#include <cstddef>
using esp_err_t=int;using nvs_handle_t=int;
constexpr int ESP_OK=0,ESP_ERR_NVS_NOT_FOUND=1,ESP_ERR_NVS_INVALID_LENGTH=2,NVS_READWRITE=1;
int nvs_open_from_partition(const char*,const char*,int,nvs_handle_t*);
int nvs_get_str(nvs_handle_t,const char*,char*,size_t*);
int nvs_set_str(nvs_handle_t,const char*,const char*);
int nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
''')
    (tmp / 'test.cpp').write_text(r'''
#include "HTTPClient.h"
#include "nvs.h"
#include "RadarClientIdentity.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <regex>
std::string stored,pending;int writes=0,randomCalls=0,readError=0;bool openFail=false,commitFail=false,verifyFail=false;
int nvs_open_from_partition(const char* partition,const char* space,int,nvs_handle_t* h){
 assert(std::string(partition)=="clockcfg");assert(std::string(space)=="radar-client");*h=1;return openFail?99:ESP_OK;
}
int nvs_get_str(nvs_handle_t,const char* key,char* out,size_t* n){
 assert(std::string(key)=="id");if(readError)return readError;if(stored.empty())return ESP_ERR_NVS_NOT_FOUND;
 if(*n<stored.size()+1){*n=stored.size()+1;return ESP_ERR_NVS_INVALID_LENGTH;}
 *n=stored.size()+1;std::memcpy(out,stored.c_str(),*n);if(verifyFail)out[0]='z';return ESP_OK;
}
int nvs_set_str(nvs_handle_t,const char*,const char* value){pending=value;++writes;return ESP_OK;}
int nvs_commit(nvs_handle_t){if(commitFail)return 99;stored=pending;return ESP_OK;}
void nvs_close(nvs_handle_t){pending.clear();}
void esp_fill_random(void* p,size_t n){++randomCalls;std::memset(p,randomCalls,n);}
int main(int argc,char** argv){
 assert(argc==2);const std::string scenario=argv[1];
 const std::string original="01010101-0101-4101-8101-010101010101";
 if(scenario=="existing")stored=original;
 if(scenario=="corrupt")stored="invalid";
 if(scenario=="open")openFail=true;
 if(scenario=="read"){stored=original;readError=99;}
 if(scenario=="commit")commitFail=true;
 if(scenario=="verify")verifyFail=true;
 HTTPClient before;addRadarClientHeaders(before);
 assert(before.headers.count("X-Radar-Device-ID")==0&&writes==0&&randomCalls==0);
 bool ready=prepareRadarClientIdentity();
 if(scenario=="open"||scenario=="read"||scenario=="commit"||scenario=="verify"){
  assert(!ready);HTTPClient failed;addRadarClientHeaders(failed);
  assert(failed.headers.count("X-Radar-Device-ID")==0);
  if(scenario=="read")assert(stored==original&&writes==0);
  if(scenario=="commit")assert(stored.empty());
  openFail=commitFail=verifyFail=false;readError=0;
  assert(prepareRadarClientIdentity());
 }else assert(ready);
 HTTPClient first;addRadarClientHeaders(first);
 const auto id=first.headers.at("X-Radar-Device-ID");
 assert(std::regex_match(id,std::regex("[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}")));
 assert(first.headers.at("X-Radar-Firmware")=="development");assert(first.redirects==0);
 if(scenario=="existing")assert(id==original&&writes==0&&randomCalls==0);
 const int oldWrites=writes;const int oldRandom=randomCalls;
 // Every subsequent request must be flash-free, even if storage becomes unavailable.
 openFail=true;readError=99;
 HTTPClient next;addRadarClientHeaders(next);
 assert(next.headers.at("X-Radar-Device-ID")==id);assert(prepareRadarClientIdentity());
 assert(writes==oldWrites&&randomCalls==oldRandom);
 std::cout<<"PASS: "<<scenario<<"\n";
}
''')
    subprocess.run(['c++', '-std=c++17', '-I'+str(tmp), '-I'+str(ROOT/'WaveshareHodiny'),
                    str(tmp/'test.cpp'), str(ROOT/'WaveshareHodiny/RadarClientIdentity.cpp'),
                    '-o', str(tmp/'test')], check=True)
    for scenario in ['new', 'existing', 'corrupt', 'open', 'read', 'commit', 'verify']:
        subprocess.run([str(tmp/'test'), scenario], check=True)
