#pragma once
#include <stdint.h>

// Recognize taps from sampled contact edges, independently of LVGL latency.
// Gesture numbers match CST820; swipes and long holds never become taps.
class TouchGestureTracker {
 public:
  uint8_t sample(bool down, uint16_t x, uint16_t y, uint8_t gesture, uint32_t now) {
    uint8_t event = 0;
    if (down && !pressed) {
      started = now; startX = x; startY = y; moved = false; emitted = false;
    }
    if (down || pressed) {
      const int dx = int(x) - startX, dy = int(y) - startY;
      if (dx * dx + dy * dy > 24 * 24) moved = true;
    }
    if (gesture >= 1 && gesture <= 4) moved = true;
    if (gesture == 12) moved = true; // Long press.
    if (gesture && gesture != lastGesture) {
      if (gesture == 5) {
        if (!emitted && !moved) { event = 5; emitted = true; }
      } else {
        event = gesture;
      }
    }
    if (!down && pressed && !moved && !emitted &&
        uint32_t(now - started) >= 20 && uint32_t(now - started) <= 800) {
      event = 5; emitted = true;
    }
    // A tap reported without contact samples can be accepted again after idle.
    if (!down && !pressed && !gesture && uint32_t(now - started) > 800) {
      emitted = false; moved = false;
    }
    pressed = down;
    lastGesture = gesture;
    return event;
  }
 private:
  bool pressed = false, moved = false, emitted = false;
  uint8_t lastGesture = 0;
  uint16_t startX = 0, startY = 0;
  uint32_t started = 0;
};
