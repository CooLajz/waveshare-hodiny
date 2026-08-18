#pragma once

#include <stdint.h>

enum class ClockSunDecision : uint8_t {
  Unavailable = 0,
  Day,
  Night,
};

ClockSunDecision clockEvaluateSunDecision(
    bool horizonIsDay, int8_t sunriseOffsetMinutes,
    int8_t sunsetOffsetMinutes, int64_t nowTimestamp,
    bool lastChangedAvailable, int64_t lastChangedTimestamp,
    bool nextTransitionAvailable, int64_t nextTransitionTimestamp);
