#pragma once

#include <Arduino.h>
#include <lvgl.h>

void displayDriverInit();
void displayDriverLoop();
void displayDriverRefresh();
void displayDriverSetPartialRefresh(bool enabled, bool rebuildBuffers = false);
bool displayDriverTakeHorizontalSwipe();
int8_t displayDriverTakeVerticalSwipe();
bool displayDriverTakeSingleClick();
bool displayDriverBeginFramebufferCapture(Print &output);
bool displayDriverStreamFramebufferChunk(Print &output);
