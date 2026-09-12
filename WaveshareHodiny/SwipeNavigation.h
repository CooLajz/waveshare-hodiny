#pragma once
#include <stdint.h>

struct DisplaySwipe {
  int8_t direction = 0;
  bool vertical = false;
};

// One recent intention, not a queue of movements that can keep running later.
class SwipeNavigation {
 public:
  void offer(int8_t direction, bool vertical, uint32_t now) {
    pending = {direction, vertical};
    receivedAt = now;
  }

  DisplaySwipe take(uint32_t now, bool allowed, bool transitioning) {
    if ((!allowed && !transitioning) || uint32_t(now - receivedAt) > 1000) {
      pending = {};
    }
    if (!allowed) return {};
    const DisplaySwipe result = pending;
    pending = {};
    return result;
  }

 private:
  DisplaySwipe pending{};
  uint32_t receivedAt = 0;
};
