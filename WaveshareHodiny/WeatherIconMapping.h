#pragma once

#include <Arduino.h>

#include "ClockConfig.h"

inline const char *weatherIconStyleName(uint8_t style) {
  if (style == CLOCK_WEATHER_ICON_STYLE_FLAT) return "flat";
  if (style == CLOCK_WEATHER_ICON_STYLE_LINE) return "line";
  return "monochrome";
}

inline const char *weatherAnimationKeyForCode(int weatherCode, bool isDay) {
  if (weatherCode >= 200 && weatherCode <= 232) return "thunderstorms";
  if (weatherCode >= 300 && weatherCode <= 321) return "drizzle";
  if (weatherCode >= 500 && weatherCode <= 510 && weatherCode != 511)
    return "rain";
  if (weatherCode == 511) return "sleet";
  if (weatherCode >= 520 && weatherCode <= 531) return "rain";
  if (weatherCode >= 600 && weatherCode <= 622) return "snow";
  if (weatherCode >= 701 && weatherCode <= 781) return "mist";
  if (weatherCode == 800) return isDay ? "clear-day" : "clear-night";
  if (weatherCode == 801)
    return isDay ? "mostly-clear-day" : "mostly-clear-night";
  if (weatherCode == 802)
    return isDay ? "partly-cloudy-day" : "partly-cloudy-night";
  if (weatherCode == 803)
    return isDay ? "overcast-day" : "overcast-night";
  if (weatherCode == 804) return "overcast";
  return nullptr;
}

inline bool weatherAnimationAssetKey(char *destination,
                                     size_t destinationSize,
                                     int weatherCode, bool isDay,
                                     uint8_t style) {
  const char *condition = weatherAnimationKeyForCode(weatherCode, isDay);
  if (destination == nullptr || destinationSize == 0 || condition == nullptr)
    return false;
  snprintf(destination, destinationSize, "%s-%s", weatherIconStyleName(style),
           condition);
  return true;
}
