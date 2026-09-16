#include "../WaveshareHodiny/TouchGestureTracker.h"
#include <cassert>
#include <cstdio>
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
  puts("Touch taps, hardware deduplication, swipes, jitter, long holds and rollover: OK");
}
