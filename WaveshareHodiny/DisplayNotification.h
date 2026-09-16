#pragma once

#include <stdint.h>

// All calls run on the main UI loop, including the HTTP handler.
bool displayNotificationShow(const char *title, const char *message,
                             uint32_t durationSeconds, uint32_t textColor,
                             uint32_t backgroundColor, uint32_t beepMs = 0);
bool displayNotificationActive();
void displayNotificationDismiss();
void displayNotificationLoop();


// Called after the final LVGL flush synchronizes with the panel.
void displayNotificationFramePresented(bool success);

void displayNotificationSetRedNight(bool enabled);
