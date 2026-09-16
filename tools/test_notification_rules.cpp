#include "../WaveshareHodiny/NotificationRules.h"
#include <assert.h>
#include <string>
#include <stdio.h>

int main() {
  uint32_t value = 42;
  assert(notificationParseSeconds("0", value) && value == 0);
  assert(notificationParseSeconds("86400", value) && value == 86400);
  for (const char *bad : {"", "-1", "+1", "1.5", " 2", "2s", "86401",
                          "4294967296", "99999999999999999999"})
    assert(!notificationParseSeconds(bad, value));
  assert(notificationParseBeep("0", value) && value == 0);
  assert(notificationParseBeep("150", value) && value == 150);
  assert(notificationParseBeep("5000", value) && value == 5000);
  for (const char *bad : {"", "5001", "86400", "-1", "0.5", "true", "150ms", "999999999999999999"})
    assert(!notificationParseBeep(bad, value));
  assert(notificationParseColor("#Ff804a", value) && value == 0xff804a);
  for (const char *bad : {"#fff", "ffffff", "#0000000", "#00xx00", ""})
    assert(!notificationParseColor(bad, value));
  assert(notificationValidText("Příliš žluťoučký kůň", 96, false));
  assert(notificationValidText("První řádek\nDruhý řádek", 768, true));
  assert(!notificationValidText("Nadpis\ndruhý řádek", 96, false));
  assert(!notificationValidText("  \n ", 768, true));
  assert(!notificationValidText("", 96, false));
  assert(notificationValidText(std::string(96, 'A').c_str(), 96, false));
  assert(!notificationValidText(std::string(97, 'A').c_str(), 96, false));
  for (const char *bad : {"\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80",
                          "\xe2\x82", "\x80", "\xf0\x80\x80\x80", "\xc2\x85"})
    assert(!notificationValidText(bad, 96, false));
  NotificationLifetime state;
  assert(!state.expired(0));
  state.show(100, 0);
  assert(state.active && !state.expired(0xffffffffU));
  state.show(100, 2);
  assert(!state.expired(2099) && state.expired(2100));
  state.show(2000, 3); // Replacement restarts the interval.
  assert(!state.expired(4999) && state.expired(5000));
  state.show(0xffffff00U, 1); // millis() wraparound.
  assert(!state.expired(743) && state.expired(744));
  state.show(1, NOTIFICATION_MAX_SECONDS);
  assert(!state.expired(86400000) && state.expired(86400001));
  state.active = false;
  assert(!state.expired(86400001));
  puts("Notification rules: OK");
}
