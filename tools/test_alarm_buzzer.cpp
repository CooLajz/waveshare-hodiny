#include <cassert>
#include <cstdio>
#include "../WaveshareHodiny/AlarmBuzzerPattern.h"
int main() {
  // Every audible pulse remains complete, including both tempo transitions.
  unsigned pulseStart = 0, pulses = 0;
  bool previous = false;
  for (unsigned ms = 0; ms <= 600000; ++ms) {
    const bool on = alarmBuzzerOn(ms);
    if (on && !previous) { pulseStart = ms; ++pulses; }
    if (!on && previous) assert(ms - pulseStart == 140);
    previous = on;
  }
  assert(pulses > 100);
  for (unsigned ms = 16800; ms < 17400; ++ms) assert(!alarmBuzzerOn(ms));
  for (unsigned ms = 46200; ms < 46800; ++ms) assert(!alarmBuzzerOn(ms));
  assert(alarmBuzzerOn(17400)); assert(alarmBuzzerOn(46800));
  assert(!alarmBuzzerOn(600000));
  puts("PASS: complete pulses, quiet tempo transitions, restart at first pulse and cutoff");
}
