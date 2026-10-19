#include "../WaveshareHodiny/TouchGestureTracker.h"
#include <cassert>
#include <cstdio>
#include <array>
#include <initializer_list>
int main() {
  TouchGestureTracker tap;
  assert(tap.sample(true, 240, 240, 0, 100) == 0);
  assert(tap.sample(false, 240, 240, 0, 180) == 5);
  assert(tap.sample(false, 240, 240, 5, 190) == 0); // Late hardware duplicate.
  assert(tap.sample(false, 240, 240, 0, 200) == 0);
  assert(tap.sample(true, 240, 240, 0, 300) == 0);
  assert(tap.sample(false, 240, 240, 5, 360) == 5);
  assert(tap.sample(false, 240, 240, 5, 370) == 0);
  TouchGestureTracker swipe;
  assert(swipe.sample(true, 50, 50, 0, 0) == 0);
  assert(swipe.sample(true, 100, 50, 3, 50) == 3);
  assert(swipe.sample(false, 100, 50, 0, 100) == 0);
  TouchGestureTracker drift;
  drift.sample(true, 50, 50, 0, 0);
  drift.sample(true, 100, 50, 0, 50);
  assert(drift.sample(false, 50, 50, 0, 100) == 0); // Moved away and back.
  TouchGestureTracker hold;
  hold.sample(true, 20, 20, 0, 0);
  assert(hold.sample(false, 20, 20, 0, 1000) == 0);
  TouchGestureTracker jitter;
  jitter.sample(true, 240, 240, 0, 100);
  assert(jitter.sample(false, 250, 250, 0, 170) == 5);
  TouchGestureTracker rollover;
  rollover.sample(true, 20, 20, 0, UINT32_MAX - 40);
  assert(rollover.sample(false, 20, 20, 0, 40) == 5);
  TouchGestureTracker hardware;
  assert(hardware.sample(false, 0, 0, 5, 2000) == 5);
  assert(hardware.sample(false, 0, 0, 5, 2010) == 0);
  for (const auto &end : {std::array<int, 3>{100, 240, 3}, {380, 240, 4},
                         {240, 100, 2}, {240, 380, 1}}) {
    TouchGestureTracker gt911;
    gt911.sample(true, 240, 240, 0, 100, true);
    assert(gt911.sample(true, end[0], end[1], 0, 200, true) == 0);
    assert(gt911.sample(false, end[0], end[1], 0, 250, true) == end[2]);
    assert(gt911.sample(false, end[0], end[1], 0, 260, true) == 0);
  }
  TouchGestureTracker diagonal;
  diagonal.sample(true, 100, 100, 0, 100, true);
  assert(diagonal.sample(false, 250, 250, 0, 200, true) == 0);
  TouchGestureTracker slow;
  slow.sample(true, 100, 100, 0, 100, true);
  assert(slow.sample(false, 300, 100, 0, 1200, true) == 0);
  TouchGestureTracker gtTap;
  gtTap.sample(true, 240, 240, 0, 100, true);
  assert(gtTap.sample(false, 245, 245, 0, 180, true) == 5);
  TouchGestureTracker gtRollover;
  gtRollover.sample(true, 240, 240, 0, UINT32_MAX - 40, true);
  assert(gtRollover.sample(false, 100, 240, 0, 40, true) == 3);
  puts("Touch taps, hardware deduplication, swipes, jitter, long holds and rollover: OK");
}
