#pragma once

#include <stddef.h>
#include <stdint.h>
#include <time.h>

constexpr size_t CLOCK_TIMEZONE_LENGTH = 64;
constexpr int64_t CLOCK_TIMEZONE_START = 1577836800;
constexpr int64_t CLOCK_TIMEZONE_END = 4133980800;

bool clockTimezoneSupported(const char *name);
// An empty name retains the legacy Czech clock until location resolution.
bool clockTimezoneSet(const char *name);
struct tm *clockLocaltime(const time_t *timestamp, struct tm *result);
bool clockPreviousLocalDay(time_t timestamp, time_t &previous);
