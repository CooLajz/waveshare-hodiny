#!/usr/bin/env python3
"""Exercise production range gestures and optionally check ESP32 stack frames."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'WaveshareHodiny/WaveshareHodiny.ino').read_text()
def section(start,end):
 a=s.index(start);return s[a:s.index(end,a)]
code=r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include "ClockConfig.h"
ClockConfig runtimeConfig;
void *runtimeConfigMutex=reinterpret_cast<void*>(1);
constexpr int portMAX_DELAY=-1;
int locks=0;
void xSemaphoreTake(void*,int){assert(locks++==0);}
void xSemaphoreGive(void*){assert(--locks==0);}
bool clockConfigRadarAvailable(const ClockConfig &c){return c.openMeteoCountry==CLOCK_LOCATION_COUNTRY_CZECHIA || c.openMeteoCountry==CLOCK_LOCATION_COUNTRY_SLOVAKIA;}
bool radarRadiusApplyPending=false,radarRotationWaitingForCycle=true;
unsigned long radarRadiusApplyAt=0,displayModeStartedAt=0;
unsigned long millis(){return 1000;}
'''+section('ClockConfig runtimeConfigSnapshot()', '\nvoid loadRuntimeConfigForWeb')+section('void handleRadarRangeChange(', '\nvoid maintainRadarRangeChange')+r'''
int main(){
 for(uint8_t country:{CLOCK_LOCATION_COUNTRY_CZECHIA,CLOCK_LOCATION_COUNTRY_SLOVAKIA,CLOCK_LOCATION_COUNTRY_OTHER}){
  runtimeConfig.openMeteoCountry=country;
  assert(runtimeRadarAvailable()==(country!=CLOCK_LOCATION_COUNTRY_OTHER));assert(locks==0);
  for(uint16_t radius:{25,50,100,200,0})for(int8_t direction:{-1,0,1}){
   runtimeConfig.radarRadiusKm=radius;radarRadiusApplyPending=false;
   ClockConfig expected=runtimeConfig;
   const uint16_t radii[]={25,50,100,200,0};int i=0;while(radii[i]!=radius)++i;
   if(country!=CLOCK_LOCATION_COUNTRY_OTHER && ((direction<0&&i>0)||(direction>0&&i<4)))expected.radarRadiusKm=radii[i+(direction>0?1:-1)];
   handleRadarRangeChange(direction);
   assert(locks==0);assert(!memcmp(&expected,&runtimeConfig,sizeof(expected)));
   assert(radarRadiusApplyPending==(radius!=expected.radarRadiusKm));
   if(radarRadiusApplyPending){assert(radarRadiusApplyAt==1350);assert(displayModeStartedAt==1000);assert(!radarRotationWaitingForCycle);}
   const auto snapshot=runtimeConfigSnapshot();assert(!memcmp(&snapshot,&runtimeConfig,sizeof(snapshot)));assert(locks==0);
  }
 }
 runtimeConfigMutex=nullptr;const auto copy=runtimeConfigSnapshot();assert(!memcmp(&copy,&runtimeConfig,sizeof(copy)));
 puts("PASS: CZ/SK range gestures, both boundaries, unsupported countries, config preservation and balanced locking");
}
'''
with tempfile.TemporaryDirectory(prefix='radar-gestures-') as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(root/'WaveshareHodiny'),'-I'+str(root/'tools/backup_test_support'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
if len(sys.argv)==3:
 output=subprocess.check_output([sys.argv[1],'-d','-C',sys.argv[2]],text=True)
 for name,limit in [('runtimeConfigSnapshot()',256),('handleRadarRangeChange(signed char)',256),('maintainDisplayGestures()',1024)]:
  m=re.search(r'<[^\n]*'+re.escape(name)+r'>:\n[^\n]*entry\s+a1,\s*(0x[0-9a-f]+|[0-9]+)',output)
  assert m,'Missing compiled function '+name
  size=int(m[1],0);assert size<=limit,(name,size,limit)
  print(f'PASS: ESP32 compiled stack frame {name}: {size} bytes (limit {limit})')
