#include "../WaveshareHodiny/AlarmService.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
static time_t now;
static uint32_t ticks;
static bool sound, visible=true, storageOk=true;
static unsigned overlays=0;
static unsigned rings=0;
static AlarmSettings saved;
extern "C" time_t time(time_t *out) { if(out)*out=now;return now; }
uint32_t millis(){return ticks;}
void buzzerServiceAlarm(bool enabled){sound=enabled;if(enabled)++rings;}
bool displayNotificationShow(const char *,const char *,uint32_t,uint32_t,uint32_t,uint32_t){visible=true;++overlays;return true;}
void displayNotificationDismiss(){visible=false;}
bool save(const AlarmSettings &s){if(!storageOk)return false;saved=s;return true;}
void step(unsigned ms=1000){ticks+=ms;alarmServiceLoop();}
int main(){
  setenv("TZ","UTC0",1);tzset();assert(clockTimezoneSet("Etc/UTC"));
  tm local{};local.tm_year=126;local.tm_mon=9;local.tm_mday=9;local.tm_hour=7;now=mktime(&local);
  AlarmSettings settings;settings.entries[0]={420,127,1};
  alarmServiceBegin(save);alarmServiceApply(settings,false);step();
  assert(sound&&!visible&&overlays==0&&rings==1&&alarmServiceActive());
  alarmServiceDismiss();assert(!sound&&!visible);
  assert(alarmServiceBlocksTouch());ticks+=349;assert(alarmServiceBlocksTouch());
  ticks+=1;assert(!alarmServiceBlocksTouch());step();assert(rings==1);
  alarmServiceApply(saved,false);step();assert(rings==1);
  assert(alarmServiceSkipNext());assert(saved.skippedEpoch>now);
  assert(alarmServiceSkipNext());assert(saved.skippedEpoch==0);
  storageOk=false;assert(!alarmServiceSetEnabled(false));assert(alarmServiceSettings().enabled);
  now+=86400;step();assert(sound&&rings==2); // Still rings if storage fails.
  storageOk=true;step(5000);assert(saved.lastMinuteKey>settings.lastMinuteKey);
  assert(alarmServiceSetEnabled(false));assert(!sound&&!visible);
  assert(alarmServiceSetEnabled(true));now+=86400;step();assert(sound&&rings==3);
  step(600000);assert(!sound&&!visible&&!alarmServiceActive());
  puts("PASS: ring, dismiss, duplicate suppression, persistent skip/undo, failed saves, retry, master switch and timeout");
}
