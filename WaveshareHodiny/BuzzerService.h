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
