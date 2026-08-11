#include "ClockConfig.h"

#include <Preferences.h>
#include <nvs_flash.h>

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

constexpr uint32_t LEGACY_SCHEMA_VERSION = 14;
constexpr uint32_t SCHEMA_15_VERSION = 15;
constexpr uint32_t PREVIOUS_SCHEMA_VERSION = 16;
constexpr size_t SCHEMA_15_CONFIG_SIZE = offsetof(ClockConfig, nightVisualMode);
constexpr size_t PREVIOUS_CONFIG_SIZE = offsetof(ClockConfig, timeFont);
constexpr size_t LEGACY_CONFIG_SIZE =
    offsetof(ClockConfig, dayNightLightEntityId);

struct ConfigRecordV14 {
  uint32_t magic;
  uint32_t schemaVersion;
  uint8_t config[LEGACY_CONFIG_SIZE];
  uint32_t checksum;
};

struct ConfigRecordV15 {
  uint32_t magic;
  uint32_t schemaVersion;
  uint8_t config[SCHEMA_15_CONFIG_SIZE];
  uint32_t checksum;
};

struct ConfigRecordV16 {
  uint32_t magic;
  uint32_t schemaVersion;
  uint8_t config[PREVIOUS_CONFIG_SIZE];
  uint32_t checksum;
};

