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

bool clockLocaltimeInverse(const struct tm &local, time_t &timestamp) {
  if (local.tm_mon < 0 || local.tm_mon > 11 || local.tm_mday < 1 ||
      local.tm_mday > 31 || local.tm_hour < 0 || local.tm_hour > 23 ||
      local.tm_min < 0 || local.tm_min > 59 || local.tm_sec < 0 || local.tm_sec > 59) return false;
  // Gregorian civil date to Unix days (March-based 400-year eras).
  int year = local.tm_year + 1900;
  const int month = local.tm_mon + 1;
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + local.tm_mday - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const int64_t wall = (static_cast<int64_t>(era) * 146097 + doe - 719468) * 86400 +
      local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec;
  const auto &rules = currentRules();
  bool found = false;
  for (unsigned i = 0; i < rules.count; ++i) {
    const int64_t candidate = wall - rules.transitions[i].offset;
    if (candidate < CLOCK_TIMEZONE_START || candidate >= CLOCK_TIMEZONE_END ||
        transitionAt(rules, static_cast<time_t>(candidate)).offset != rules.transitions[i].offset) continue;
    tm check{}; const time_t value = static_cast<time_t>(candidate);
    if (!clockLocaltime(&value, &check) || check.tm_year != local.tm_year ||
        check.tm_mon != local.tm_mon || check.tm_mday != local.tm_mday ||
        check.tm_hour != local.tm_hour || check.tm_min != local.tm_min || check.tm_sec != local.tm_sec) continue;
    if (!found || value < timestamp) { timestamp = value; found = true; }
  }
  return found;
}
