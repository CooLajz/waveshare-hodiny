#include "ClockConfig.h"

#include "SettingsStore.h"
#include <nvs_flash.h>

#include <cmath>

namespace {
constexpr uint32_t CONFIG_MAGIC = 0x57484346;
constexpr char CONFIG_PARTITION[] = "clockcfg";
constexpr char CONFIG_NAMESPACE[] = "clock-config";
constexpr char CONFIG_KEY[] = "config";
constexpr char APPEARANCE_NAMESPACE[] = "clock-look";
constexpr char APPEARANCE_STYLE_KEY[] = "style";
constexpr char APPEARANCE_TONE_KEY[] = "tone";
constexpr char APPEARANCE_HAND_TONE_KEY[] = "hand-tone";
constexpr char APPEARANCE_ACCENT_COLOR_KEY[] = "accent-color";
constexpr char APPEARANCE_ACCENTS_KEY[] = "accents";
constexpr char APPEARANCE_OUTLINE_HANDS_KEY[] = "outline-hands";
constexpr char APPEARANCE_MONO_VALUES_KEY[] = "mono-values";
constexpr char APPEARANCE_VALUES_ABOVE_KEY[] = "values-above";
constexpr char APPEARANCE_DATE_FORMAT_KEY[] = "date-format";
constexpr char APPEARANCE_DATE_COLOR_KEY[] = "date-color";
constexpr char APPEARANCE_WEATHER_COLOR_KEY[] = "weather-color";

struct ConfigRecord {
  uint32_t magic;
  uint32_t schemaVersion;
  ClockConfig config;
  uint32_t checksum;
};

constexpr uint32_t PUBLIC_1_5_5_SCHEMA_VERSION = 20;
constexpr uint32_t LANGUAGE_SCHEMA_VERSION = 25;
constexpr uint32_t RADAR_SCHEMA_VERSION = 24;
constexpr uint32_t TMEP_PREDECESSOR_SCHEMA_VERSION = 26;
constexpr uint32_t SIDE_VALUES_PREDECESSOR_SCHEMA_VERSION = 27;

// Firmware 1.5.5 stored the same prefix as ClockConfig up to dateFormat.
// Keeping the payload as bytes preserves its exact released NVS layout and
// checksum without retaining every unreleased development migration.
constexpr size_t PUBLIC_1_5_5_CONFIG_SIZE = offsetof(ClockConfig, radarRadiusKm);

struct ConfigRecordV155 {
  uint32_t magic;
  uint32_t schemaVersion;
  uint8_t config[PUBLIC_1_5_5_CONFIG_SIZE];
  uint32_t checksum;
};

constexpr size_t SCHEMA_26_CONFIG_SIZE = offsetof(ClockConfig, tmepExportKey);

struct ConfigRecordV26 {
  uint32_t magic;
  uint32_t schemaVersion;
  uint8_t config[SCHEMA_26_CONFIG_SIZE];
  uint32_t checksum;
};

constexpr size_t SCHEMA_27_CONFIG_SIZE = offsetof(ClockConfig, leftValue);

constexpr size_t SCHEMA_29_CONFIG_SIZE = offsetof(ClockConfig, forecastDisplaySeconds);
struct ConfigRecordV29 {
  uint32_t magic;
  uint32_t schemaVersion;
  uint8_t config[SCHEMA_29_CONFIG_SIZE];
  uint32_t checksum;
};
static_assert(sizeof(ConfigRecordV29) == 2764, "Preserve schema 29 NVS layout.");

constexpr size_t SCHEMA_28_CONFIG_SIZE = offsetof(ClockConfig, timeZone);
struct ConfigRecordV28 {
  uint32_t magic;
  uint32_t schemaVersion;
  uint8_t config[SCHEMA_28_CONFIG_SIZE];
  uint32_t checksum;
};
static_assert(sizeof(ConfigRecordV28) == 2700, "Preserve schema 28 NVS layout.");

struct ConfigRecordV27 {
  uint32_t magic;
  uint32_t schemaVersion;
  uint8_t config[SCHEMA_27_CONFIG_SIZE];
  uint32_t checksum;
};

void applyLegacySideValueDefaults(ClockConfig &config) {
  config.leftValue = ClockSideValueConfig{};
  config.rightValue = ClockSideValueConfig{};
  // Původní levá a pravá teplota dovolovaly vlastní názvy. Zachováme je jako
  // vlastní hodnoty, aby samotné uložení nové stránky nepřejmenovalo například
  // VENKU nebo LOŽNICE na obecné TEPLOTA.
  config.leftValue.custom = true;
  config.rightValue.custom = true;
  clockConfigCopy(config.leftValue.preset,
                  sizeof(config.leftValue.preset), "custom");
  clockConfigCopy(config.rightValue.preset,
                  sizeof(config.rightValue.preset), "custom");
  config.leftValueColorScale = ClockMetricColorScale{};
  config.leftValueColorScale.points[0] = {0.0f, config.leftSide.color};
  config.rightValueColorScale = ClockMetricColorScale{};
  config.rightValueColorScale.points[0] = {0.0f, config.rightSide.color};
}

void applyOpenMeteoDefaults(ClockConfig &config) {
  config.dataSource = CLOCK_DATA_SOURCE_OPEN_METEO;
  clockConfigCopy(config.openMeteoCity, sizeof(config.openMeteoCity), "Brno");
  config.openMeteoLatitude = 49.1951f;
  config.openMeteoLongitude = 16.6068f;
  config.openMeteoCountry = CLOCK_LOCATION_COUNTRY_CZECHIA;
  static const char *values[] = {"temperature_2m", "apparent_temperature",
                                 "relative_humidity_2m", "pressure_msl"};
  static const char *names[] = {"TEPLOTA", "POCITOVÁ", "VLHKOST", "TLAK"};
  static const uint32_t colors[] = {0x4CCBEC, 0xFFB843, 0x65C744, 0xFFB843};
  for (size_t index = 0; index < 4; ++index) {
    clockConfigCopy(config.openMeteoSlots[index].value,
                    sizeof(config.openMeteoSlots[index].value), values[index]);
    clockConfigCopy(config.openMeteoSlots[index].name,
                    sizeof(config.openMeteoSlots[index].name), names[index]);
    config.openMeteoSlots[index].color = colors[index];
  }
}

static_assert(PUBLIC_1_5_5_CONFIG_SIZE % alignof(ClockConfig) == 0,
              "Záznam veřejné verze 1.5.5 musí zahrnout koncový padding.");
static_assert(PUBLIC_1_5_5_CONFIG_SIZE == 2096 &&
                  sizeof(ConfigRecordV155) == 2108,
              "NVS formát veřejné verze 1.5.5 se nesmí změnit.");
static_assert(SCHEMA_26_CONFIG_SIZE == 2108 &&
                  sizeof(ConfigRecordV26) == 2120,
              "Migrační záznam schématu 26 musí zachovat přesnou velikost.");
static_assert(SCHEMA_27_CONFIG_SIZE == 2452 &&
                  sizeof(ConfigRecordV27) == 2464,
              "Migrační záznam schématu 27 musí zachovat přesnou velikost.");
static_assert(sizeof(ConfigRecordV155) <= sizeof(ConfigRecord),
              "Migrační záznam se musí vejít do společného pracovního bufferu.");
static_assert(sizeof(ConfigRecordV26) <= sizeof(ConfigRecord),
              "Schéma 26 se musí vejít do společného pracovního bufferu.");
static_assert(sizeof(ConfigRecordV27) <= sizeof(ConfigRecord),
              "Schéma 27 se musí vejít do společného pracovního bufferu.");

uint32_t bytesChecksum(const uint8_t *bytes, size_t size) {
  uint32_t hash = 2166136261u;
  for (size_t index = 0; index < size; ++index) {
    hash ^= bytes[index];
    hash *= 16777619u;
  }
  return hash;
}

uint32_t configChecksum(const ClockConfig &config) {
  return bytesChecksum(reinterpret_cast<const uint8_t *>(&config),
                       sizeof(config));
}

void normalizeConfig(ClockConfig &config) {
  config.schemaVersion = CLOCK_CONFIG_SCHEMA_VERSION;
  config.timeZone[sizeof(config.timeZone) - 1] = '\0';
  if (!clockTimezoneSupported(config.timeZone)) config.timeZone[0] = '\0';
  config.dayBrightness = constrain(config.dayBrightness, 1, 100);
  config.nightBrightness = constrain(config.nightBrightness, 1, 100);
  config.sunriseOffsetMinutes = constrain(config.sunriseOffsetMinutes, -60, 60);
  config.sunsetOffsetMinutes = constrain(config.sunsetOffsetMinutes, -60, 60);
  config.metricA.decimals = constrain(config.metricA.decimals, 0, 2);
  config.metricB.decimals = constrain(config.metricB.decimals, 0, 2);
  config.leftValue.decimals = constrain(config.leftValue.decimals, 0, 2);
  config.rightValue.decimals = constrain(config.rightValue.decimals, 0, 2);
  config.secondRingBackgroundDotSize =
      constrain(config.secondRingBackgroundDotSize, 1, 10);
  config.secondDotSize = constrain(config.secondDotSize, 1, 10);
  config.secondEffect = constrain(
      config.secondEffect, static_cast<uint8_t>(CLOCK_SECOND_EFFECT_DOTS),
      static_cast<uint8_t>(CLOCK_SECOND_EFFECT_COMET));
  config.timeColonEffect = constrain(
      config.timeColonEffect, static_cast<uint8_t>(CLOCK_TIME_COLON_STEADY),
      static_cast<uint8_t>(CLOCK_TIME_COLON_FADE));
  config.weatherIconStyle = constrain(
      config.weatherIconStyle,
      static_cast<uint8_t>(CLOCK_WEATHER_ICON_STYLE_MONOCHROME),
      static_cast<uint8_t>(CLOCK_WEATHER_ICON_STYLE_LINE));
  config.nightVisualMode = constrain(
      config.nightVisualMode, static_cast<uint8_t>(CLOCK_NIGHT_VISUAL_RED),
      static_cast<uint8_t>(CLOCK_NIGHT_VISUAL_BRIGHTNESS_ONLY));
  config.timeFont = constrain(
      config.timeFont, static_cast<uint8_t>(CLOCK_TIME_FONT_BARLOW),
      static_cast<uint8_t>(CLOCK_TIME_FONT_DOTO));
  config.dateFormat = constrain(
      config.dateFormat,
      static_cast<uint8_t>(CLOCK_DATE_FORMAT_WEEKDAY_DAY_MONTH),
      static_cast<uint8_t>(CLOCK_DATE_FORMAT_DAY_MONTH));
  config.dataSource = constrain(
      config.dataSource, static_cast<uint8_t>(CLOCK_DATA_SOURCE_OPEN_METEO),
      static_cast<uint8_t>(CLOCK_DATA_SOURCE_HOME_ASSISTANT));
  config.language = constrain(
      config.language, static_cast<uint8_t>(CLOCK_LANGUAGE_UNSET),
      static_cast<uint8_t>(CLOCK_LANGUAGE_ENGLISH));
  if (config.openMeteoCountry < CLOCK_LOCATION_COUNTRY_CZECHIA ||
      config.openMeteoCountry > CLOCK_LOCATION_COUNTRY_OTHER) {
    // Verze 1.5.5 i všechna dosavadní vývojová schémata byla určená české
    // komunitě. Konfigurace bez uložené země proto při migraci dostane CZ.
    // Každé nové vyhledání už ukládá výslovný country_code z Open-Meteo.
    config.openMeteoCountry = CLOCK_LOCATION_COUNTRY_CZECHIA;
  }
  if (config.radarRadiusKm != 0 && config.radarRadiusKm != 25 &&
      config.radarRadiusKm != 50 &&
      config.radarRadiusKm != 100 && config.radarRadiusKm != 200) {
    config.radarRadiusKm = 50;
  }
  config.radarFrameCount = constrain(config.radarFrameCount, 1, 15);
  config.clockDisplaySeconds =
      constrain(config.clockDisplaySeconds, 0, 3600);
  config.radarDisplaySeconds =
      constrain(config.radarDisplaySeconds, 0, 3600);
  config.forecastDisplaySeconds = constrain(config.forecastDisplaySeconds, 0, 3600);
  config.radarMapOpacity = constrain(config.radarMapOpacity, 0, 100);
  config.radarPauseSeconds = constrain(config.radarPauseSeconds, 0, 30);
  if (!std::isfinite(config.openMeteoLatitude) ||
      config.openMeteoLatitude < -90.0f || config.openMeteoLatitude > 90.0f ||
      !std::isfinite(config.openMeteoLongitude) ||
      config.openMeteoLongitude < -180.0f || config.openMeteoLongitude > 180.0f) {
    config.openMeteoLatitude = 49.1951f;
    config.openMeteoLongitude = 16.6068f;
    clockConfigCopy(config.timeZone, sizeof(config.timeZone), "Europe/Prague");
  }
  config.secondRingBackgroundColor &= 0xFFFFFF;
  config.secondDotColor &= 0xFFFFFF;
  config.leftSide.color &= 0xFFFFFF;
  config.rightSide.color &= 0xFFFFFF;
  for (ClockOpenMeteoSlotConfig &slot : config.openMeteoSlots) {
    slot.color &= 0xFFFFFF;
  }
  for (ClockTmepSlotConfig &slot : config.tmepSlots) {
    slot.decimals = constrain(slot.decimals, static_cast<uint8_t>(0),
                              static_cast<uint8_t>(2));
    if (slot.sensorId[0] == '\0' || slot.field[0] == '\0' ||
        slot.unit[0] == '\0') {
      slot = ClockTmepSlotConfig{};
    }
  }
  config.timeColor &= 0xFFFFFF;
  config.dateColor &= 0xFFFFFF;
  config.leftWeatherIconColor &= 0xFFFFFF;
  config.rightWeatherIconColor &= 0xFFFFFF;
  ClockMetricColorScale *scales[] = {
      &config.leftValueColorScale, &config.rightValueColorScale,
      &config.metricAColorScale, &config.metricBColorScale};
  for (ClockMetricColorScale *scale : scales) {
    scale->count = constrain(scale->count, static_cast<uint8_t>(1),
                             static_cast<uint8_t>(CLOCK_METRIC_COLOR_POINT_COUNT));
    for (uint8_t index = 0; index < scale->count; ++index) {
      scale->points[index].color &= 0xFFFFFF;
    }
    for (uint8_t index = 1; index < scale->count; ++index) {
      const ClockMetricColorPoint point = scale->points[index];
      uint8_t position = index;
      while (position > 0 &&
             scale->points[position - 1].value > point.value) {
        scale->points[position] = scale->points[position - 1];
        --position;
      }
      scale->points[position] = point;
    }
  }
}
}  // namespace

