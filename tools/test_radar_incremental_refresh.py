#!/usr/bin/env python3
"""Run production cache/load/refresh code with bounded PSRAM and mocked I/O."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
src = (root / 'WaveshareHodiny/ChmiRadarService.cpp').read_text()
def section(start, end):
    a = src.index(start)
    return src[a:src.index(end, a)]
harness = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <utility>
using std::min;
constexpr int WL_CONNECTED=1, MALLOC_CAP_SPIRAM=1, MALLOC_CAP_8BIT=2;
struct {int status(){return WL_CONNECTED;}} WiFi;
constexpr size_t MAX_ANIMATION_FRAME_COUNT=15, FILE_NAME_CAPACITY=96, MAX_PENDING_REFRESH_FRAMES=4;
constexpr size_t PNG_CAPACITY=446086, RADAR_PIXEL_COUNT=230400;
size_t animationFrameCount=0,cachedPngCount=0,pendingRefreshCount=0;
char preparedFrameNames[15][96]={},pendingFrameNames[4][96]={},pendingFrameTimes[4][6]={};
char cachedPngNames[15][96]={},preparedFrameTimes[15][6]={};
uint32_t pendingFrameRevisions[4]={},preparedFrameRevisions[15]={};
bool preparedFrameReady[15]={};
uint8_t *preparedFrames[15]={},*pendingPreparedFrames[4]={},*cachedPngFrames[15]={},*pendingPngFrames[4]={};
size_t cachedPngSizes[15]={},cachedPngCapacities[15]={},pendingPngSizes[4]={},pendingPngCapacities[4]={};
uint8_t workspace[PNG_CAPACITY],*pngBuffer=workspace;
bool animationPause=false,visible=false,preparationInProgress=false,fullPreparationInProgress=false,ready=false;
int displayedFrame=-1; uint32_t requestRevision=1,generation=0;
unsigned long lastProgressiveFrameShownAt=0; uint16_t activeRadiusKm=0;
#define portENTER_CRITICAL(x)
#define portEXIT_CRITICAL(x)
std::vector<std::string> serverNames,downloads;
std::map<void*,size_t> allocations;
size_t memoryLimit=4*1024*1024, allocated=0, wireSize=50000;
int manifests=0, failDownload=-1; bool sk=true,failPending=false;
bool slovakSource(){return sk;}
size_t heap_caps_get_free_size(int){return memoryLimit-allocated;}
void *allocateRadarMemory(size_t n,int){if(n>memoryLimit-allocated)return nullptr;void*p=malloc(n);assert(p);allocations[p]=n;allocated+=n;return p;}
void heap_caps_free(void*p){if(!p)return;assert(allocations.count(p));allocated-=allocations.at(p);allocations.erase(p);free(p);}
bool ensureBuffers(){return true;}
void setStatus(bool,const char*){}
bool latestFileNames(char output[][96],size_t &count,uint32_t){++manifests;count=serverNames.size();for(size_t i=0;i<count;++i)strcpy(output[i],serverNames[i].c_str());return true;}
bool downloadPngWithRetry(const char*n,size_t &size,uint32_t){if(failDownload==int(downloads.size()))return false;downloads.push_back(n);size=wireSize;memset(pngBuffer,42,size);return true;}
bool decodeRadar(const uint8_t*p,size_t,float,float,uint16_t,uint8_t,uint8_t*out){assert(p[0]==42);memset(out,42,RADAR_PIXEL_COUNT);return true;}
bool requestMatches(uint32_t r){return r==requestRevision;}
void frameTimeFromName(const char*,char*out){strcpy(out,"12:00");}
bool showPreparedFrame(size_t i,unsigned long){assert(preparedFrameReady[i]);return true;}
bool showProgressivelyPreparedFrame(size_t i,uint16_t,uint32_t){assert(preparedFrameReady[i]);return true;}
void finishProgressivePreparation(uint32_t){preparationInProgress=fullPreparationInProgress=false;}
unsigned long millis(){return 0;}
'''
harness += section('bool ensurePreparedFrame(', '// Source files are optional:')
harness += section('bool reserveSourceCache(', '\nvoid insertLatestName(')
harness += section('bool ensurePendingFrame(', '\nvoid beginProgressivePreparation(').replace('if (slot >= MAX_PENDING_REFRESH_FRAMES) return false;', 'if (failPending || slot >= MAX_PENDING_REFRESH_FRAMES) return false;')
harness += section('void releaseUnusedFrames(', '\nbool rebuildAnimationFromCache(')
harness += section('void releasePendingWorkspace(', '\nint firstPreparedFrame() {')
# Compile the actual scheduler condition using 32-bit millis on the host.
condition = section('if (reloadNow || nextAttemptAt == 0 ||', ') {\n      const bool fullPreparation')
harness += '\nbool due(uint32_t now,uint32_t nextAttemptAt,bool reloadNow=false,bool haveFrames=true){' + condition + ') return true; return false;}\n'
harness += r'''
void reset(){
 releasePendingWorkspace();releaseUnusedFrames(0);assert(allocated==0);
 animationFrameCount=cachedPngCount=0;ready=false;requestRevision=1;
 wireSize=50000;memoryLimit=4*1024*1024;sk=true;failPending=false;failDownload=-1;
 downloads.clear();serverNames.clear();manifests=0;
}
void names(int first,int count){serverNames.clear();for(int i=first;i<first+count;++i)serverNames.push_back(std::to_string(i));}
void initial(int count){names(0,count);assert(loadAnimation(0,0,10,count,100,1));downloads.clear();manifests=0;}
void check(int count){
 assert(animationFrameCount==size_t(count));std::set<uint8_t*> unique;
 for(int i=0;i<count;++i){assert(preparedFrameReady[i]);assert(serverNames[i]==preparedFrameNames[i]);assert(preparedFrames[i][0]==42);assert(unique.insert(preparedFrames[i]).second);}
}
int main(){
 for(int count : {1,6,10,15})for(int shift=0;shift<=count;++shift){
  reset();sk=count!=15;initial(count);names(shift,count);
  assert(refreshLatestFrame(0,0,10,count,100,1));if(pendingRefreshCount)commitPendingRefresh();
  assert(manifests==1);assert(downloads.size()==size_t(shift));check(count);
  downloads.clear();assert(refreshLatestFrame(0,0,10,count,100,1));assert(downloads.empty());
 }
 puts("PASS: CZ 15/SK 10, every shift incl. >4, no duplicate manifest or cached download");
 reset();initial(10);serverNames[4]="replacement";
 assert(refreshLatestFrame(0,0,10,10,100,1));assert(downloads.size()==1);check(10);
 reset();initial(10);names(5,10);failDownload=2;
 assert(!refreshLatestFrame(0,0,10,10,100,1));failDownload=-1;downloads.clear();
 assert(refreshLatestFrame(0,0,10,10,100,1));assert(downloads.size()==3);check(10);
 reset();initial(10);names(3,10);failDownload=2;
 assert(!refreshLatestFrame(0,0,10,10,100,1));failDownload=-1;downloads.clear();
 assert(refreshLatestFrame(0,0,10,10,100,1));assert(downloads.size()==1);check(10);
 reset();initial(10);names(3,10);failDownload=2;
 assert(!refreshLatestFrame(0,0,10,10,100,1));failDownload=-1;downloads.clear();names(5,10);
 assert(refreshLatestFrame(0,0,10,10,100,1));assert(downloads.size()==3);check(10);
 reset();initial(6);names(0,10);assert(refreshLatestFrame(0,0,10,10,100,1));assert(downloads.size()==4);check(10);
 downloads.clear();names(4,6);assert(refreshLatestFrame(0,0,10,6,100,1));assert(downloads.empty());check(6);
 puts("PASS: changed history/count and interrupted refresh retain all successful frames");
 reset();initial(10);failPending=true;names(2,10);
 assert(refreshLatestFrame(0,0,10,10,100,1));assert(downloads.size()==2);check(10);
 puts("PASS: pending allocation failure reuses active frame slots without duplicate downloads");
 reset();wireSize=PNG_CAPACITY;names(0,10);
 assert(loadAnimation(0,0,10,10,100,1));check(10);
 for(int shift=1;shift<100;++shift){
  names(shift,10);downloads.clear();assert(refreshLatestFrame(0,0,10,10,100,1));if(pendingRefreshCount)commitPendingRefresh();
  assert(downloads.size()==1);check(10);
  size_t sources=0;for(size_t n:cachedPngCapacities)sources+=n;for(size_t n:pendingPngCapacities)sources+=n;
  assert(sources<=1024*1024);assert(allocated<=memoryLimit);
 }
 names(104,10);downloads.clear();assert(refreshLatestFrame(0,0,10,10,100,1));check(10);assert(downloads.size()==5);
 reset();wireSize=PNG_CAPACITY;names(0,10);assert(loadAnimation(0,0,10,10,100,1));names(4,10);downloads.clear();
 assert(refreshLatestFrame(0,0,10,10,100,1));if(pendingRefreshCount)commitPendingRefresh();check(10);assert(downloads.size()==4);
 puts("PASS: 100 worst-size SK refreshes and four-frame catch-up preserve 10 prepared frames");
 assert(!due(1000,61000,false,false)); // Failed initial load must back off too.
 assert(due(1000,61000,true,false));
 assert(due(2160000000u,0));assert(due(0x20,0xfffffff0));assert(!due(0xfffffff0,0x20));
 puts("PASS: immediate refresh after 25 days and millis rollover");
 reset();
}
'''
with tempfile.TemporaryDirectory(prefix='radar-refresh-') as tmp:
    cpp = Path(tmp)/'test.cpp'; cpp.write_text(harness)
    exe = Path(tmp)/'test'
    subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
