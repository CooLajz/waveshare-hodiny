#include "../WaveshareHodiny/ScreenRotation.h"
#include <cassert>
#include <cstdio>
int main() {
  for (unsigned mask=0; mask<8; ++mask) {
    uint16_t durations[3] = {uint16_t(mask&1?120:0),uint16_t(mask&2?20:0),uint16_t(mask&4?30:0)};
    for (uint8_t current=0; current<3; ++current) {
      const auto next=nextRotationPage(current,durations);
      if (!mask) { assert(next==current); continue; }
      assert(durations[next]>0);
      if (!(mask&(mask-1))) {
        assert(next==((mask&1)?0:(mask&2)?1:2));
      } else {
        assert(next!=current);
        assert(next==((durations[(current+1)%3])?(current+1)%3:(current+2)%3));
      }
    }
  }
  puts("PASS: all zero, single page, skipped pages and every page combination");
}