bool clockConfigRadarAvailable(const ClockConfig &config) {
  return config.openMeteoCountry == CLOCK_LOCATION_COUNTRY_CZECHIA;
}

bool clockAppearanceLoad(ClockAppearanceConfig &appearance,
                         uint32_t defaultMonochromeWeatherIconColor,
                         uint8_t defaultAnalogDateFormat,
                         uint32_t defaultAnalogDateColor) {
  appearance = ClockAppearanceConfig{};
  appearance.monochromeWeatherIconColor =
      defaultMonochromeWeatherIconColor & 0xFFFFFF;
  SettingsPreferences preferences;
  if (!preferences.begin(APPEARANCE_NAMESPACE, true, CONFIG_PARTITION))
    return false;
  appearance.style = constrain(
      preferences.getUChar(APPEARANCE_STYLE_KEY, CLOCK_STYLE_DIGITAL),
      static_cast<uint8_t>(CLOCK_STYLE_DIGITAL),
      static_cast<uint8_t>(CLOCK_STYLE_FORECAST));
  // The former experimental forecast face is now a separate page.
  if (appearance.style == CLOCK_STYLE_FORECAST) appearance.style = CLOCK_STYLE_DIGITAL;
  appearance.retroProgressSegments = constrain(preferences.getUChar("retroBarCount", 10), 5, 50);
  appearance.retroProgressSource = constrain(preferences.getUChar("retroBarSrc", 4), 0, 4);
  appearance.retroProgressMin = preferences.getFloat("retroBarMin", 0.0f);
  appearance.retroProgressMax = preferences.getFloat("retroBarMax", 100.0f);
  if (!std::isfinite(appearance.retroProgressMin) || !std::isfinite(appearance.retroProgressMax) ||
      appearance.retroProgressMin >= appearance.retroProgressMax) {
    appearance.retroProgressMin = 0.0f;
    appearance.retroProgressMax = 100.0f;
  }
  appearance.retroLeftSource = constrain(preferences.getUChar("retroLeft", 2), 0, 4);
  appearance.retroRightSource = constrain(preferences.getUChar("retroRight", 3), 0, 4);
  appearance.use12HourFormat = preferences.getBool("time12h", false);
  appearance.retroWeatherRaster = preferences.getBool("retroWxRaster", true);
  appearance.retroFixedWeekday = preferences.getBool("retroFixedDay", false);
  appearance.retroDateFormat = preferences.getUChar("retroDateFmt", 0);
  if (appearance.retroDateFormat > 13) appearance.retroDateFormat = 0;
  appearance.retroGhostOpacity = constrain(preferences.getUChar("retroGhost", 5), 0, 50);
  appearance.retroBackgroundColor = preferences.getUInt("retroBg", 0xB7C1A5) & 0xFFFFFF;
  appearance.retroForegroundColor = preferences.getUInt("retroFg", 0x20261C) & 0xFFFFFF;
  appearance.retroMetricADigits = constrain(preferences.getUChar("retroDigitsA", 3), 1, 4);
  appearance.retroMetricBDigits = constrain(preferences.getUChar("retroDigitsB", 4), 1, 4);
  appearance.animatedScreenTransitions = preferences.getBool("screenSlide", true);
  appearance.analogToneColor =
      preferences.getUInt(APPEARANCE_TONE_KEY, 0x00D6FF) & 0xFFFFFF;
  appearance.analogHandToneColor =
      preferences.getUInt(APPEARANCE_HAND_TONE_KEY,
                          appearance.analogToneColor) &
      0xFFFFFF;
  appearance.analogCardinalAccentColor =
      preferences.getUInt(APPEARANCE_ACCENT_COLOR_KEY, 0xFFAB00) &
      0xFFFFFF;
  appearance.analogCardinalAccentsEnabled =
      preferences.getBool(APPEARANCE_ACCENTS_KEY, true);
  appearance.analogOutlineHandsEnabled =
      preferences.getBool(APPEARANCE_OUTLINE_HANDS_KEY, false);
  appearance.analogMonochromeValuesEnabled =
      preferences.getBool(APPEARANCE_MONO_VALUES_KEY, false);
  appearance.analogValuesAboveHandsEnabled =
      preferences.getBool(APPEARANCE_VALUES_ABOVE_KEY, false);
  appearance.analogDateFormat = constrain(
      preferences.getUChar(APPEARANCE_DATE_FORMAT_KEY,
                           defaultAnalogDateFormat),
      static_cast<uint8_t>(CLOCK_DATE_FORMAT_WEEKDAY_DAY_MONTH),
      static_cast<uint8_t>(CLOCK_DATE_FORMAT_DAY_MONTH));
  appearance.analogDateColor =
      preferences.getUInt(APPEARANCE_DATE_COLOR_KEY,
                          defaultAnalogDateColor) &
      0xFFFFFF;
  appearance.monochromeWeatherIconColor =
      preferences.getUInt(APPEARANCE_WEATHER_COLOR_KEY,
                          defaultMonochromeWeatherIconColor) &
      0xFFFFFF;
  preferences.end();
  return true;
}

