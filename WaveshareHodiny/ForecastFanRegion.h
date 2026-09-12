#pragma once
#include <algorithm>
#include <cmath>
#include <stdint.h>

struct ForecastFanKey {
  uint32_t colors[12]{};
  int hour = 0;
  int minute = 0;
  bool valid = false;
  bool stale = true;
};

struct ForecastFanRegion {
  int x1 = 480, y1 = 480, x2 = -1, y2 = -1;
};

// Each interpolated sector depends only on its two endpoint colors and,
// for the current hour, the moving seam. Retain all other cached pixels.
inline ForecastFanRegion forecastFanRegion(const ForecastFanKey &previous,
                                           const ForecastFanKey &next,
                                           bool initialized) {
  if (!initialized || previous.valid != next.valid || previous.stale != next.stale)
    return {0, 0, 479, 479};
  ForecastFanRegion region;
  for (int sector = 0; sector < 12; ++sector) {
    const bool seamChanged = next.valid &&
        (previous.hour != next.hour || previous.minute != next.minute) &&
        (sector == previous.hour || sector == next.hour);
    if (!seamChanged && previous.colors[sector] == next.colors[sector] &&
        previous.colors[(sector + 1) % 12] == next.colors[(sector + 1) % 12])
      continue;
    region.x1 = std::min(region.x1, 240);
    region.y1 = std::min(region.y1, 240);
    region.x2 = std::max(region.x2, 240);
    region.y2 = std::max(region.y2, 240);
    // A 30-degree sector cannot have a cardinal extremum in its interior.
    // Two extra pixels conservatively cover floating-point edge rounding.
    for (int end = sector; end <= sector + 1; ++end) {
      const float angle = end * (3.14159265359f / 6);
      const int x = static_cast<int>(std::lround(240 + 240 * std::sin(angle)));
      const int y = static_cast<int>(std::lround(240 - 240 * std::cos(angle)));
      region.x1 = std::min(region.x1, std::max(0, x - 2));
      region.y1 = std::min(region.y1, std::max(0, y - 2));
      region.x2 = std::max(region.x2, std::min(479, x + 2));
      region.y2 = std::max(region.y2, std::min(479, y + 2));
    }
  }
  return region;
}