static_assert(offsetof(ClockConfig, dayNightLightEntityId) <=
                      LEGACY_CONFIG_SIZE &&
                  LEGACY_CONFIG_SIZE -
                          offsetof(ClockConfig, dayNightLightEntityId) <
                      alignof(ClockConfig),
              "Nové konfigurační pole musí zůstat za schématem 14.");

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
  config.secondRingBackgroundColor &= 0xFFFFFF;
  config.secondDotColor &= 0xFFFFFF;
  config.leftSide.color &= 0xFFFFFF;
  config.rightSide.color &= 0xFFFFFF;
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

  // Záznamy jsou velké (obsahují celé ClockConfig). Nesmějí být současně na
  // malém zásobníku Arduino loopTask, zejména během migrace více schémat.
  static ConfigRecord record;
  record = ConfigRecord{};
  const size_t storedSize = preferences.getBytesLength(CONFIG_KEY);
  const bool readComplete = storedSize == sizeof(record) &&
                            preferences.getBytes(CONFIG_KEY, &record,
                                                 sizeof(record)) == sizeof(record);
  static ConfigRecordV14 legacyRecord;
  legacyRecord = ConfigRecordV14{};
  const bool legacyReadComplete =
      storedSize == sizeof(legacyRecord) &&
      preferences.getBytes(CONFIG_KEY, &legacyRecord, sizeof(legacyRecord)) ==
          sizeof(legacyRecord);
  static ConfigRecordV15 previousRecord;
  previousRecord = ConfigRecordV15{};
  const bool schema15ReadComplete =
      storedSize == sizeof(previousRecord) &&
      preferences.getBytes(CONFIG_KEY, &previousRecord,
                           sizeof(previousRecord)) == sizeof(previousRecord);
  static ConfigRecordV16 schema16Record;
  schema16Record = ConfigRecordV16{};
  const bool previousReadComplete =
      storedSize == sizeof(schema16Record) &&
      preferences.getBytes(CONFIG_KEY, &schema16Record,
                           sizeof(schema16Record)) == sizeof(schema16Record);
  preferences.end();

  const bool currentRecord =
      readComplete && record.magic == CONFIG_MAGIC &&
      record.schemaVersion == CLOCK_CONFIG_SCHEMA_VERSION &&
      record.config.schemaVersion == CLOCK_CONFIG_SCHEMA_VERSION &&
      record.checksum == configChecksum(record.config);
  if (!currentRecord) {
    const bool validSameSizePreviousRecord =
        readComplete && record.magic == CONFIG_MAGIC &&
        record.schemaVersion == PREVIOUS_SCHEMA_VERSION &&
        record.config.schemaVersion == PREVIOUS_SCHEMA_VERSION &&
        record.checksum == configChecksum(record.config);
    if (validSameSizePreviousRecord) {
      config = record.config;
      config.schemaVersion = CLOCK_CONFIG_SCHEMA_VERSION;
      config.timeFont = CLOCK_TIME_FONT_BARLOW;
      normalizeConfig(config);
      return clockConfigSave(config);
    }

    const bool validSameSizeSchema15Record =
        readComplete && record.magic == CONFIG_MAGIC &&
        record.schemaVersion == SCHEMA_15_VERSION &&
        record.config.schemaVersion == SCHEMA_15_VERSION &&
        record.checksum == configChecksum(record.config);
    if (validSameSizeSchema15Record) {
      config = record.config;
      config.schemaVersion = CLOCK_CONFIG_SCHEMA_VERSION;
      config.nightVisualMode = CLOCK_NIGHT_VISUAL_RED;
      config.timeFont = CLOCK_TIME_FONT_BARLOW;
      normalizeConfig(config);
      return clockConfigSave(config);
    }

    uint32_t previousEmbeddedSchemaVersion = 0;
    memcpy(&previousEmbeddedSchemaVersion, schema16Record.config,
           sizeof(previousEmbeddedSchemaVersion));
    const bool validPreviousRecord =
        previousReadComplete && schema16Record.magic == CONFIG_MAGIC &&
        schema16Record.schemaVersion == PREVIOUS_SCHEMA_VERSION &&
        previousEmbeddedSchemaVersion == PREVIOUS_SCHEMA_VERSION &&
        schema16Record.checksum == bytesChecksum(schema16Record.config,
                                                 sizeof(schema16Record.config));
    if (validPreviousRecord) {
      memcpy(&config, schema16Record.config, sizeof(schema16Record.config));
      config.schemaVersion = CLOCK_CONFIG_SCHEMA_VERSION;
      config.timeFont = CLOCK_TIME_FONT_BARLOW;
      normalizeConfig(config);
      return clockConfigSave(config);
    }

    uint32_t schema15EmbeddedSchemaVersion = 0;
    memcpy(&schema15EmbeddedSchemaVersion, previousRecord.config,
           sizeof(schema15EmbeddedSchemaVersion));
    const bool validSchema15Record =
        schema15ReadComplete && previousRecord.magic == CONFIG_MAGIC &&
        previousRecord.schemaVersion == SCHEMA_15_VERSION &&
        schema15EmbeddedSchemaVersion == SCHEMA_15_VERSION &&
        previousRecord.checksum == bytesChecksum(previousRecord.config,
                                                 sizeof(previousRecord.config));
    if (validSchema15Record) {
      memcpy(&config, previousRecord.config, sizeof(previousRecord.config));
      config.schemaVersion = CLOCK_CONFIG_SCHEMA_VERSION;
      config.nightVisualMode = CLOCK_NIGHT_VISUAL_RED;
      config.timeFont = CLOCK_TIME_FONT_BARLOW;
      normalizeConfig(config);
      return clockConfigSave(config);
    }

    uint32_t embeddedSchemaVersion = 0;
    memcpy(&embeddedSchemaVersion, legacyRecord.config,
           sizeof(embeddedSchemaVersion));
    const bool validLegacyRecord =
        legacyReadComplete && legacyRecord.magic == CONFIG_MAGIC &&
        legacyRecord.schemaVersion == LEGACY_SCHEMA_VERSION &&
        embeddedSchemaVersion == LEGACY_SCHEMA_VERSION &&
        legacyRecord.checksum ==
            bytesChecksum(legacyRecord.config, sizeof(legacyRecord.config));
    if (!validLegacyRecord) return clockConfigSave(config);

    memcpy(&config, legacyRecord.config, sizeof(legacyRecord.config));
    config.schemaVersion = CLOCK_CONFIG_SCHEMA_VERSION;
    config.dayNightLightEntityId[0] = '\0';
    config.nightVisualMode = CLOCK_NIGHT_VISUAL_RED;
    config.timeFont = CLOCK_TIME_FONT_BARLOW;
    normalizeConfig(config);
    return clockConfigSave(config);
  }

  config = record.config;
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

  Preferences preferences;
  if (!preferences.begin(CONFIG_NAMESPACE, false, CONFIG_PARTITION)) return false;
  const bool ok =
      preferences.putBytes(CONFIG_KEY, &record, sizeof(record)) == sizeof(record);
  preferences.end();
  return ok;
}