bool clockAppearanceSave(const ClockAppearanceConfig &appearance) {
  if (!std::isfinite(appearance.retroProgressMin) || !std::isfinite(appearance.retroProgressMax) ||
      appearance.retroProgressMin >= appearance.retroProgressMax || appearance.retroProgressSource > 4 ||
      appearance.retroProgressSegments < 5 || appearance.retroProgressSegments > 50) return false;

  const bool ownTransaction = !settingsTransactionActive();
  if (ownTransaction && !settingsTransactionBegin()) return false;
  SettingsPreferences preferences;
  if (!preferences.begin(APPEARANCE_NAMESPACE, false, CONFIG_PARTITION)) {
    if (ownTransaction) settingsTransactionAbort();
    return false;
  }
  const uint8_t style = constrain(
      appearance.style, static_cast<uint8_t>(CLOCK_STYLE_DIGITAL),
      static_cast<uint8_t>(CLOCK_STYLE_FORECAST));
  const bool styleSaved =
      preferences.putUChar(APPEARANCE_STYLE_KEY, style) == sizeof(style);
  const bool retroProgressSaved = preferences.putUChar("retroBarCount", appearance.retroProgressSegments) == sizeof(uint8_t) &&
      preferences.putUChar("retroBarSrc", appearance.retroProgressSource) == sizeof(uint8_t) &&
      preferences.putFloat("retroBarMin", appearance.retroProgressMin) == sizeof(float) &&
      preferences.putFloat("retroBarMax", appearance.retroProgressMax) == sizeof(float);
  const bool retroSourcesSaved = preferences.putUChar("retroLeft", constrain(appearance.retroLeftSource, 0, 4)) == sizeof(uint8_t) &&
      preferences.putUChar("retroRight", constrain(appearance.retroRightSource, 0, 4)) == sizeof(uint8_t);
  const bool timeFormatSaved = preferences.putBool("time12h", appearance.use12HourFormat) == sizeof(bool);
  const bool retroDateSaved = preferences.putUChar("retroDateFmt", appearance.retroDateFormat <= 13 ? appearance.retroDateFormat : 0) == sizeof(uint8_t);
  const bool retroWeekdaySaved = preferences.putBool("retroFixedDay", appearance.retroFixedWeekday) == sizeof(bool);
  const bool retroWeatherSaved = preferences.putBool("retroWxRaster", appearance.retroWeatherRaster) == sizeof(bool);
  const bool retroGhostSaved = preferences.putUChar("retroGhost", constrain(appearance.retroGhostOpacity, 0, 50)) == sizeof(uint8_t);
  const bool retroColorsSaved = preferences.putUInt("retroBg", appearance.retroBackgroundColor & 0xFFFFFF) == sizeof(uint32_t) &&
      preferences.putUInt("retroFg", appearance.retroForegroundColor & 0xFFFFFF) == sizeof(uint32_t);
  const bool digitsSavedA = preferences.putUChar("retroDigitsA", constrain(appearance.retroMetricADigits, 1, 4)) == sizeof(uint8_t);
  const bool digitsSavedB = preferences.putUChar("retroDigitsB", constrain(appearance.retroMetricBDigits, 1, 4)) == sizeof(uint8_t);
  const bool transitionSaved = preferences.putBool(
      "screenSlide", appearance.animatedScreenTransitions) == sizeof(bool);
  const bool toneSaved =
      preferences.putUInt(APPEARANCE_TONE_KEY,
                          appearance.analogToneColor & 0xFFFFFF) ==
      sizeof(uint32_t);
  const bool handToneSaved =
      preferences.putUInt(APPEARANCE_HAND_TONE_KEY,
                          appearance.analogHandToneColor & 0xFFFFFF) ==
      sizeof(uint32_t);
  const bool accentColorSaved =
      preferences.putUInt(APPEARANCE_ACCENT_COLOR_KEY,
                          appearance.analogCardinalAccentColor & 0xFFFFFF) ==
      sizeof(uint32_t);
  const bool accentsSaved =
      preferences.putBool(APPEARANCE_ACCENTS_KEY,
                          appearance.analogCardinalAccentsEnabled) ==
      sizeof(bool);
  const bool outlineHandsSaved =
      preferences.putBool(APPEARANCE_OUTLINE_HANDS_KEY,
                          appearance.analogOutlineHandsEnabled) ==
      sizeof(bool);
  const bool monoValuesSaved =
      preferences.putBool(APPEARANCE_MONO_VALUES_KEY,
                          appearance.analogMonochromeValuesEnabled) ==
      sizeof(bool);
  const bool valuesAboveSaved =
      preferences.putBool(APPEARANCE_VALUES_ABOVE_KEY,
                          appearance.analogValuesAboveHandsEnabled) ==
      sizeof(bool);
  const uint8_t dateFormat = constrain(
      appearance.analogDateFormat,
      static_cast<uint8_t>(CLOCK_DATE_FORMAT_WEEKDAY_DAY_MONTH),
      static_cast<uint8_t>(CLOCK_DATE_FORMAT_DAY_MONTH));
  const bool dateFormatSaved =
      preferences.putUChar(APPEARANCE_DATE_FORMAT_KEY, dateFormat) ==
      sizeof(dateFormat);
  const bool dateColorSaved =
      preferences.putUInt(APPEARANCE_DATE_COLOR_KEY,
                          appearance.analogDateColor & 0xFFFFFF) ==
      sizeof(uint32_t);
  const bool weatherColorSaved =
      preferences.putUInt(APPEARANCE_WEATHER_COLOR_KEY,
                          appearance.monochromeWeatherIconColor & 0xFFFFFF) ==
      sizeof(uint32_t);
  preferences.end();
  const bool ok = retroDateSaved && retroWeekdaySaved && timeFormatSaved && retroWeatherSaved && retroProgressSaved && retroSourcesSaved && retroGhostSaved && styleSaved && toneSaved && handToneSaved && accentColorSaved &&
         accentsSaved && outlineHandsSaved && monoValuesSaved &&
         valuesAboveSaved && dateFormatSaved && dateColorSaved &&
         weatherColorSaved && transitionSaved && digitsSavedA && digitsSavedB && retroColorsSaved;
  if (!ok) { if (ownTransaction) settingsTransactionAbort(); return false; }
  return !ownTransaction || settingsTransactionCommit();
}

