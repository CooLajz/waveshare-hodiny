#include <cassert>

#include "../WaveshareHodiny/DayNightLogic.h"

namespace {
constexpr int64_t HOUR = 60 * 60;
constexpr int64_t VALID_NOW = 1760000000;
}

int main() {
  int64_t selectedTransition = 0;
  const int64_t expectedSunrise = VALID_NOW - 3 * HOUR;
  assert(clockSelectCompletedTransitionTimestamp(
      true, VALID_NOW - 15 * 60, true, expectedSunrise,
      selectedTransition));
  assert(selectedTransition == expectedSunrise);
  assert(clockEvaluateSunDecision(true, 30, 0, VALID_NOW, true,
                                  selectedTransition, true,
                                  VALID_NOW + 10 * HOUR) ==
         ClockSunDecision::Day);

  const int64_t expectedSunset = VALID_NOW - 4 * HOUR;
  assert(clockSelectCompletedTransitionTimestamp(
      true, VALID_NOW - 10 * 60, true, expectedSunset,
      selectedTransition));
  assert(selectedTransition == expectedSunset);
  assert(clockEvaluateSunDecision(false, 0, 60, VALID_NOW, true,
                                  selectedTransition, true,
                                  VALID_NOW + 8 * HOUR) ==
         ClockSunDecision::Night);

  assert(clockSelectCompletedTransitionTimestamp(
      true, expectedSunrise + 2 * 60, true, expectedSunrise,
      selectedTransition));
  assert(selectedTransition == expectedSunrise + 2 * 60);
  assert(clockSelectCompletedTransitionTimestamp(
      true, expectedSunrise + 6 * 60, true, expectedSunrise,
      selectedTransition));
  assert(selectedTransition == expectedSunrise);
  assert(clockSelectCompletedTransitionTimestamp(
      true, expectedSunrise + 2 * 60, true, expectedSunrise,
      selectedTransition));
  assert(clockEvaluateSunDecision(true, 30, 0,
                                  expectedSunrise + 20 * 60, true,
                                  selectedTransition, true,
                                  VALID_NOW + 10 * HOUR) ==
         ClockSunDecision::Night);
  assert(clockEvaluateSunDecision(true, 30, 0,
                                  expectedSunrise + 35 * 60, true,
                                  selectedTransition, true,
                                  VALID_NOW + 10 * HOUR) ==
         ClockSunDecision::Day);

  assert(!clockSelectCompletedTransitionTimestamp(
      true, VALID_NOW, false, 0, selectedTransition));

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
