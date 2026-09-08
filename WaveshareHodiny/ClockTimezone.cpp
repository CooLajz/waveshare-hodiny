#include "ClockTimezone.h"

#include <atomic>
#include <string.h>

namespace {
struct ClockTimezoneTransition {
  uint32_t at;
  int32_t offset;
  bool daylight;
};
struct ClockTimezoneRules {
  const ClockTimezoneTransition *transitions;
  uint16_t count;
};
struct ClockTimezoneName {
  const char *name;
  uint16_t rules;
};
#include "ClockTimezoneData.h"

const ClockTimezoneRules *findRules(const char *name) {
  if (!name || !*name) return nullptr;
  size_t low = 0, high = sizeof(timezoneNames) / sizeof(timezoneNames[0]);
  while (low < high) {
    const size_t middle = low + (high - low) / 2;
    const int comparison = strcmp(name, timezoneNames[middle].name);
    if (comparison == 0) return &timezoneRules[timezoneNames[middle].rules];
    if (comparison < 0) high = middle;
    else low = middle + 1;
  }
  return nullptr;
}

// Immutable flash data, atomically selected for the UI and network tasks.
std::atomic<const ClockTimezoneRules *> activeRules{nullptr};

const ClockTimezoneTransition &transitionAt(const ClockTimezoneRules &rules,
                                          time_t timestamp) {
  size_t low = 0, high = rules.count;
  while (low < high) {
    const size_t middle = low + (high - low) / 2;
    if (static_cast<int64_t>(timestamp) < rules.transitions[middle].at)
      high = middle;
    else low = middle + 1;
  }
  return rules.transitions[low == 0 ? 0 : low - 1];
}

const ClockTimezoneRules &currentRules() {
  const auto *rules = activeRules.load();
  return rules ? *rules : *findRules("Europe/Prague");
}
}  // namespace

bool clockTimezoneSupported(const char *name) { return findRules(name) != nullptr; }

bool clockTimezoneSet(const char *name) {
  const auto *rules = findRules(name && *name ? name : "Europe/Prague");
  if (!rules) return false;
  activeRules.store(rules);
  return true;
}

struct tm *clockLocaltime(const time_t *timestamp, struct tm *result) {
  if (!timestamp || !result) return nullptr;
  const auto &transition = transitionAt(currentRules(), *timestamp);
  const time_t shifted = *timestamp + transition.offset;
  if (!gmtime_r(&shifted, result)) return nullptr;
  result->tm_isdst = transition.daylight ? 1 : 0;
  return result;
}

bool clockPreviousLocalDay(time_t timestamp, time_t &previous) {
  if (timestamp <= 86400) return false;
  const auto &rules = currentRules();
  const int32_t offset = transitionAt(rules, timestamp).offset;
  previous = timestamp - 86400;
  // Preserve local wall time across both one-hour and half-hour transitions.
  for (int iteration = 0; iteration < 3; ++iteration) {
    const time_t corrected = timestamp - 86400 + offset -
                            transitionAt(rules, previous).offset;
    if (corrected == previous) break;
    previous = corrected;
  }
  return previous > 0 && previous < timestamp;
}