void clockConfigCopy(char *destination, size_t destinationSize,
                     const String &value) {
  clockConfigCopy(destination, destinationSize, value.c_str());
}

void clockConfigCopy(char *destination, size_t destinationSize,
                     const char *value) {
  if (destinationSize == 0) return;
  strlcpy(destination, value == nullptr ? "" : value, destinationSize);
}

void clockConfigApplyDefaults(ClockConfig &config) {
  config = ClockConfig{};
  applyOpenMeteoDefaults(config);
  clockConfigCopy(config.leftSide.name, sizeof(config.leftSide.name), "VENKU");
  clockConfigCopy(config.leftSide.icon, sizeof(config.leftSide.icon), "weather");
  config.leftSide.color = 0x4CCBEC;
  clockConfigCopy(config.rightSide.name, sizeof(config.rightSide.name),
                  "MÍSTNOST");
  clockConfigCopy(config.rightSide.icon, sizeof(config.rightSide.icon), "home");
  config.rightSide.color = 0xFFB843;
  config.metricA.custom = false;
  clockConfigCopy(config.metricA.preset, sizeof(config.metricA.preset), "co2");
  clockConfigCopy(config.metricA.name, sizeof(config.metricA.name), "CO₂");
  clockConfigCopy(config.metricA.suffix, sizeof(config.metricA.suffix), "ppm");
  config.metricA.decimals = 0;

  config.metricB.custom = false;
  clockConfigCopy(config.metricB.preset, sizeof(config.metricB.preset),
                  "humidity");
  clockConfigCopy(config.metricB.name, sizeof(config.metricB.name), "VLHKOST");
  clockConfigCopy(config.metricB.suffix, sizeof(config.metricB.suffix), "%");
  config.metricB.decimals = 0;
  config.metricAColorScale = ClockMetricColorScale{};
  config.metricAColorScale.points[0] = {0.0f, 0x65C744};
  config.metricBColorScale = ClockMetricColorScale{};
  config.metricBColorScale.points[0] = {0.0f, 0xFFB843};
  applyLegacySideValueDefaults(config);
}

