#include "HourlyForecast.h"
#include <cJSON.h>
#include <cmath>

int forecastWeatherCode(int wmo) {
  if (wmo == 0) return 800;
  if (wmo == 1) return 801;
  if (wmo == 2) return 802;
  if (wmo == 3) return 804;
  if (wmo == 45 || wmo == 48) return 741;
  if (wmo == 51 || wmo == 53 || wmo == 55) return 300;
  if (wmo == 56 || wmo == 57 || wmo == 66 || wmo == 67) return 511;
  if (wmo == 61 || wmo == 63 || wmo == 80 || wmo == 81) return 500;
  if (wmo == 65 || wmo == 82) return 502;
  if (wmo == 71 || wmo == 73 || wmo == 77 || wmo == 85) return 600;
  if (wmo == 75 || wmo == 86) return 602;
  if (wmo == 95) return 200;
  if (wmo == 96 || wmo == 99) return 202;
  return -1;
}

bool parseHourlyForecast(const char *json, HourlyForecast &result) {
  cJSON *root = cJSON_Parse(json);
  if (!root) return false;
  const cJSON *hourly = cJSON_GetObjectItemCaseSensitive(root, "hourly");
  const char *keys[] = {"time", "temperature_2m", "weather_code", "is_day"};
  const cJSON *arrays[4];
  bool ok = cJSON_IsObject(hourly);
  for (size_t i = 0; i < 4; ++i) {
    arrays[i] = cJSON_GetObjectItemCaseSensitive(hourly, keys[i]);
    ok = ok && cJSON_IsArray(arrays[i]);
  }
  const int count = cJSON_GetArraySize(arrays[0]);
  ok = ok && count > 0 && count <= static_cast<int>(HOURLY_FORECAST_CAPACITY);
  for (size_t i = 1; i < 4; ++i)
    ok = ok && cJSON_GetArraySize(arrays[i]) == count;
  HourlyForecast parsed;
  for (int i = 0; ok && i < count; ++i) {
    const cJSON *t = cJSON_GetArrayItem(arrays[0], i);
    if (!cJSON_IsNumber(t) || !std::isfinite(t->valuedouble) ||
        t->valuedouble < 1577836800 || t->valuedouble > 4133980800 ||
        std::floor(t->valuedouble) != t->valuedouble) { ok = false; break; }
    ForecastHour &hour = parsed.hours[i];
    hour.timestamp = static_cast<time_t>(t->valuedouble);
    if (i && hour.timestamp != parsed.hours[i - 1].timestamp + 3600) {
      ok = false; break;
    }
    const cJSON *temp = cJSON_GetArrayItem(arrays[1], i);
    const cJSON *code = cJSON_GetArrayItem(arrays[2], i);
    const cJSON *day = cJSON_GetArrayItem(arrays[3], i);
    // Missing model values remain explicitly unavailable, never zero degrees.
    hour.valid = cJSON_IsNumber(temp) && cJSON_IsNumber(code) && cJSON_IsNumber(day);
    if (hour.valid) {
      hour.temperature = temp->valuedouble;
      hour.valid = std::isfinite(hour.temperature) && hour.temperature >= -100 &&
          hour.temperature <= 70 && code->valuedouble >= 0 && code->valuedouble <= 99 &&
          std::floor(code->valuedouble) == code->valuedouble &&
          (day->valuedouble == 0 || day->valuedouble == 1);
      if (hour.valid) {
        hour.weatherCode = forecastWeatherCode(code->valueint);
        hour.isDay = day->valueint == 1;
        hour.valid = hour.weatherCode >= 0;
      }
    }
  }
  cJSON_Delete(root);
  if (!ok) return false;
  parsed.count = count;
  result = parsed;
  return true;
}

const ForecastHour *forecastHourAt(const HourlyForecast &forecast, time_t hour) {
  for (size_t i = 0; i < forecast.count; ++i)
    if (forecast.hours[i].timestamp == hour && forecast.hours[i].valid)
      return &forecast.hours[i];
  return nullptr;
}
