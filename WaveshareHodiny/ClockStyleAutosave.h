#pragma once
#include <stdint.h>

// Only explicit clock-face swipes schedule a save. Page rotation is unrelated.
class ClockStyleAutosave {
 public:
  static constexpr uint32_t delayMs = 15000;
  void select(uint8_t style, uint8_t savedStyle, uint32_t now) {
    style_ = style;
    pending_ = style != savedStyle;
    changedAt_ = now;
  }
  void cancel() { pending_ = false; }
  bool due(uint32_t now) const {
    return pending_ && uint32_t(now - changedAt_) >= delayMs;
  }
  uint8_t style() const { return style_; }
  void completed(bool success, uint32_t now) {
    pending_ = !success;
    changedAt_ = now;  // Failed writes retry at most once every 15 seconds.
  }
 private:
  bool pending_ = false;
  uint8_t style_ = 0;
  uint32_t changedAt_ = 0;
};
