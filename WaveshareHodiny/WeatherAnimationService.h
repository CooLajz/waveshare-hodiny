#pragma once

#include <stdint.h>

void weatherAnimationServiceLoop(int weatherCode, bool isDay, uint8_t style,
                                 bool enabled);
