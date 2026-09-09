#pragma once

#include <cmath>
#include <algorithm>

inline float retroLcdProgressPercent(float value, float minimum, float maximum) {
  if (!std::isfinite(value) || !std::isfinite(minimum) || !std::isfinite(maximum) || minimum >= maximum) return NAN;
  const double percent = (static_cast<double>(value) - minimum) / (static_cast<double>(maximum) - minimum) * 100.0;
  return static_cast<float>(std::max(0.0, std::min(100.0, percent)));
}


#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

// Počet pozic před tečkou zahrnuje pouze číslice; znaménko má vlastní segment.
// Přesah po zaokrouhlení a nedostupná data zobrazíme pomlčkami, nikdy ořezem.
inline void retroLcdFormatValue(char *out, size_t capacity, float value,
                                unsigned places, unsigned decimals, bool *negative = nullptr) {
  if (negative) *negative = false;
  places = std::max(1U, std::min(4U, places));
  decimals = std::min(2U, decimals);
  const size_t length = places + (decimals ? decimals + 1 : 0);
  if (capacity <= length) { if (capacity) out[0] = '\0'; return; }
  std::memset(out, '-', length);
  if (decimals) out[places] = '.';
  out[length] = '\0';
  if (!std::isfinite(value) || std::fabs(value) >= 10000000) return;
  char formatted[32];
  std::snprintf(formatted, sizeof(formatted), "%.*f", static_cast<int>(decimals), std::fabs(value));
  const char *dot = std::strchr(formatted, '.');
  const size_t integerLength = dot ? static_cast<size_t>(dot - formatted) : std::strlen(formatted);
  if (integerLength > places) return;
  if (negative) *negative = value < 0 && std::strspn(formatted, "0.") != std::strlen(formatted);
  const size_t padding = places - integerLength;
  std::memset(out, ' ', padding);
  std::memcpy(out + padding, formatted, std::strlen(formatted) + 1);
}

// Fixed LCD character positions; an unmatched spare position stays on the right.
inline void retroLcdFormatWeekday(char *out, size_t capacity, int weekday, bool english, bool fixed = true) {
  static const char *const cz[] = {"NEDELE","PONDELI","UTERY","STREDA","CTVRTEK","PATEK","SOBOTA"};
  static const char *const en[] = {"SUNDAY","MONDAY","TUESDAY","WEDNESDAY","THURSDAY","FRIDAY","SATURDAY"};
  const auto &names = english ? en : cz;
  size_t places = 0;
  for (const char *name : names) places = std::max(places, std::strlen(name));
  if (!fixed && weekday >= 0 && weekday < 7) places = std::strlen(names[weekday]);
  if (!fixed && (weekday < 0 || weekday >= 7)) places = 7;
  if (!capacity) return;
  if (capacity <= places) { out[0] = '\0'; return; }
  std::memset(out, weekday >= 0 && weekday < 7 ? ' ' : '-', places);
  out[places] = '\0';
  if (weekday < 0 || weekday >= 7) return;
  const size_t length = std::strlen(names[weekday]);
  std::memcpy(out + (places - length) / 2, names[weekday], length);
}

// All date layouts reserve two day/month positions, even with hidden zeros.
inline void retroLcdFormatDate(char *out, size_t capacity, int day, int month, int year,
                              unsigned format, bool available = true) {
  if (!capacity) return;
  if (capacity < 11) { out[0] = '\0'; return; }
  if (format > 13) format = 0;
  const bool leadingZeros = format < 7;
  format %= 7;
  const char separator = format == 0 ? '.' : (format == 2 || format == 4 || format == 6 ? '/' : '-');
  char d[3], m[3], y[5];
  if (available && day >= 1 && day <= 31 && month >= 1 && month <= 12 && year >= 0 && year <= 9999) {
    std::snprintf(d,sizeof(d),leadingZeros ? "%02d" : "%2d",day);
    std::snprintf(m,sizeof(m),leadingZeros ? "%02d" : "%2d",month);
    std::snprintf(y,sizeof(y),"%04d",year);
  } else {
    std::strcpy(d,"--"); std::strcpy(m,"--"); std::strcpy(y,"----");
  }
  if (format >= 5) std::snprintf(out,capacity,"%s%c%s%c%s",y,separator,m,separator,d);
  else if (format >= 3) std::snprintf(out,capacity,"%s%c%s%c%s",m,separator,d,separator,y);
  else std::snprintf(out,capacity,"%s%c%s%c%s",d,separator,m,separator,y);
}
