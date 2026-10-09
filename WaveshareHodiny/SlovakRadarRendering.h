#pragma once
#include <cstdint>
#include <cstddef>
#include <cmath>

// Public color scale: https://www.shmu.sk/img/radary/data.cmax.png
// Samples every 0.5 dBZ (0..85), converted to RGB565; extra map colors.
// Prepared SHMU frames keep one byte/pixel without RGB332 color loss.
namespace slovakRadarRender {
constexpr double WEST = 13.6, EAST = 23.79, SOUTH = 46.05, NORTH = 50.7;
constexpr uint16_t COLORS[] = {
  0x0000, 0x0255, 0x0254, 0x0274, 0x0294, 0x02b3, 0x02d3, 0x02f3, 0x0312, 0x0332, 0x0351, 0x0371,
  0x0390, 0x03b0, 0x03d0, 0x03ef, 0x040f, 0x042e, 0x044e, 0x046d, 0x048d, 0x04ad, 0x04cc, 0x04eb,
  0x050b, 0x052a, 0x054a, 0x0569, 0x0588, 0x05a8, 0x05e7, 0x0606, 0x0626, 0x0645, 0x0684, 0x06a4,
  0x06c3, 0x06e2, 0x0722, 0x0741, 0x0761, 0x0780, 0x07a0, 0x0fa0, 0x1fa0, 0x27a0, 0x37a0, 0x3f80,
  0x4f80, 0x5780, 0x6780, 0x6f60, 0x7f60, 0x8f60, 0x9760, 0xa740, 0xaf40, 0xbf40, 0xc740, 0xd720,
  0xdf20, 0xef20, 0xf700, 0xfee0, 0xfec0, 0xfea0, 0xfe80, 0xfe60, 0xfe40, 0xfe20, 0xfe00, 0xfde0,
  0xfdc0, 0xfda0, 0xfd80, 0xfd60, 0xfd40, 0xfd00, 0xfce0, 0xfcc0, 0xfc80, 0xfc40, 0xfc20, 0xfbe0,
  0xfba0, 0xfb60, 0xfb20, 0xfae0, 0xfaa0, 0xfa60, 0xfa20, 0xf9e0, 0xf9a0, 0xf960, 0xf920, 0xf8e0,
  0xf8a0, 0xf860, 0xf040, 0xe840, 0xe040, 0xd840, 0xd040, 0xc840, 0xc060, 0xb860, 0xb060, 0xa860,
  0xa060, 0x9860, 0x9880, 0x9080, 0x90a1, 0x90a3, 0x90c4, 0x90e6, 0x9907, 0x9929, 0xa12a, 0xa14c,
  0xa96d, 0xa98f, 0xb1b1, 0xb1d2, 0xb9d4, 0xb9f5, 0xc216, 0xc237, 0xca58, 0xca59, 0xd279, 0xd29a,
  0xdabb, 0xdadb, 0xe2fc, 0xeafc, 0xeb1d, 0xeb3d, 0xf35e, 0xf37e, 0xf37f, 0xfb9f, 0xfbbf, 0xfbdf,
  0xfbff, 0xfc1f, 0xfc3f, 0xfc5f, 0xfc7f, 0xfc9f, 0xfcbf, 0xfcdf, 0xfcff, 0xfd1f, 0xfd3f, 0xfd5f,
  0xfd7f, 0xfd9f, 0xffff, 0xbdf7, 0x07ff, 0x4208, 0x0841, 0x1082, 0x18c3, 0x2104, 0x2945, 0x3186,
  0x39c7, 0x4a49, 0x528a, 0x5acb, 0x630c, 0x6b4d, 0x738e, 0x7bcf, 0x8410, 0x8c51, 0x9492, 0x9cd3,
  0xa514, 0xad55, 0xb596, 0xbdd7, 0xc618, 0xce59, 0xd69a, 0xdedb, 0xe71c, 0xef5d, 0xf79e, 0x0082,
  0x0104, 0x0186, 0x0208, 0x02aa, 0x032c, 0x03ae, 0x0430, 0x04b2, 0x0554, 0x05d6, 0x0658, 0x06da,
  0x075c,
};
constexpr size_t COLOR_COUNT = sizeof(COLORS) / sizeof(COLORS[0]);
constexpr uint8_t SORTED[] = {
0, 191, 192, 193, 194, 2, 1, 3, 4, 195, 5, 6, 7, 8, 196, 9, 10, 11, 12, 197, 13, 14, 15, 16, 17, 198, 18, 19, 20, 21, 199, 22, 23, 24, 25, 26, 200, 27, 28, 29, 201, 30, 31, 32, 33, 202, 34, 35, 36, 203, 37, 38, 39, 204, 40, 41, 42, 160, 162, 43, 163, 164, 44, 165, 45, 166, 167, 46, 168, 47, 161, 169, 48, 170, 49, 171, 172, 50, 173, 51, 174, 175, 52, 176, 177, 53, 111, 112, 113, 114, 115, 178, 54, 109, 110, 116, 117, 179, 108, 118, 119, 180, 55, 107, 120, 121, 181, 56, 106, 122, 123, 182, 105, 124, 125, 183, 159, 57, 104, 126, 127, 184, 58, 103, 128, 129, 185, 102, 130, 131, 186, 59, 101, 132, 133, 187, 60, 100, 134, 188, 99, 135, 136, 137, 61, 189, 98, 138, 139, 140, 62, 190, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87, 86, 85, 141, 84, 142, 143, 83, 144, 145, 82, 146, 81, 147, 148, 80, 149, 150, 79, 151, 78, 152, 77, 153, 154, 76, 155, 75, 156, 74, 157, 73, 72, 71, 70, 69, 68, 67, 66, 65, 64, 63, 158
};
inline uint16_t decode(uint8_t index) { return index < COLOR_COUNT ? COLORS[index] : 0; }
inline uint8_t encode(uint16_t color) {
  size_t low = 0, high = COLOR_COUNT;
  while (low < high) {
    const size_t mid = (low + high) / 2;
    if (COLORS[SORTED[mid]] < color) low = mid + 1; else high = mid;
  }
  if (low < COLOR_COUNT && COLORS[SORTED[low]] == color) return SORTED[low];
  // Map transparency can introduce intermediate colors: choose closest palette entry.
  uint32_t best = UINT32_MAX;
  uint8_t selected = 0;
  for (size_t i = 0; i < COLOR_COUNT; ++i) {
    const int dr = int(color >> 11) - int(COLORS[i] >> 11);
    const int dg = int((color >> 5) & 63) - int((COLORS[i] >> 5) & 63);
    const int db = int(color & 31) - int(COLORS[i] & 31);
    const uint32_t distance = 4 * dr * dr + dg * dg + 4 * db * db;
    if (distance < best) { best = distance; selected = uint8_t(i); }
  }
  return selected;
}
inline double mercator(double lat) { return log(tan(0.7853981633974483 + lat * 0.008726646259971648)); }
inline double edgeX(double lon) { return (lon - WEST) * 800 / (EAST - WEST); }
inline double edgeY(double lat) { return (mercator(NORTH) - mercator(lat)) * 550 / (mercator(NORTH) - mercator(SOUTH)); }
struct View {
  double left, right, top, bottom;
  void set(double lat, double lon, double radiusKm) {
    const double dy = radiusKm / 111.32;
    const double dx = dy / cos(lat * 0.0174532925199433);
    left = edgeX(lon - dx); right = edgeX(lon + dx);
    top = edgeY(lat + dy); bottom = edgeY(lat - dy);
  }
  int sourceX(int x) const { return int(floor(left + (x + 0.5) * (right-left) / 480)); }
  int sourceY(int y) const { return int(floor(top + (y + 0.5) * (bottom-top) / 480)); }
  int mapX(double lon) const { return int(lround((edgeX(lon)-left) * 480 / (right-left) - 0.5)); }
  int mapY(double lat) const { return int(lround((edgeY(lat)-top) * 480 / (bottom-top) - 0.5)); }
};
}  // namespace slovakRadarRender
