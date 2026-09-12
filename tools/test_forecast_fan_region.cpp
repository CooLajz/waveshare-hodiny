#include "../WaveshareHodiny/ForecastFanRegion.h"
#include <cassert>
#include <cstdio>
#include <vector>

// Reference pixel formula from the original full-frame renderer.
static uint16_t pixel(const ForecastFanKey &key, int x, int y) {
  const float dx = x - 240, dy = y - 240;
  if (std::sqrt(dx * dx + dy * dy) > 240) return 0;
  float angle = std::atan2(dy, dx) * 180 / 3.14159265359f + 90;
  if (angle < 0) angle += 360;
  const float position = angle / 30;
  const int sector = static_cast<int>(position) % 12;
  float fraction = position - std::floor(position);
  if (key.valid && sector == key.hour)
    fraction = fraction >= key.minute / 60.0f ? 1.0f : 0.0f;
  const uint32_t left = key.colors[sector], right = key.colors[(sector + 1) % 12];
  static const uint8_t bayer[4][4] = {{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
  const float threshold = (bayer[y & 3][x & 3] + 0.5f) / 16;
  const auto channel = [&](int shift, int levels) -> uint8_t {
    const float a = (left >> shift) & 255, b = (right >> shift) & 255;
    const float value = (a + (b - a) * fraction) * (key.stale ? 0.375f : 0.75f);
    const int quantized = static_cast<int>(value * levels / 255 + threshold);
    return static_cast<uint8_t>((quantized * 255 + levels - 1) / levels);
  };
  return ((channel(16, 31) >> 3) << 11) |
         ((channel(8, 63) >> 2) << 5) | (channel(0, 31) >> 3);
}

int main() {
  ForecastFanKey previous{};
  bool initialized = false;
  std::vector<uint16_t> frame(480 * 480, 0xFFFF);
  unsigned checked = 0;
  const auto verify = [&](const ForecastFanKey &next) {
    const auto region = forecastFanRegion(previous, next, initialized);
    for (int y = region.y1; y <= region.y2; ++y)
      for (int x = region.x1; x <= region.x2; ++x)
        frame[y * 480 + x] = pixel(next, x, y);
    for (int y = 0; y < 480; ++y)
      for (int x = 0; x < 480; ++x) {
        assert(frame[y * 480 + x] == pixel(next, x, y));
        const float dx = x - 240, dy = y - 240;
        assert((std::sqrt(dx * dx + dy * dy) <= 240) ==
               (dx * dx + dy * dy <= 240 * 240));
      }
    previous = next;
    initialized = true;
    ++checked;
  };
  ForecastFanKey next;
  for (int i = 0; i < 12; ++i) next.colors[i] = (0x214785U * (i + 1)) & 0xFFFFFF;
  verify(next); // No synchronized time.
  next.valid = true;
  next.stale = false;
  for (int hour = 0; hour < 12; ++hour) {
    next.hour = hour;
    for (int minute = 0; minute < 60; ++minute) {
      next.minute = minute;
      verify(next);
    }
  }
  for (int i = 0; i < 12; ++i) {
    next.colors[i] ^= 0xDEADBE;
    verify(next); // Both neighboring sectors must change.
  }
  next.stale = true; verify(next);
  next.valid = false; verify(next);
  for (auto &color : next.colors) color = 0xC83020;
  verify(next); // Night palette.
  const auto empty = forecastFanRegion(next, next, true);
  assert(empty.x1 > empty.x2 && empty.y1 > empty.y2);
  auto minuteLater = next;
  next.valid = minuteLater.valid = true;
  next.minute = 1; minuteLater.minute = 2;
  const auto small = forecastFanRegion(next, minuteLater, true);
  assert((small.x2 - small.x1 + 1) * (small.y2 - small.y1 + 1) < 480 * 480 / 4);
  printf("PASS: %u full-frame comparisons (all hours/minutes, data, stale/night/time changes)\n", checked);
}
