#pragma once

#include <stdint.h>

enum class ClockSunDecision : uint8_t {
  Unavailable = 0,
  Day,
  Night,
};

bool clockSelectCompletedTransitionTimestamp(
    bool lastChangedAvailable, int64_t lastChangedTimestamp,
    bool expectedTransitionAvailable, int64_t expectedTransitionTimestamp,
    int64_t &selectedTransitionTimestamp);

ClockSunDecision clockEvaluateSunDecision(
    bool horizonIsDay, int8_t sunriseOffsetMinutes,
    int8_t sunsetOffsetMinutes, int64_t nowTimestamp,
    bool completedTransitionAvailable, int64_t completedTransitionTimestamp,
    bool nextTransitionAvailable, int64_t nextTransitionTimestamp);
