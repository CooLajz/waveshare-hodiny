#include "ForecastWeatherSource.h"
#include <cJSON.h>
#include <cstdio>

float forecastEntityTemperature(const char *json, bool sensor) {
  cJSON *root = cJSON_Parse(json);
  if (!root) return NAN;
  const cJSON *attributes = cJSON_GetObjectItemCaseSensitive(root, "attributes");
  const cJSON *unit = cJSON_GetObjectItemCaseSensitive(attributes,
      sensor ? "unit_of_measurement" : "temperature_unit");
  const cJSON *value = cJSON_GetObjectItemCaseSensitive(sensor ? root : attributes,
      sensor ? "state" : "temperature");
  char number[40];
  const char *state = nullptr;
  if (cJSON_IsString(value)) state = value->valuestring;
  else if (cJSON_IsNumber(value)) {
    snprintf(number, sizeof(number), "%.9g", value->valuedouble);
    state = number;
  }
  const float result = state && cJSON_IsString(unit)
      ? forecastSensorTemperature(state, unit->valuestring) : NAN;
  cJSON_Delete(root);
  return result;
}
