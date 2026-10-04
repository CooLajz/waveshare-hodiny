#!/usr/bin/env python3
"""Exercise production cache retention and radar allocation with a minimal host model."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
def function(path, signature):
    source = (root / path).read_text()
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

model = r'''
#include <cassert>
#include <atomic>
#include <cstdint>
#include <cstddef>
constexpr int CLOCK_STYLE_ANALOG=1, CLOCK_STYLE_DIGITAL=0;
int activeClockStyle=1, backgroundImage=0;
bool radar=false, night=false, resident=true, dial=true, shadow=true;
int loads=0, invalidations=0;
struct Options { bool present=true, enabled=true; int opacity=80, shadow=70, shadowSpread=3; } options;
std::atomic<bool> memoryReclaimRequested{false};
bool redNightVisualEnabled(){return night;}
bool clockDashboardRadarVisible(){return radar;}
bool analogLayoutEnabled(){return activeClockStyle==1;}
const Options &clockBackgroundActiveOptions(){return options;}
void *clockBackgroundPixels(){return resident? &backgroundImage:nullptr;}
void lv_img_cache_invalidate_src(void*){++invalidations;}
void releaseShadowCache(){shadow=false;}
void clearAnalogDialCache(){dial=false;}
void clockBackgroundSetResident(bool wanted){
 wanted = wanted && options.present && options.enabled && options.opacity;
 if(wanted && !resident) ++loads;
 resident=wanted;
}
bool chmiRadarServiceMemoryReclaimRequested(){return memoryReclaimRequested.load();}
void chmiRadarServiceMemoryReclaimCompleted(){memoryReclaimRequested.store(false);}
constexpr uint32_t MALLOC_CAP_SPIRAM=1;
using TickType_t=uint32_t;
int taskHandle=1, currentTask=1, allocationCalls=0, successesAfter=0;
TickType_t ticks=0;
bool serviceUi=true;
int xTaskGetCurrentTaskHandle(){return currentTask;}
TickType_t xTaskGetTickCount(){return ticks;}
TickType_t pdMS_TO_TICKS(int v){return v;}
void *heap_caps_malloc(size_t, uint32_t){return ++allocationCalls>successesAfter? &backgroundImage:nullptr;}
void maintainClockCaches();
void vTaskDelay(TickType_t delay){ticks+=delay; if(serviceUi)maintainClockCaches();}
void reset(){
 activeClockStyle=1;radar=night=false;resident=dial=shadow=true;
 loads=invalidations=0; options={};memoryReclaimRequested=false;
 allocationCalls=successesAfter=0;ticks=0;currentTask=1;serviceUi=true;
}
'''
cases = r'''
int main(){
 reset();
 for(int i=0;i<10;++i){
   radar=true;maintainClockCaches();assert(resident&&dial&&shadow);
   radar=false;activeClockStyle=3;maintainClockCaches();assert(resident&&dial&&shadow);
   activeClockStyle=0;maintainClockCaches();assert(resident&&dial&&shadow);
   activeClockStyle=1;maintainClockCaches();assert(resident&&dial&&shadow);
 }
 assert(loads==0);
 night=true;maintainClockCaches();assert(resident&&dial&&shadow);
 night=false;options.enabled=false;maintainClockCaches();assert(!resident&&!shadow&&invalidations);
 options.enabled=true;maintainClockCaches();assert(resident&&loads==1);
 options.present=false;maintainClockCaches();assert(!resident);
 reset();radar=true;memoryReclaimRequested=true;maintainClockCaches();
 assert(!resident&&!dial&&!shadow&&!memoryReclaimRequested);
 maintainClockCaches();assert(!resident&&loads==0);
 radar=false;maintainClockCaches();assert(resident&&loads==1);
 reset();memoryReclaimRequested=true;maintainClockCaches();
 assert(resident&&dial&&shadow&&!memoryReclaimRequested);
 reset();assert(allocateRadarMemory(100,1));assert(allocationCalls==1&&ticks==0);
 reset();radar=true;successesAfter=1;assert(allocateRadarMemory(100,1));
 assert(allocationCalls==2&&ticks==5&&!resident&&!dial&&!shadow);
 reset();successesAfter=100;serviceUi=false;assert(!allocateRadarMemory(100,1));
 assert(ticks==500&&allocationCalls==2);
 reset();successesAfter=100;currentTask=2;assert(!allocateRadarMemory(100,1));
 assert(ticks==0&&allocationCalls==1&&!memoryReclaimRequested);
}
'''
production = function('WaveshareHodiny/ClockDashboard.cpp', 'void maintainClockCaches() {') + '\n' + function('WaveshareHodiny/ChmiRadarService.cpp', 'void *allocateRadarMemory(')
with tempfile.TemporaryDirectory() as temp:
    cpp=Path(temp)/'test.cpp'; exe=Path(temp)/'test'
    cpp.write_text(model + production + cases)
    subprocess.run(['c++','-std=c++17','-fsanitize=address,undefined','-g',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Clock cache retention and radar memory pressure tests passed')
