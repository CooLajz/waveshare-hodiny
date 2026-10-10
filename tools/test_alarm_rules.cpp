#include "../WaveshareHodiny/AlarmRules.h"
#include <cassert>
#include <cstdlib>
#include <cstdio>

time_t local(int y,int month,int day,int h,int m,int dst=-1) {
  tm t{};t.tm_year=y-1900;t.tm_mon=month-1;t.tm_mday=day;t.tm_hour=h;t.tm_min=m;t.tm_isdst=dst;
  return mktime(&t);
}
uint32_t key(time_t t) { tm l{};clockLocaltime(&t,&l);return alarmMinuteKey(l); }
int main() {
  setenv("TZ","CET-1CEST,M3.5.0,M10.5.0/3",1);tzset();
  assert(clockTimezoneSet("Europe/Bratislava"));
  AlarmSettings s;
  assert(alarmSettingsValid(s));assert(!alarmDue(s,local(2026,10,9,7,0)));
  s.entries[0]={420,31,1};
  auto fri=local(2026,10,9,7,0);
  assert(alarmDue(s,fri));assert(alarmDue(s,fri+59));assert(!alarmDue(s,fri+60));
  assert(alarmNext(s,fri+60)==local(2026,10,12,7,0));
  s.lastMinuteKey=key(fri);assert(!alarmDue(s,fri));
  s.skippedEpoch=local(2026,10,12,7,0);
  assert(alarmSkipPending(s, s.skippedEpoch - 1));
  assert(!alarmSkipPending(s, s.skippedEpoch));
  assert(!alarmSkipPending(s, s.skippedEpoch + 60));
  assert(!alarmSkipPending(s, 0));
  s.enabled=0; assert(!alarmSkipPending(s, s.skippedEpoch - 1)); s.enabled=1;
  assert(!alarmDue(s,s.skippedEpoch));
  assert(alarmNext(s,fri+60)==local(2026,10,13,7,0));
  AlarmSettings reboot=s;assert(!alarmDue(reboot,s.skippedEpoch));
  s.enabled=0;assert(!alarmDue(s,local(2026,10,13,7,0)));assert(!alarmNext(s,fri));
  s.enabled=1;s.entries[1]={390,64,1};
  assert(alarmNext(s,fri+60)==local(2026,10,11,6,30));
  s.entries[1].minute=1440;assert(!alarmSettingsValid(s));
  s=AlarmSettings{};s.entries[0]={150,64,1};
  // Spring gap: 02:30 does not exist on March 29.
  assert(alarmNext(s,local(2026,3,28,12,0))==local(2026,4,5,2,30));
  const auto first=local(2026,10,25,2,30,1), second=local(2026,10,25,2,30,0);
  assert(second-first==3600);assert(alarmDue(s,first));
  s.lastMinuteKey=key(first);assert(!alarmDue(s,second));
  s.lastMinuteKey=0;s.skippedEpoch=first;
  assert(!alarmDue(s,first));assert(!alarmDue(s,second));
  assert(alarmNext(s,first-60)==local(2026,11,1,2,30));
  s.skippedEpoch=0;s.entries[1]=s.entries[0];
  assert(alarmDue(s,first));s.lastMinuteKey=key(first);assert(!alarmDue(s,first+1));
  assert(!alarmDue(s,0));assert(!alarmNext(s,0));
  // Production regression: libc remains UTC while the clock uses Bratislava.
  setenv("TZ","UTC0",1);tzset();
  s=AlarmSettings{};s.entries[0]={420,127,1};
  const auto utcFive=local(2026,10,9,5,0);
  assert(alarmDue(s,utcFive));
  assert(alarmNext(s,utcFive-60)==utcFive);
  assert(clockTimezoneSet("Asia/Kathmandu"));s.entries[0].minute=645;
  assert(alarmDue(s,utcFive));assert(alarmNext(s,utcFive-60)==utcFive);
  assert(clockTimezoneSet("Europe/Bratislava"));s.entries[0].minute=420;
  assert(alarmNext(s,local(2026,12,31,8,0))==local(2027,1,1,6,0));
  assert(alarmNext(s,local(2028,2,28,8,0))==local(2028,2,29,6,0));
  assert(alarmNext(s,local(2028,2,29,8,0))==local(2028,3,1,6,0));
  // Skipping a weekly alarm can push the next occurrence almost two weeks out.
  s.entries[0]={420,16,1};s.skippedEpoch=local(2026,10,16,5,0);
  assert(alarmNext(s,local(2026,10,9,8,0))==local(2026,10,23,5,0));
  puts("PASS: weekday schedules, multiple alarms, skip/reboot, disable, DST gap/fold, duplicate suppression and invalid time");
}
