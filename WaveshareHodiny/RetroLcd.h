#pragma once

#include <lvgl.h>
#include <time.h>
#include "ClockDashboard.h"

// Renderer Retro LCD; persistentní nastavení spravuje ClockAppearanceConfig.
bool retroLcdEnabled();
void retroLcdEnable(lv_obj_t *parent, bool enabled);
void retroLcdRaise();
void retroLcdSetDigitPlaces(uint8_t a, uint8_t b);
void retroLcdSetGhostOpacity(uint8_t percent);
void retroLcdSetColors(uint32_t background, uint32_t foreground);
void retroLcdSetWeatherStyle(bool raster);
void retroLcdSetWeather(bool left, bool right, const lv_img_dsc_t *icon);
void retroLcdSetProgress(const ClockMetricConfig *config, float value, float minimum, float maximum, uint8_t segments);
void retroLcdSetLeadingHourZero(bool enabled);
void retroLcdSetFixedWeekday(bool enabled);
void retroLcdSet12HourFormat(bool enabled);
void retroLcdSetTime(const tm &localTime);
void retroLcdUpdate(const ClockValues &values, const ClockMetricConfig &a,
                    const ClockMetricConfig &b, bool english, bool night,
                    bool wifi, bool web);
