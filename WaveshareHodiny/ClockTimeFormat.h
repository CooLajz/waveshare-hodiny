#pragma once

inline int clockDisplayHour(unsigned hour, bool twelveHour) {
  return twelveHour ? (hour % 12 == 0 ? 12 : hour % 12) : hour;
}
inline const char *clockTimePeriod(unsigned hour) { return hour < 12 ? "AM" : "PM"; }
