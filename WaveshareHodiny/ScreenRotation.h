#pragma once
#include <stdint.h>
// Order matches horizontal navigation: clock, forecast, radar.
inline uint8_t nextRotationPage(uint8_t current, const uint16_t seconds[3]) {
  for (uint8_t step = 1; step <= 3; ++step) {
    const uint8_t candidate = (current + step) % 3;
    if (seconds[candidate] != 0) return candidate;
  }
  return current; // All zero: retain the current page, even with rotation enabled.
}
