#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include "SwipeNavigation.h"

void displayDriverInit();
void displayDriverLoop();
void displayDriverRefresh();
void displayDriverSetPartialRefresh(bool enabled, bool rebuildBuffers = false);
DisplaySwipe displayDriverTakeSwipe(bool allowed, bool transitioning);
bool displayDriverTakeSingleClick();
void displayDriverDiscardTouchUntilRelease();
bool displayDriverBeginFramebufferCapture(Print &output);
bool displayDriverStreamFramebufferChunk(Print &output);

void displayDriverPrintRenderStats(Print &output);
