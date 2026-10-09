#include "../WaveshareHodiny/SlovakRadarRendering.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

int main() {
  using namespace slovakRadarRender;
  static_assert(COLOR_COUNT <= 256, "one byte per pixel");
  for (auto color : COLORS) assert(decode(encode(color)) == color);
  assert(encode(0) == 0);
  assert(edgeX(WEST) == 0 && edgeX(EAST) == 800);
  assert(edgeY(NORTH) == 0 && edgeY(SOUTH) == 550);
  for (double radius : {25., 50., 100., 260.}) {
    View v; v.set(49.13571, 20.43352, radius);
    assert(v.mapX(20.43352) >= 239 && v.mapX(20.43352) <= 240);
    // Mercator is not linear in latitude; all points must share raster geometry.
    for (int i = 0; i < 480; ++i) {
      const double sx = v.left + (i + .5) * (v.right-v.left) / 480;
      assert(v.sourceX(i) <= sx && sx < v.sourceX(i) + 1);
      const double lon = WEST + sx * (EAST-WEST) / 800;
      assert(v.mapX(lon) == i);
    }
  }
  puts("PASS: SHMU RGB565 palette roundtrip and shared pixel-center geometry");
}
