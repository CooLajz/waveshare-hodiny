#pragma once
#include <stdint.h>
#include <time.h>
#include "ClockTimezone.h"

constexpr unsigned CLOCK_ALARM_COUNT = 12;
struct ClockAlarm {
  uint16_t minute = 420;
  uint8_t days = 0; // Monday = bit 0. Zero means an unused slot.
  uint8_t enabled = 0;
};
struct AlarmSettings {
  uint8_t enabled = 1;
  uint8_t reserved[3] = {};
  uint32_t skippedEpoch = 0;
  uint32_t lastMinuteKey = 0;
  ClockAlarm entries[CLOCK_ALARM_COUNT];
};
inline bool alarmSkipPending(const AlarmSettings &s, time_t now) {
  return s.enabled && now >= 1704067200 && s.skippedEpoch > now;
}
inline bool alarmSettingsValid(const AlarmSettings &s) {
  if (s.enabled > 1) return false;
  for (const auto &a : s.entries)
    if (a.minute >= 1440 || a.days > 127 || a.enabled > 1 || (a.enabled && !a.days)) return false;
  return true;
}
inline uint32_t alarmMinuteKey(const tm &t) {
  return ((t.tm_year * 366U + t.tm_yday) * 1440U) + t.tm_hour * 60U + t.tm_min;
}
inline bool alarmMatches(const ClockAlarm &a, const tm &t) {
  return a.enabled && (a.days & (1U << ((t.tm_wday + 6) % 7))) &&
      a.minute == t.tm_hour * 60 + t.tm_min;
}
inline bool alarmSkipped(const AlarmSettings &s, const tm &local) {
  if (!s.skippedEpoch) return false;
  const time_t skipped = s.skippedEpoch;
  tm skippedLocal{}; clockLocaltime(&skipped, &skippedLocal);
  return alarmMinuteKey(local) == alarmMinuteKey(skippedLocal);
}
inline bool alarmDue(const AlarmSettings &s, time_t now) {
  if (!s.enabled || now < 1704067200) return false;
  tm local{}; clockLocaltime(&now, &local);
  if (alarmMinuteKey(local) <= s.lastMinuteKey ||
      alarmSkipped(s, local)) return false;
  for (const auto &a : s.entries) if (alarmMatches(a, local)) return true;
  return false;
}
// Calendar arithmetic uses the device's IANA database, including DST. A nonexistent
// spring-forward time is omitted; a repeated autumn minute rings only once.
inline time_t alarmNext(const AlarmSettings &s, time_t now) {
  if (!s.enabled || now < 1704067200) return 0;
  tm date{}; clockLocaltime(&now, &date);
  time_t best = 0;
  for (int day = 0; day <= 15; ++day) {
    for (const auto &a : s.entries) {
      if (!a.enabled || !(a.days & (1U << ((date.tm_wday + 6) % 7)))) continue;
      tm candidate = date; candidate.tm_hour = a.minute / 60;
      candidate.tm_min = a.minute % 60; candidate.tm_sec = 0;
      time_t epoch = 0;
      if (!clockLocaltimeInverse(candidate, epoch)) continue;
      clockLocaltime(&epoch, &candidate);
      if (epoch < now || alarmSkipped(s, candidate) ||
          alarmMinuteKey(candidate) <= s.lastMinuteKey) continue;
      if (!best || epoch < best) best = epoch;
    }
    const int year = date.tm_year + 1900;
    const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    const int monthDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (++date.tm_mday > monthDays[date.tm_mon] + (date.tm_mon == 1 && leap)) {
      date.tm_mday = 1;
      if (++date.tm_mon == 12) { date.tm_mon = 0; ++date.tm_year; }
    }
    date.tm_wday = (date.tm_wday + 1) % 7;
  }
  return best;
}
