#pragma once
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <cmath>

constexpr size_t HOURLY_FORECAST_CAPACITY = 24;
struct ForecastHour {
  time_t timestamp = 0;
  float temperature = NAN;
  int weatherCode = -1;
  bool isDay = true;
  bool valid = false;
};
struct HourlyForecast {
  ForecastHour current;
  ForecastHour hours[HOURLY_FORECAST_CAPACITY];
  size_t count = 0;
  time_t fetchedAt = 0;
};
bool parseHourlyForecast(const char *json, HourlyForecast &result);
const ForecastHour *forecastHourAt(const HourlyForecast &forecast, time_t hour);
int forecastWeatherCode(int wmo);
