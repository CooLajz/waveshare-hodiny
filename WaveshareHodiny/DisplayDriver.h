#pragma once

#include <Arduino.h>
#include <lvgl.h>

void displayDriverInit();
void displayDriverLoop();
void displayDriverRefresh();
bool displayDriverBeginFramebufferCapture(Print &output);
bool displayDriverStreamFramebufferChunk(Print &output);
