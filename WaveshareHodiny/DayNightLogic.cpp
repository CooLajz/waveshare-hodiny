#include "DayNightLogic.h"

namespace {
constexpr int64_t VALID_TIME_THRESHOLD = 1700000000LL;
}

ClockSunDecision clockEvaluateSunDecision(
    bool horizonIsDay, int8_t sunriseOffsetMinutes,
    int8_t sunsetOffsetMinutes, int64_t nowTimestamp,
    bool lastChangedAvailable, int64_t lastChangedTimestamp,
    bool nextTransitionAvailable, int64_t nextTransitionTimestamp) {
  const int8_t completedOffset =
      horizonIsDay ? sunriseOffsetMinutes : sunsetOffsetMinutes;
  const int8_t upcomingOffset =
      horizonIsDay ? sunsetOffsetMinutes : sunriseOffsetMinutes;
  const bool needsCurrentTime = completedOffset > 0 || upcomingOffset < 0;
  if (needsCurrentTime && nowTimestamp < VALID_TIME_THRESHOLD) {
    return ClockSunDecision::Unavailable;
  }
  if ((completedOffset > 0 && !lastChangedAvailable) ||
      (upcomingOffset < 0 && !nextTransitionAvailable)) {
    return ClockSunDecision::Unavailable;
  }

  bool isDay = horizonIsDay;
  if (completedOffset > 0 && lastChangedAvailable &&
      nowTimestamp < lastChangedTimestamp + completedOffset * 60LL) {
    isDay = !isDay;
  } else if (upcomingOffset < 0 && nextTransitionAvailable &&
             nowTimestamp >=
                 nextTransitionTimestamp + upcomingOffset * 60LL) {
    isDay = !isDay;
  }
  return isDay ? ClockSunDecision::Day : ClockSunDecision::Night;
}
