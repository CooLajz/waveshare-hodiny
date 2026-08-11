#pragma once

#include <Arduino.h>

constexpr size_t CLOCK_ROOM_NAME_LENGTH = 32;
constexpr size_t CLOCK_HA_URL_LENGTH = 192;
constexpr size_t CLOCK_HA_TOKEN_LENGTH = 256;
constexpr size_t CLOCK_ENTITY_ID_LENGTH = 128;
constexpr size_t CLOCK_METRIC_NAME_LENGTH = 24;
constexpr size_t CLOCK_METRIC_SUFFIX_LENGTH = 16;
constexpr size_t CLOCK_ROOM_ICON_LENGTH = 16;
constexpr size_t CLOCK_METRIC_COLOR_POINT_COUNT = 10;
constexpr uint32_t CLOCK_CONFIG_SCHEMA_VERSION = 16;

enum ClockSecondEffect : uint8_t {
  CLOCK_SECOND_EFFECT_DOTS = 0,
  CLOCK_SECOND_EFFECT_LINE = 1,
  CLOCK_SECOND_EFFECT_COMET = 2,
};

enum ClockWeatherIconStyle : uint8_t {
  CLOCK_WEATHER_ICON_STYLE_MONOCHROME = 0,
  CLOCK_WEATHER_ICON_STYLE_FLAT = 1,
  CLOCK_WEATHER_ICON_STYLE_LINE = 2,
};

enum ClockNightVisualMode : uint8_t {
  CLOCK_NIGHT_VISUAL_RED = 0,
  CLOCK_NIGHT_VISUAL_BRIGHTNESS_ONLY = 1,
};

struct ClockMetricConfig {
  bool custom = false;
  char preset[16] = "co2";
  char name[CLOCK_METRIC_NAME_LENGTH] = "CO₂";
  char entityId[CLOCK_ENTITY_ID_LENGTH] = "";
  char suffix[CLOCK_METRIC_SUFFIX_LENGTH] = "ppm";
  uint8_t decimals = 0;
};

struct ClockSideConfig {
  char name[CLOCK_ROOM_NAME_LENGTH] = "MÍSTNOST";
  char temperatureEntityId[CLOCK_ENTITY_ID_LENGTH] = "";
  char icon[CLOCK_ROOM_ICON_LENGTH] = "home";
  uint32_t color = 0xFFFFFF;
};

struct ClockMetricColorPoint {
  float value = 0.0f;
  uint32_t color = 0xFFFFFF;
};

struct ClockMetricColorScale {
  uint8_t count = 1;
  ClockMetricColorPoint points[CLOCK_METRIC_COLOR_POINT_COUNT];
};

struct ClockConfig {
  uint32_t schemaVersion = CLOCK_CONFIG_SCHEMA_VERSION;
  char homeAssistantUrl[CLOCK_HA_URL_LENGTH] = "";
  char homeAssistantToken[CLOCK_HA_TOKEN_LENGTH] = "";
  char weatherEntityId[CLOCK_ENTITY_ID_LENGTH] = "";
  char sunEntityId[CLOCK_ENTITY_ID_LENGTH] = "sun.sun";
  ClockSideConfig leftSide;
  ClockSideConfig rightSide;
  ClockMetricConfig metricA;
  ClockMetricConfig metricB;
  ClockMetricColorScale metricAColorScale;
  ClockMetricColorScale metricBColorScale;
  uint32_t timeColor = 0xF6F6F6;
  uint32_t dateColor = 0xB5B5B5;
  uint32_t leftWeatherIconColor = 0xFFFFFF;
  uint32_t rightWeatherIconColor = 0xFFFFFF;
  bool animatedWeatherIcons = true;
  uint8_t weatherIconStyle = CLOCK_WEATHER_ICON_STYLE_MONOCHROME;
  uint8_t dayBrightness = 35;
  uint8_t nightBrightness = 10;
  bool automaticDayNight = true;
  int8_t sunsetOffsetMinutes = 0;
  bool automaticFirmwareUpdate = false;
  bool secondRingEnabled = true;
  uint8_t secondEffect = CLOCK_SECOND_EFFECT_DOTS;
  int8_t sunriseOffsetMinutes = 0;
  uint32_t secondRingBackgroundColor = 0xFFFFFF;
  uint8_t secondRingBackgroundBrightness = 0;
  uint8_t secondRingBackgroundDotSize = 3;
  uint8_t secondDotSize = 3;
  uint32_t secondDotColor = 0xFFFFFF;
  uint8_t secondDotBrightness = 175;
  char dayNightLightEntityId[CLOCK_ENTITY_ID_LENGTH] = "";
  uint8_t nightVisualMode = CLOCK_NIGHT_VISUAL_RED;
};

bool clockConfigBegin();
bool clockConfigLoad(ClockConfig &config);
bool clockConfigSave(const ClockConfig &config);
void clockConfigApplyDefaults(ClockConfig &config);

void clockConfigCopy(char *destination, size_t destinationSize,
                     const String &value);
void clockConfigCopy(char *destination, size_t destinationSize,
                     const char *value);
