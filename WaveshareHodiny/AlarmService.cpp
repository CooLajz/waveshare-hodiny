#include "AlarmService.h"
#include "BuzzerService.h"
#include "DisplayNotification.h"
#include <Arduino.h>

namespace {
AlarmSettings settings;
AlarmSaveCallback saveSettings = nullptr;
bool ringing = false, english = false, stoppedRecently = false;
uint32_t stoppedAt = 0;
uint32_t startedAt = 0, lastCheck = 0, consumedKey = 0;
bool commit(const AlarmSettings &value) {
  if (!saveSettings || !saveSettings(value)) return false;
  settings = value;
  return true;
}
}
void alarmServiceBegin(AlarmSaveCallback save) { saveSettings = save; }
void alarmServiceApply(const AlarmSettings &value, bool en) {
  settings = value; english = en;
  if (settings.lastMinuteKey > consumedKey) consumedKey = settings.lastMinuteKey;
  if (!settings.enabled) alarmServiceDismiss();
}
const AlarmSettings &alarmServiceSettings() { return settings; }
bool alarmServiceActive() { return ringing; }
bool alarmServiceBlocksTouch() { return ringing || (stoppedRecently && millis() - stoppedAt < 350); }
void alarmServiceDismiss() {
  if (!ringing) return;
  ringing = false;
  stoppedRecently = true; stoppedAt = millis();
  buzzerServiceAlarm(false);
}
bool alarmServiceSetEnabled(bool enabled) {
  AlarmSettings next = settings; next.enabled = enabled;
  if (!commit(next)) return false;
  if (!enabled) alarmServiceDismiss();
  return true;
}
bool alarmServiceSkipNext() {
  AlarmSettings next = settings;
  // One pending skip; another press restores that occurrence.
  if (next.skippedEpoch > time(nullptr)) next.skippedEpoch = 0;
  else {
    next.skippedEpoch = alarmNext(settings, time(nullptr));
    if (!next.skippedEpoch) return false;
  }
  return commit(next);
}
void alarmServiceDescribeNext(char *text, unsigned size) {
  if (!settings.enabled) { snprintf(text,size,"%s",english?"Alarms off":"BUDÍKY VYPNUTÉ"); return; }
  const time_t now = time(nullptr);
  if (now < 1704067200) { snprintf(text,size,"%s",english?"Waiting for time":"ČEKÁNÍ NA ČAS"); return; }
  const time_t next = alarmNext(settings, now);
  if (!next) { snprintf(text,size,"%s",english?"No alarm scheduled":"ŽÁDNÝ DALŠÍ BUDÍK"); return; }
  tm local{}; clockLocaltime(&next,&local);
  const char *cs[] = {"NE","PO","ÚT","ST","ČT","PÁ","SO"};
  const char *en[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  snprintf(text,size,"%s %d.%d. %02d:%02d",(english?en:cs)[local.tm_wday],local.tm_mday,local.tm_mon+1,local.tm_hour,local.tm_min);
}
void alarmServiceLoop() {
  if (ringing && millis() - startedAt >= 10UL * 60 * 1000) alarmServiceDismiss();
  if (millis() - lastCheck < 250) return;
  lastCheck = millis();
  static uint32_t lastSaveRetry = 0;
  if (consumedKey > settings.lastMinuteKey && millis() - lastSaveRetry >= 5000) {
    lastSaveRetry = millis();
    AlarmSettings retry = settings; retry.lastMinuteKey = consumedKey; commit(retry);
  }
  const time_t now = time(nullptr);
  AlarmSettings check = settings;
  if (consumedKey > check.lastMinuteKey) check.lastMinuteKey = consumedKey;
  if (!alarmDue(check, now)) return;
  tm local{}; clockLocaltime(&now,&local);
  consumedKey = alarmMinuteKey(local);
  check.lastMinuteKey = consumedKey;
  // Ring even if flash is temporarily unavailable; RAM still prevents repeats.
  commit(check);
  if (ringing) return; // Simultaneous alarms share one ringing session.
  displayNotificationDismiss(); // The clock face remains visible while ringing.
  ringing = true;
  startedAt = millis();
  buzzerServiceAlarm(true);
}
