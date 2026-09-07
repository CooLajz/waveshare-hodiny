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
