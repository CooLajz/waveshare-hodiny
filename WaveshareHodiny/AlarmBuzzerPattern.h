#pragma once
#include <stdint.h>

// Finish whole rhythm cycles before changing tempo. Each new rhythm starts
// from its first pulse after an additional quiet transition interval.
inline bool alarmBuzzerOn(uint32_t elapsed) {
  constexpr uint32_t pause = 600;
  constexpr uint32_t gentleEnd = 7 * 2400;
  constexpr uint32_t mediumStart = gentleEnd + pause;
  constexpr uint32_t mediumEnd = mediumStart + 16 * 1800;
  constexpr uint32_t urgentStart = mediumEnd + pause;
  uint32_t phase, pulses;
  if (elapsed < gentleEnd) {
    phase = elapsed % 2400; pulses = 2;
  } else if (elapsed < mediumStart) {
    return false;
  } else if (elapsed < mediumEnd) {
    phase = (elapsed - mediumStart) % 1800; pulses = 4;
  } else if (elapsed < urgentStart) {
    return false;
  } else {
    phase = (elapsed - urgentStart) % 1200; pulses = 4;
  }
  return elapsed < 600000 && phase < pulses * 240 && phase % 240 < 140;
}
