#pragma once
#include <stdint.h>

// The GT911 uses 16-bit register addresses and explicitly acknowledged frames.
bool GT911_Detect();
bool GT911_ReadSample(uint8_t &points, uint16_t &x, uint16_t &y, bool &fresh);
