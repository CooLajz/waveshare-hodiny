#include "ClockConfig.h"

#include <Preferences.h>
#include <nvs_flash.h>

#include <cmath>

namespace {
constexpr uint32_t CONFIG_MAGIC = 0x57484346;
constexpr char CONFIG_PARTITION[] = "clockcfg";
constexpr char CONFIG_NAMESPACE[] = "clock-config";
constexpr char CONFIG_KEY[] = "config";

struct ConfigRecord {
  uint32_t magic;
  uint32_t schemaVersion;
  ClockConfig config;
  uint32_t checksum;
};

constexpr uint32_t PUBLIC_1_5_5_SCHEMA_VERSION = 20;

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

void applyOpenMeteoDefaults(ClockConfig &config) {
  config.dataSource = CLOCK_DATA_SOURCE_OPEN_METEO;
  clockConfigCopy(config.openMeteoCity, sizeof(config.openMeteoCity), "Brno");
  config.openMeteoLatitude = 49.1951f;
  config.openMeteoLongitude = 16.6068f;
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
static_assert(sizeof(ConfigRecordV155) <= sizeof(ConfigRecord),
              "Migrační záznam se musí vejít do společného pracovního bufferu.");

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
  config.dayBrightness = constrain(config.dayBrightness, 1, 100);
  config.nightBrightness = constrain(config.nightBrightness, 1, 100);
  config.sunriseOffsetMinutes = constrain(config.sunriseOffsetMinutes, -60, 60);
  config.sunsetOffsetMinutes = constrain(config.sunsetOffsetMinutes, -60, 60);
  config.metricA.decimals = constrain(config.metricA.decimals, 0, 2);
  config.metricB.decimals = constrain(config.metricB.decimals, 0, 2);
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
      static_cast<uint8_t>(CLOCK_DATE_FORMAT_HIDDEN));
  config.dataSource = constrain(
      config.dataSource, static_cast<uint8_t>(CLOCK_DATA_SOURCE_OPEN_METEO),
      static_cast<uint8_t>(CLOCK_DATA_SOURCE_HOME_ASSISTANT));
  if (config.radarRadiusKm != 0 && config.radarRadiusKm != 25 &&
      config.radarRadiusKm != 50 &&
      config.radarRadiusKm != 100 && config.radarRadiusKm != 200) {
    config.radarRadiusKm = 50;
  }
  config.radarFrameCount = constrain(config.radarFrameCount, 1, 15);
  config.clockDisplaySeconds =
      constrain(config.clockDisplaySeconds, 10, 3600);
  config.radarDisplaySeconds =
      constrain(config.radarDisplaySeconds, 10, 3600);
  config.radarMapOpacity = constrain(config.radarMapOpacity, 0, 100);
  config.radarPauseSeconds = constrain(config.radarPauseSeconds, 0, 30);
  if (!std::isfinite(config.openMeteoLatitude) ||
      config.openMeteoLatitude < -90.0f || config.openMeteoLatitude > 90.0f ||
      !std::isfinite(config.openMeteoLongitude) ||
      config.openMeteoLongitude < -180.0f || config.openMeteoLongitude > 180.0f) {
    config.openMeteoLatitude = 49.1951f;
    config.openMeteoLongitude = 16.6068f;
  }
  config.secondRingBackgroundColor &= 0xFFFFFF;
  config.secondDotColor &= 0xFFFFFF;
  config.leftSide.color &= 0xFFFFFF;
  config.rightSide.color &= 0xFFFFFF;
  for (ClockOpenMeteoSlotConfig &slot : config.openMeteoSlots) {
    slot.color &= 0xFFFFFF;
  }
  config.timeColor &= 0xFFFFFF;
  config.dateColor &= 0xFFFFFF;
  config.leftWeatherIconColor &= 0xFFFFFF;
  config.rightWeatherIconColor &= 0xFFFFFF;
  ClockMetricColorScale *scales[] = {&config.metricAColorScale,
                                    &config.metricBColorScale};
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
}

bool clockConfigBegin() {
  return nvs_flash_init_partition(CONFIG_PARTITION) == ESP_OK;
}

bool clockConfigLoad(ClockConfig &config) {
  clockConfigApplyDefaults(config);
  Preferences preferences;
  if (!preferences.begin(CONFIG_NAMESPACE, false, CONFIG_PARTITION)) return false;

  // Aktuální i jediný podporovaný migrační záznam sdílejí jeden statický
  // buffer. Konfigurace je velká a nemá ležet na zásobníku loopTask.
  static ConfigRecord record;
  record = ConfigRecord{};
  const size_t storedSize = preferences.getBytesLength(CONFIG_KEY);
  const bool supportedSize =
      storedSize == sizeof(record) || storedSize == sizeof(ConfigRecordV155);
  const bool readComplete =
      supportedSize && preferences.getBytes(CONFIG_KEY, &record, storedSize) ==
                           storedSize;
  preferences.end();

  const bool currentRecord =
      readComplete && storedSize == sizeof(record) &&
      record.magic == CONFIG_MAGIC &&
      record.schemaVersion == CLOCK_CONFIG_SCHEMA_VERSION &&
      record.config.schemaVersion == CLOCK_CONFIG_SCHEMA_VERSION &&
      record.checksum == configChecksum(record.config);
  if (currentRecord) {
    config = record.config;
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
  if (!validPublic155Record) return clockConfigSave(config);

  memcpy(&config, legacy.config, sizeof(legacy.config));
  config.schemaVersion = CLOCK_CONFIG_SCHEMA_VERSION;
  config.radarRadiusKm = 50;
  config.radarFrameCount = 6;
  config.automaticRadarRotation = false;
  config.clockDisplaySeconds = 120;
  config.radarDisplaySeconds = 20;
  config.radarMapOpacity = 100;
  config.radarPauseSeconds = 5;
  normalizeConfig(config);
  return clockConfigSave(config);
}

bool clockConfigSave(const ClockConfig &config) {
  static ConfigRecord record;
  record = ConfigRecord{};
  record.magic = CONFIG_MAGIC;
  record.schemaVersion = CLOCK_CONFIG_SCHEMA_VERSION;
  record.config = config;
  normalizeConfig(record.config);
  record.checksum = configChecksum(record.config);

  Preferences preferences;
  if (!preferences.begin(CONFIG_NAMESPACE, false, CONFIG_PARTITION)) return false;
  bool ok =
      preferences.putBytes(CONFIG_KEY, &record, sizeof(record)) == sizeof(record);
  if (!ok && preferences.remove(CONFIG_KEY)) {
    // Velký konfigurační blob při mnoha změnách schématu může zaplnit NVS
    // historickými verzemi. Odstranění pouze tohoto klíče umožní NVS staré
    // blobové stránky zkompaktovat; ostatní namespace v clockcfg zůstávají.
    preferences.end();
    if (!preferences.begin(CONFIG_NAMESPACE, false, CONFIG_PARTITION))
      return false;
    ok = preferences.putBytes(CONFIG_KEY, &record, sizeof(record)) ==
         sizeof(record);
  }
  preferences.end();
  return ok;
}