bool clockConfigBegin() {
  return nvs_flash_init_partition(CONFIG_PARTITION) == ESP_OK && settingsStoreBegin();
}

bool clockConfigSchemaSupported(uint32_t schema) {
  return schema == CLOCK_CONFIG_SCHEMA_VERSION || schema == 20 ||
         schema == 24 || schema == 25 || schema == 26 || schema == 27 || schema == 28 || schema == 29;
}

bool clockConfigValidate(const ClockConfig &c) {
  // Inspect bool representations before reading them; imported bytes are untrusted.
#define VALID_BOOL(field) if (*reinterpret_cast<const uint8_t *>(&c.field) > 1) return false
#define VALID_TEXT(field) if (!memchr(c.field, 0, sizeof(c.field))) return false
  VALID_BOOL(animatedWeatherIcons); VALID_BOOL(automaticDayNight);
  VALID_BOOL(automaticFirmwareUpdate); VALID_BOOL(secondRingEnabled);
  VALID_BOOL(showLeadingHourZero); VALID_BOOL(automaticRadarRotation);
  VALID_BOOL(metricA.custom); VALID_BOOL(metricB.custom);
  VALID_BOOL(leftValue.custom); VALID_BOOL(rightValue.custom);
  VALID_TEXT(homeAssistantUrl); VALID_TEXT(homeAssistantToken);
  VALID_TEXT(weatherEntityId); VALID_TEXT(sunEntityId); VALID_TEXT(dayNightLightEntityId);
  VALID_TEXT(leftSide.name); VALID_TEXT(leftSide.temperatureEntityId); VALID_TEXT(leftSide.icon);
  VALID_TEXT(rightSide.name); VALID_TEXT(rightSide.temperatureEntityId); VALID_TEXT(rightSide.icon);
  VALID_TEXT(metricA.preset); VALID_TEXT(metricA.name); VALID_TEXT(metricA.entityId); VALID_TEXT(metricA.suffix);
  VALID_TEXT(metricB.preset); VALID_TEXT(metricB.name); VALID_TEXT(metricB.entityId); VALID_TEXT(metricB.suffix);
  VALID_TEXT(leftValue.preset); VALID_TEXT(leftValue.suffix);
  VALID_TEXT(rightValue.preset); VALID_TEXT(rightValue.suffix);
  VALID_TEXT(openMeteoCity); VALID_TEXT(timeZone); VALID_TEXT(tmepExportKey); VALID_TEXT(tmepExportId);
  for (size_t i = 0; i < 4; ++i) {
    VALID_BOOL(tmepSlots[i].enabled);
    VALID_TEXT(tmepSlots[i].sensorId); VALID_TEXT(tmepSlots[i].field); VALID_TEXT(tmepSlots[i].unit);
    VALID_TEXT(openMeteoSlots[i].value); VALID_TEXT(openMeteoSlots[i].name);
    if (c.tmepSlots[i].decimals > 2 || c.openMeteoSlots[i].color > 0xFFFFFF) return false;
    if (c.tmepSlots[i].enabled && (!c.tmepSlots[i].sensorId[0] ||
        !c.tmepSlots[i].field[0] || !c.tmepSlots[i].unit[0])) return false;
  }
#undef VALID_BOOL
#undef VALID_TEXT
  if (c.schemaVersion != CLOCK_CONFIG_SCHEMA_VERSION || c.dataSource > 1 ||
      c.weatherIconStyle > 2 || c.nightVisualMode > 1 || c.timeFont > 3 ||
      c.dateFormat > 5 || c.timeColonEffect > 2 || c.secondEffect > 2 ||
      c.language > 2 || c.openMeteoCountry > 2 || c.dayBrightness < 1 || c.dayBrightness > 100 ||
      c.nightBrightness < 1 || c.nightBrightness > 100 ||
      c.sunriseOffsetMinutes < -60 || c.sunriseOffsetMinutes > 60 || c.sunriseOffsetMinutes % 15 ||
      c.sunsetOffsetMinutes < -60 || c.sunsetOffsetMinutes > 60 || c.sunsetOffsetMinutes % 15 ||
      c.metricA.decimals > 2 || c.metricB.decimals > 2 || c.leftValue.decimals > 2 || c.rightValue.decimals > 2 ||
      c.radarFrameCount < 1 || c.radarFrameCount > 15 || c.radarMapOpacity > 100 || c.radarPauseSeconds > 30 ||
      c.clockDisplaySeconds > 3600 ||
      c.radarDisplaySeconds > 3600 || c.forecastDisplaySeconds > 3600 ||
      c.secondRingBackgroundDotSize < 1 || c.secondRingBackgroundDotSize > 10 ||
      c.secondDotSize < 1 || c.secondDotSize > 10) return false;
  if (c.radarRadiusKm != 0 && c.radarRadiusKm != 25 && c.radarRadiusKm != 50 &&
      c.radarRadiusKm != 100 && c.radarRadiusKm != 200) return false;
  if (!std::isfinite(c.openMeteoLatitude) || c.openMeteoLatitude < -90 || c.openMeteoLatitude > 90 ||
      !std::isfinite(c.openMeteoLongitude) || c.openMeteoLongitude < -180 || c.openMeteoLongitude > 180 ||
      !c.openMeteoCity[0] || (c.timeZone[0] && !clockTimezoneSupported(c.timeZone))) return false;
  if (c.homeAssistantUrl[0] && strncmp(c.homeAssistantUrl, "http://", 7) &&
      strncmp(c.homeAssistantUrl, "https://", 8)) return false;
  const uint32_t colors[] = {c.timeColor, c.dateColor, c.leftWeatherIconColor, c.rightWeatherIconColor,
      c.leftSide.color, c.rightSide.color, c.secondRingBackgroundColor, c.secondDotColor};
  for (uint32_t color : colors) if (color > 0xFFFFFF) return false;
  for (const ClockMetricColorScale *scale : {&c.metricAColorScale, &c.metricBColorScale,
                                            &c.leftValueColorScale, &c.rightValueColorScale}) {
    if (!scale->count || scale->count > CLOCK_METRIC_COLOR_POINT_COUNT) return false;
    for (size_t i = 0; i < scale->count; ++i) {
      if (!std::isfinite(scale->points[i].value) || scale->points[i].color > 0xFFFFFF) return false;
      for (size_t j = 0; j < i; ++j)
        if (scale->points[j].value == scale->points[i].value) return false;
    }
  }
  return true;
}

