#include <cassert>

#include "../WaveshareHodiny/DayNightLogic.h"

namespace {
constexpr int64_t HOUR = 60 * 60;
constexpr int64_t VALID_NOW = 1760000000;
}

int main() {
  assert(clockEvaluateSunDecision(false, 0, 60, 0, true,
                                  VALID_NOW - 2 * HOUR, true,
                                  VALID_NOW + 10 * HOUR) ==
         ClockSunDecision::Unavailable);

  assert(clockEvaluateSunDecision(false, 0, 60, VALID_NOW, true,
                                  VALID_NOW - 30 * 60, true,
                                  VALID_NOW + 10 * HOUR) ==
         ClockSunDecision::Day);
  assert(clockEvaluateSunDecision(false, 0, 60, VALID_NOW, true,
                                  VALID_NOW - 90 * 60, true,
                                  VALID_NOW + 10 * HOUR) ==
         ClockSunDecision::Night);
  assert(clockEvaluateSunDecision(false, 0, 60, VALID_NOW, false, 0, true,
                                  VALID_NOW + 10 * HOUR) ==
         ClockSunDecision::Unavailable);

  assert(clockEvaluateSunDecision(false, 0, 0, 0, false, 0, false, 0) ==
         ClockSunDecision::Night);
  assert(clockEvaluateSunDecision(true, 0, 0, 0, false, 0, false, 0) ==
         ClockSunDecision::Day);

  assert(clockEvaluateSunDecision(true, 0, -30, VALID_NOW, true,
                                  VALID_NOW - 10 * HOUR, true,
                                  VALID_NOW + 40 * 60) ==
         ClockSunDecision::Day);
  assert(clockEvaluateSunDecision(true, 0, -30, VALID_NOW, true,
                                  VALID_NOW - 10 * HOUR, true,
                                  VALID_NOW + 10 * 60) ==
         ClockSunDecision::Night);
  return 0;
}
