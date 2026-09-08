#include "../WaveshareHodiny/ClockTimezone.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

time_t utc(int year, int month, int day, int hour, int minute = 0, int second = 0) {
  struct tm value{};
  value.tm_year = year - 1900;
  value.tm_mon = month - 1;
  value.tm_mday = day;
  value.tm_hour = hour;
  value.tm_min = minute;
  value.tm_sec = second;
  return timegm(&value);
}

void check(const char *zone, time_t timestamp, int hour, int minute, bool dst) {
  assert(clockTimezoneSet(zone));
  struct tm local{};
  assert(clockLocaltime(&timestamp, &local));
  assert(local.tm_hour == hour && local.tm_min == minute);
  assert(local.tm_isdst == static_cast<int>(dst));
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--probe") == 0) {
    std::string zone;
    int64_t epoch;
    while (std::cin >> zone >> epoch) {
      assert(clockTimezoneSet(zone.c_str()));
      const time_t timestamp = epoch;
      struct tm local{};
      assert(clockLocaltime(&timestamp, &local));
      std::cout << local.tm_year + 1900 << ' ' << local.tm_mon + 1 << ' '
                << local.tm_mday << ' ' << local.tm_hour << ' ' << local.tm_min
                << ' ' << local.tm_sec << ' ' << local.tm_isdst << '\n';
    }
    return 0;
  }
  check("Europe/Prague", utc(2026, 3, 29, 0, 59, 59), 1, 59, false);
  check("Europe/Prague", utc(2026, 3, 29, 1), 3, 0, true);
  check("Europe/Prague", utc(2026, 10, 25, 0, 59, 59), 2, 59, true);
  check("Europe/Prague", utc(2026, 10, 25, 1), 2, 0, false);
  check("America/New_York", utc(2026, 3, 8, 7), 3, 0, true);
  check("America/Phoenix", utc(2026, 7, 1, 12), 5, 0, false);
  check("Asia/Kathmandu", utc(2026, 7, 1, 12), 17, 45, false);
  check("Australia/Sydney", utc(2026, 1, 1, 12), 23, 0, true);
  check("Australia/Sydney", utc(2026, 7, 1, 12), 22, 0, false);
  check("Australia/Lord_Howe", utc(2026, 4, 4, 15), 1, 30, false);
  check("Pacific/Chatham", utc(2026, 1, 1, 12), 1, 45, true);
  // Morocco's Ramadan suspension cannot be represented by one recurring EU rule.
  check("Africa/Casablanca", utc(2026, 2, 20, 12), 12, 0, true);
  check("Africa/Casablanca", utc(2026, 4, 1, 12), 13, 0, false);
  assert(!clockTimezoneSupported("Europe/Not_A_Zone"));
  assert(!clockTimezoneSet("Europe/Not_A_Zone"));
  check("", utc(2026, 1, 1, 12), 13, 0, false);
  time_t previous;
  assert(clockTimezoneSet("Europe/Prague"));
  assert(clockPreviousLocalDay(utc(2026, 3, 29, 6), previous));
  assert(previous == utc(2026, 3, 28, 7));
  assert(clockPreviousLocalDay(utc(2026, 10, 25, 7), previous));
  assert(previous == utc(2026, 10, 24, 6));
  assert(clockTimezoneSet("Australia/Lord_Howe"));
  assert(clockPreviousLocalDay(utc(2026, 4, 5, 0), previous));
  assert(previous == utc(2026, 4, 3, 23, 30));
  std::puts("Clock timezone tests passed");
}
