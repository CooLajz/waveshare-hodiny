#pragma once
#include <stdint.h>

struct BuzzerSnapshot {
  bool ready = false;
  bool active = false;
  bool ioOk = false;
  uint32_t requestedMs = 0;
  uint32_t lastPulseMs = 0;
};

void buzzerServiceBegin();
bool buzzerServicePlay(uint32_t milliseconds);
BuzzerSnapshot buzzerServiceSnapshot();

// Alarm owns the buzzer until stopped; ordinary notification pulses cannot interrupt it.
void buzzerServiceAlarm(bool enabled);