bool clockConfigLoad(ClockConfig &config) {
  SettingsPreferences preferences;
  if (!preferences.begin(CONFIG_NAMESPACE, true, CONFIG_PARTITION)) return false;
  static uint8_t record[sizeof(ConfigRecord)];
  const size_t size = preferences.getBytesLength(CONFIG_KEY);
  if (size == 0) {
    clockConfigApplyDefaults(config);
    return clockConfigSave(config);
  }
  if (size > sizeof(record) || preferences.getBytes(CONFIG_KEY, record, sizeof(record)) != size)
    return false;
  if (!clockConfigDecodeRecord(record, size, config)) return false;
  uint32_t schema;
  memcpy(&schema, record + 4, sizeof(schema));
  return schema == CLOCK_CONFIG_SCHEMA_VERSION || clockConfigSave(config);
}

bool clockConfigDecodeRecord(const void *data, size_t storedSize, ClockConfig &config) {
  clockConfigApplyDefaults(config);
  static ConfigRecord record;
  record = ConfigRecord{};
  const bool supportedSize = storedSize == sizeof(record) ||
      storedSize == sizeof(ConfigRecordV29) || storedSize == sizeof(ConfigRecordV28) || storedSize == sizeof(ConfigRecordV27) ||
      storedSize == sizeof(ConfigRecordV26) || storedSize == sizeof(ConfigRecordV155);
  const bool readComplete = data && supportedSize;
  if (!readComplete) return false;
  memcpy(&record, data, storedSize);
  const bool currentRecord =
      readComplete && storedSize == sizeof(record) &&
      record.magic == CONFIG_MAGIC &&
      record.schemaVersion == CLOCK_CONFIG_SCHEMA_VERSION &&
      record.config.schemaVersion == CLOCK_CONFIG_SCHEMA_VERSION &&
      record.checksum == configChecksum(record.config);
  if (currentRecord) {
    config = record.config;
    if (!clockConfigValidate(config)) return false;
    normalizeConfig(config);
    return true;
  }

  const ConfigRecordV29 &legacyV29 = *reinterpret_cast<const ConfigRecordV29 *>(&record);
  uint32_t embeddedSchemaV29 = 0;
  if (storedSize == sizeof(legacyV29)) memcpy(&embeddedSchemaV29, legacyV29.config, 4);
  if (storedSize == sizeof(legacyV29) && legacyV29.magic == CONFIG_MAGIC &&
      legacyV29.schemaVersion == 29 && embeddedSchemaV29 == 29 &&
      legacyV29.checksum == bytesChecksum(legacyV29.config, sizeof(legacyV29.config))) {
    memcpy(&config, legacyV29.config, sizeof(legacyV29.config));
    config.schemaVersion = CLOCK_CONFIG_SCHEMA_VERSION;
    if (!clockConfigValidate(config)) return false;
    normalizeConfig(config);
    return true;
  }

  // Older records contain a location, but no zone. Resolve it asynchronously
  // from its coordinates; keep the previous Czech clock until that succeeds.
  config.timeZone[0] = '\0';
  const ConfigRecordV28 &legacyV28 =
      *reinterpret_cast<const ConfigRecordV28 *>(&record);
  uint32_t embeddedSchemaV28 = 0;
  if (readComplete && storedSize == sizeof(legacyV28))
    memcpy(&embeddedSchemaV28, legacyV28.config, sizeof(embeddedSchemaV28));
  if (readComplete && storedSize == sizeof(legacyV28) &&
      legacyV28.magic == CONFIG_MAGIC && legacyV28.schemaVersion == 28 &&
      embeddedSchemaV28 == 28 &&
      legacyV28.checksum == bytesChecksum(legacyV28.config, sizeof(legacyV28.config))) {
    memcpy(&config, legacyV28.config, sizeof(legacyV28.config));
    config.schemaVersion = CLOCK_CONFIG_SCHEMA_VERSION;
    if (!clockConfigValidate(config)) return false;
    normalizeConfig(config);
    return true;
  }

  const ConfigRecordV27 &legacyV27 =
      *reinterpret_cast<const ConfigRecordV27 *>(&record);
  uint32_t embeddedSchemaV27 = 0;
  if (readComplete && storedSize == sizeof(legacyV27)) {
    memcpy(&embeddedSchemaV27, legacyV27.config,
           sizeof(embeddedSchemaV27));
  }
  const bool validSchema27Record =
      readComplete && storedSize == sizeof(legacyV27) &&
      legacyV27.magic == CONFIG_MAGIC &&
      legacyV27.schemaVersion == SIDE_VALUES_PREDECESSOR_SCHEMA_VERSION &&
      embeddedSchemaV27 == SIDE_VALUES_PREDECESSOR_SCHEMA_VERSION &&
      legacyV27.checksum ==
          bytesChecksum(legacyV27.config, sizeof(legacyV27.config));
  if (validSchema27Record) {
    memcpy(&config, legacyV27.config, sizeof(legacyV27.config));
    config.schemaVersion = CLOCK_CONFIG_SCHEMA_VERSION;
    applyLegacySideValueDefaults(config);
    if (!clockConfigValidate(config)) return false;
    normalizeConfig(config);
    return true;
  }

  const ConfigRecordV26 &legacyV26 =
      *reinterpret_cast<const ConfigRecordV26 *>(&record);
  uint32_t embeddedSchemaV26 = 0;
  if (readComplete && storedSize == sizeof(legacyV26)) {
    memcpy(&embeddedSchemaV26, legacyV26.config,
           sizeof(embeddedSchemaV26));
  }
  const bool validSchema26Prefix =
      readComplete && storedSize == sizeof(legacyV26) &&
      legacyV26.magic == CONFIG_MAGIC &&
      legacyV26.schemaVersion >= RADAR_SCHEMA_VERSION &&
      legacyV26.schemaVersion <= TMEP_PREDECESSOR_SCHEMA_VERSION &&
      embeddedSchemaV26 == legacyV26.schemaVersion &&
      legacyV26.checksum ==
          bytesChecksum(legacyV26.config, sizeof(legacyV26.config));

  if (validSchema26Prefix &&
      legacyV26.schemaVersion == TMEP_PREDECESSOR_SCHEMA_VERSION) {
    memcpy(&config, legacyV26.config, sizeof(legacyV26.config));
    config.schemaVersion = CLOCK_CONFIG_SCHEMA_VERSION;
    config.tmepExportKey[0] = '\0';
    config.tmepExportId[0] = '\0';
    for (ClockTmepSlotConfig &slot : config.tmepSlots)
      slot = ClockTmepSlotConfig{};
    if (!clockConfigValidate(config)) return false;
    normalizeConfig(config);
    return true;
  }

  // Schema 25 used 0 for Czech and 1 for English. Preserve that explicit
  // choice while migrating to the tri-state representation.
  const bool validLanguageRecord =
      validSchema26Prefix &&
      legacyV26.schemaVersion == LANGUAGE_SCHEMA_VERSION;
  if (validLanguageRecord) {
    memcpy(&config, legacyV26.config, sizeof(legacyV26.config));
    const uint8_t legacyLanguage = config.language;
    config.schemaVersion = CLOCK_CONFIG_SCHEMA_VERSION;
    config.language = legacyLanguage == 1 ? CLOCK_LANGUAGE_ENGLISH
                                          : CLOCK_LANGUAGE_CZECH;
    config.openMeteoCountry = CLOCK_LOCATION_COUNTRY_CZECHIA;
    config.tmepExportKey[0] = '\0';
    config.tmepExportId[0] = '\0';
    for (ClockTmepSlotConfig &slot : config.tmepSlots)
      slot = ClockTmepSlotConfig{};
    if (!clockConfigValidate(config)) return false;
    normalizeConfig(config);
    return true;
  }

  // Schema 24 has the same binary size. The language byte occupied trailing
  // padding, so the old checksum can be verified before migration.
  const bool validRadarRecord =
      validSchema26Prefix && legacyV26.schemaVersion == RADAR_SCHEMA_VERSION;
  if (validRadarRecord) {
    memcpy(&config, legacyV26.config, sizeof(legacyV26.config));
    config.schemaVersion = CLOCK_CONFIG_SCHEMA_VERSION;
    config.language = CLOCK_LANGUAGE_UNSET;
    config.openMeteoCountry = CLOCK_LOCATION_COUNTRY_CZECHIA;
    config.tmepExportKey[0] = '\0';
    config.tmepExportId[0] = '\0';
    for (ClockTmepSlotConfig &slot : config.tmepSlots)
      slot = ClockTmepSlotConfig{};
    if (!clockConfigValidate(config)) return false;
    normalizeConfig(config);
    return true;
  }

  const ConfigRecordV155 &legacy =
      *reinterpret_cast<const ConfigRecordV155 *>(&record);
  uint32_t embeddedSchemaVersion = 0;
  if (readComplete && storedSize == sizeof(legacy)) {
    memcpy(&embeddedSchemaVersion, legacy.config,
           sizeof(embeddedSchemaVersion));
  }
  const bool validPublic155Record =
      readComplete && storedSize == sizeof(legacy) &&
      legacy.magic == CONFIG_MAGIC &&
      legacy.schemaVersion == PUBLIC_1_5_5_SCHEMA_VERSION &&
      embeddedSchemaVersion == PUBLIC_1_5_5_SCHEMA_VERSION &&
      legacy.checksum == bytesChecksum(legacy.config, sizeof(legacy.config));
  if (!validPublic155Record) {
    return false;
  }

  memcpy(&config, legacy.config, sizeof(legacy.config));
  config.schemaVersion = CLOCK_CONFIG_SCHEMA_VERSION;
  config.radarRadiusKm = 0;
  config.radarFrameCount = 6;
  config.automaticRadarRotation = false;
  config.clockDisplaySeconds = 120;
  config.radarDisplaySeconds = 20;
  config.forecastDisplaySeconds = 20;
  config.radarMapOpacity = 100;
  config.radarPauseSeconds = 5;
  config.language = CLOCK_LANGUAGE_UNSET;
  config.openMeteoCountry = CLOCK_LOCATION_COUNTRY_CZECHIA;
  config.tmepExportKey[0] = '\0';
  config.tmepExportId[0] = '\0';
  for (ClockTmepSlotConfig &slot : config.tmepSlots)
    slot = ClockTmepSlotConfig{};
  if (!clockConfigValidate(config)) return false;
  normalizeConfig(config);
  return true;
}

bool clockConfigSave(const ClockConfig &config) {
  static ConfigRecord record;
  record = ConfigRecord{};
  record.magic = CONFIG_MAGIC;
  record.schemaVersion = CLOCK_CONFIG_SCHEMA_VERSION;
  record.config = config;
  normalizeConfig(record.config);
  record.checksum = configChecksum(record.config);

  SettingsPreferences preferences;
  if (!preferences.begin(CONFIG_NAMESPACE, false, CONFIG_PARTITION)) return false;
  bool ok =
      preferences.putBytes(CONFIG_KEY, &record, sizeof(record)) == sizeof(record);
  preferences.end();
  return ok;
}
