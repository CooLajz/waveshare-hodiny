#include "../WaveshareHodiny/SwipeNavigation.h"
#include <cassert>
#include <cstdio>

int main() {
  SwipeNavigation input;
  assert(input.take(0, true, false).direction == 0);
  input.offer(-1, false, 10);
  assert(input.take(20, false, true).direction == 0);
  const auto deferred = input.take(510, true, false);
  assert(deferred.direction == -1 && !deferred.vertical);
  assert(input.take(511, true, false).direction == 0);

  input.offer(-1, false, 600);
  input.offer(1, true, 650);
  const auto latest = input.take(900, true, false);
  assert(latest.direction == 1 && latest.vertical);
  assert(input.take(901, true, false).direction == 0);

  input.offer(1, false, 1000);
  assert(input.take(1001, false, false).direction == 0); // Settings/update.
  assert(input.take(1002, true, false).direction == 0);
  input.offer(1, true, 2000);
  assert(input.take(3001, false, true).direction == 0); // Expired while busy.
  assert(input.take(3002, true, false).direction == 0);
  input.offer(-1, true, 4000);
  assert(input.take(5001, true, false).direction == 0); // Expired before use.

  input.offer(-1, true, UINT32_MAX - 100);
  const auto wrapped = input.take(100, true, false);
  assert(wrapped.direction == -1 && wrapped.vertical);
  input.offer(1, false, UINT32_MAX - 100);
  assert(input.take(1000, true, false).direction == 0);
  puts("PASS: deferred/latest gesture, blocked screens, expiry and millis wrap");
}
