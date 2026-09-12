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

// Within one hour a moving seam only replaces one endpoint color with the
// other. Cross products classify its narrow wedge without atan2 per pixel.
struct ForecastFanSeamUpdate {
  bool active = false;
  float oldX = 0, oldY = 0, nextX = 0, nextY = 0;
  float middleX = 0, middleY = 0;
  int hour = 0, oldMinute = 0, nextMinute = 0;

  ForecastFanSeamUpdate(const ForecastFanKey &previous,
                       const ForecastFanKey &next, bool initialized) {
    if (!initialized || !previous.valid || !next.valid ||
        previous.stale != next.stale || previous.hour != next.hour ||
        previous.minute == next.minute) return;
    for (int i = 0; i < 12; ++i)
      if (previous.colors[i] != next.colors[i]) return;
    active = true;
    hour = next.hour; oldMinute = previous.minute; nextMinute = next.minute;
    const float oldAngle = (hour + oldMinute / 60.0f) * (3.14159265359f / 6);
    const float nextAngle = (hour + nextMinute / 60.0f) * (3.14159265359f / 6);
    oldX = std::sin(oldAngle); oldY = -std::cos(oldAngle);
    nextX = std::sin(nextAngle); nextY = -std::cos(nextAngle);
    middleX = oldX + nextX; middleY = oldY + nextY;
  }

  bool changes(int x, int y) const {
    const float dx = x - 240, dy = y - 240;
    if (dx * dx + dy * dy > 240 * 240) return false;
    const float a = oldX * dy - oldY * dx;
    const float b = nextX * dy - nextY * dx;
    // Near a ray use the original formula, preserving exact pixel ownership
    // despite floating-point rounding (including cardinal rays and center).
    if (std::fabs(a) < 0.002f || std::fabs(b) < 0.002f) {
      float angle = std::atan2(dy, dx) * 180 / 3.14159265359f + 90;
      if (angle < 0) angle += 360;
      const float position = angle / 30;
      if (static_cast<int>(position) % 12 != hour) return false;
      const float fraction = position - std::floor(position);
      return (fraction >= oldMinute / 60.0f) !=
             (fraction >= nextMinute / 60.0f);
    }
    return dx * middleX + dy * middleY > 0 && (a > 0) != (b > 0);
  }
};

inline uint32_t forecastFanEndpointColor(uint32_t value, bool stale, int x, int y) {
  static const uint8_t bayer[4][4] = {{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
  const float threshold = (bayer[y & 3][x & 3] + 0.5f) / 16;
  const auto channel = [&](int shift, int levels) -> uint32_t {
    const float scaled = ((value >> shift) & 255) * (stale ? 0.375f : 0.75f);
    const int quantized = static_cast<int>(scaled * levels / 255 + threshold);
    return (quantized * 255 + levels - 1) / levels;
  };
  return (channel(16, 31) << 16) | (channel(8, 63) << 8) | channel(0, 31);
}

// Resume a hidden render across UI iterations. If its inputs change midway,
// discard its progress: pixels may contain a mixture of both previous keys.
class ForecastFanBuild {
 public:
  bool pending = false;
  bool allowSeam = false;
  ForecastFanKey target{};
  ForecastFanRegion region{};
  int row = 0;

  void begin(const ForecastFanKey &rendered, const ForecastFanKey &next, bool initialized) {
    bool same = target.hour == next.hour && target.minute == next.minute &&
                target.valid == next.valid && target.stale == next.stale;
    for (int i = 0; i < 12; ++i) same = same && target.colors[i] == next.colors[i];
    if (pending && same) return;
    allowSeam = initialized && !pending;
    region = forecastFanRegion(rendered, next, allowSeam);
    target = next;
    row = region.y1;
    pending = true;
  }
  int endRow(int budget) const { return std::min(region.y2 + 1, row + budget); }
  bool advance(int end) {
    row = end;
    pending = row <= region.y2;
    return !pending;
  }
};
