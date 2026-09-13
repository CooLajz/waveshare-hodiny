#pragma once
#include "HourlyForecast.h"
#include <cmath>
#include <cstdlib>
#include <cstring>

inline ForecastHour resolveForecastCurrent(bool haEnabled, const ForecastHour &ha,
                                           float sensor, const ForecastHour &openMeteo) {
  ForecastHour result = haEnabled && ha.valid ? ha : openMeteo;
  if (!std::isfinite(result.temperature)) result.temperature = openMeteo.temperature;
  if (haEnabled && std::isfinite(sensor)) result.temperature = sensor;
  return result;
}

inline float forecastSensorTemperature(const char *state, const char *unit) {
  char *end = nullptr;
  const float value = std::strtof(state, &end);
  if (end == state || *end != '\0' || !std::isfinite(value)) return NAN;
  float celsius = NAN;
  if (!std::strcmp(unit, "°C")) celsius = value;
  else if (!std::strcmp(unit, "°F")) celsius = (value - 32.0f) / 1.8f;
  else if (!std::strcmp(unit, "K")) celsius = value - 273.15f;
  return celsius >= -100 && celsius <= 70 ? celsius : NAN;
}

float forecastEntityTemperature(const char *json, bool sensor);
