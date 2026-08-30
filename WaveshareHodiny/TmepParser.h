#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr size_t TMEP_MAX_SENSORS = 32;
constexpr size_t TMEP_SENSOR_TITLE_LENGTH = 64;
constexpr size_t TMEP_SENSOR_DOMAIN_LENGTH = 64;
constexpr size_t TMEP_SENSOR_LOCATION_LENGTH = 64;
constexpr size_t TMEP_MEASURED_AT_LENGTH = 24;
constexpr size_t TMEP_VALUE_UNIT_LENGTH = 16;

struct TmepValue {
  bool available = false;
  float value = 0.0f;
  uint8_t decimals = 0;
  char unit[TMEP_VALUE_UNIT_LENGTH] = "";
};

struct TmepSensor {
  char id[16] = "";
  char title[TMEP_SENSOR_TITLE_LENGTH] = "";
  char domain[TMEP_SENSOR_DOMAIN_LENGTH] = "";
  char location[TMEP_SENSOR_LOCATION_LENGTH] = "";
  char measuredAt[TMEP_MEASURED_AT_LENGTH] = "";
  TmepValue temperature;
  TmepValue humidity;
  TmepValue pressure;
  TmepValue rssi;
  TmepValue voltage;
};

struct TmepCatalog {
  size_t count = 0;
  bool truncated = false;
  TmepSensor sensors[TMEP_MAX_SENSORS];
};

bool tmepParseExport(const char *payload, size_t length, TmepCatalog &catalog,
                     char *error, size_t errorSize);
const TmepSensor *tmepFindSensor(const TmepCatalog &catalog,
                                const char *sensorId);
const TmepValue *tmepFindValue(const TmepSensor &sensor, const char *field);
bool tmepFieldSupported(const char *field);
