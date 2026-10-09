#!/usr/bin/env python3
"""Production body parser: wire framing, short TLS reads, stalls and cancellation."""
from pathlib import Path
import subprocess
import tempfile
import sys
root=Path(__file__).resolve().parents[1]
src=(root/'WaveshareHodiny/ChmiRadarService.cpp').read_text()
body=src[src.index('class RadarBodySink'):src.index('\nbool latestSlovakNames')]
harness=r'''
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdio>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include "RadarHttpBody.h"
using std::min;
struct String:std::string {using std::string::string; void trim(){}; void toLowerCase(){};bool isEmpty(){return empty();}};
class Stream {public: virtual ~Stream(){};virtual size_t write(uint8_t)=0;virtual size_t write(const uint8_t*,size_t)=0;virtual int available()=0;virtual int read()=0;virtual int peek()=0;virtual void flush()=0;};
uint32_t currentRevision=1,ticks=0,yields=0;
bool requestMatches(uint32_t r){return r==currentRevision;}
uint32_t millis(){return ticks++;}
void delay(int){++yields;}
void advanceAnimation(uint32_t){}
#define portENTER_CRITICAL(x)
#define portEXIT_CRITICAL(x)
size_t lastDownloadedBytes=0;
struct Batch {
 struct Client{
  bool stopped=false,stall=false;size_t offset=0,fragment=1000000;
  std::vector<uint8_t> body;
  void stop(){stopped=true;}
  bool connected(){return !stopped && (stall || offset<body.size());}
  int available(){return stopped?0:min(fragment,body.size()-offset);}
  int read(uint8_t*out,size_t n){n=min(n,size_t(available()));memcpy(out,body.data()+offset,n);offset+=n;return n;}
 }client;
 struct Http{
  int declared=4;String encoding;
  int getSize(){return declared;}
  String header(const char*){return encoding;}
  void end(){}
 }http;
 size_t bytes=0;
} batch,*httpBatch=&batch;
'''+body+r'''
int main(int argc,char**argv){
 auto setup=[&](const std::string &wire,int length,bool chunked){batch=Batch{};batch.client.body.assign(wire.begin(),wire.end());batch.http.declared=length;batch.http.encoding=chunked?"chunked":"";ticks=yields=0;};
 std::vector<uint8_t> out(2*1024*1024+1);
 auto run=[&](size_t limit,uint32_t revision=1){RadarBuffer b{out.data()};RadarBodySink sink(appendRadarBuffer,&b,limit,revision);return readRadarBody(sink);};
 setup("abcd",4,false);assert(run(4));assert(!batch.client.stopped);assert(!memcmp(out.data(),"abcd",4));
 setup("abcd",4,false);assert(!run(3));assert(batch.client.stopped);
 setup("abcd",5,false);assert(!run(5));assert(batch.client.stopped);
 setup("abcd",-1,false);assert(run(4));
 setup("abcd",4,false);assert(!run(4,2));assert(batch.client.stopped);
 for(size_t fragment:{1ul,2ul,3ul,1024ul}){
  setup("1;x=y\r\na\r\n3\r\nbcd\r\n0\r\nX-Test: value\r\n\r\n",-1,true);batch.client.fragment=fragment;
  assert(run(4));assert(batch.client.offset==batch.client.body.size());assert(!memcmp(out.data(),"abcd",4));
 }
 for(const auto&wire:{"4\r\nab", "4\r\nabcd\r\n", "4\r\nabcd\r\n0\r\n", "z\r\n", "FFFFFFFFFFFFFFFFFFFFFFFF\r\n", "4\r\nabcdXX0\r\n\r\n"}){
  setup(wire,-1,true);assert(!run(4));assert(batch.client.stopped);
 }
 setup("0\r\n\r\n",4,true);assert(!run(4));
 setup("4\r\nab",-1,true);batch.client.stall=true;assert(!run(4));assert(yields>0);assert(ticks<50000);
 setup("abcd",4,false);batch.http.encoding="gzip";assert(!run(4));
 puts("PASS: real chunk framing, fragmented TLS reads, trailer consumption, bounds, truncation, yielding timeout and cancellation");
 if(argc==2){
  std::ifstream f(argv[1],std::ios::binary);std::string wire((std::istreambuf_iterator<char>(f)),{});assert(!wire.empty());
  setup(wire,-1,true);batch.client.fragment=37;assert(run(out.size()-1));assert(batch.client.offset==wire.size());
  std::string decoded(reinterpret_cast<char*>(out.data()),lastDownloadedBytes);
  assert(decoded.find("pacz2gmaps3.z_max3d.")!=std::string::npos);
  assert(decoded.find("</html>")!=std::string::npos);
  printf("PASS: complete live CHMI chunked directory (%zu payload bytes)\n",lastDownloadedBytes);
 }
}
'''
with tempfile.TemporaryDirectory(prefix='radar-body-') as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(harness)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(root/'WaveshareHodiny'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test'),*sys.argv[1:]],check=True)
