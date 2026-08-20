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

constexpr uint32_t PUBLIC_1_4_SCHEMA_VERSION = 16;
constexpr uint32_t OPEN_METEO_SCHEMA_VERSION = 17;
constexpr uint32_t BOOLEAN_COLON_SCHEMA_VERSION = 18;
constexpr size_t SCHEMA_16_PAYLOAD_SIZE = offsetof(ClockConfig, timeFont);
constexpr size_t SCHEMA_16_CONFIG_SIZE =
    (SCHEMA_16_PAYLOAD_SIZE + alignof(ClockConfig) - 1) &
    ~(alignof(ClockConfig) - 1);

struct ConfigRecordV16 {
  uint32_t magic;
  uint32_t schemaVersion;
  uint8_t config[SCHEMA_16_CONFIG_SIZE];
  uint32_t checksum;
};

constexpr size_t SCHEMA_17_PAYLOAD_SIZE =
    offsetof(ClockConfig, timeColonEffect);
constexpr size_t SCHEMA_17_CONFIG_SIZE =
    (SCHEMA_17_PAYLOAD_SIZE + alignof(ClockConfig) - 1) &
    ~(alignof(ClockConfig) - 1);

struct ConfigRecordV17 {
  uint32_t magic;
  uint32_t schemaVersion;
  uint8_t config[SCHEMA_17_CONFIG_SIZE];
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

static_assert(SCHEMA_16_CONFIG_SIZE % alignof(ClockConfig) == 0,
              "Záznam veřejné verze 1.4.0 musí zahrnout koncový padding.");
static_assert(SCHEMA_17_CONFIG_SIZE % alignof(ClockConfig) == 0,
              "Záznam schema 17 musí zahrnout koncový padding.");

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
  config.dataSource = constrain(
      config.dataSource, static_cast<uint8_t>(CLOCK_DATA_SOURCE_OPEN_METEO),
      static_cast<uint8_t>(CLOCK_DATA_SOURCE_HOME_ASSISTANT));
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

  // Záznamy jsou velké (obsahují celé ClockConfig), proto neleží na malém
  // zásobníku Arduino loopTask.
  static ConfigRecord record;
  record = ConfigRecord{};
  const size_t storedSize = preferences.getBytesLength(CONFIG_KEY);
  const bool readComplete = storedSize == sizeof(record) &&
                            preferences.getBytes(CONFIG_KEY, &record,
                                                 sizeof(record)) == sizeof(record);
  static ConfigRecordV16 schema16Record;
  schema16Record = ConfigRecordV16{};
  const bool schema16ReadComplete =
      storedSize == sizeof(schema16Record) &&
      preferences.getBytes(CONFIG_KEY, &schema16Record,
                           sizeof(schema16Record)) == sizeof(schema16Record);
  static ConfigRecordV17 schema17Record;
  schema17Record = ConfigRecordV17{};
  const bool schema17ReadComplete =
      storedSize == sizeof(schema17Record) &&
      preferences.getBytes(CONFIG_KEY, &schema17Record,
                           sizeof(schema17Record)) == sizeof(schema17Record);
  preferences.end();

  const bool currentRecord =
      readComplete && record.magic == CONFIG_MAGIC &&
      record.schemaVersion == CLOCK_CONFIG_SCHEMA_VERSION &&
      record.config.schemaVersion == CLOCK_CONFIG_SCHEMA_VERSION &&
      record.checksum == configChecksum(record.config);
  if (!currentRecord) {
    const bool validSchema18Record =
        readComplete && record.magic == CONFIG_MAGIC &&
        record.schemaVersion == BOOLEAN_COLON_SCHEMA_VERSION &&
        record.config.schemaVersion == BOOLEAN_COLON_SCHEMA_VERSION &&
        record.checksum == configChecksum(record.config);
    if (validSchema18Record) {
      config = record.config;
      config.schemaVersion = CLOCK_CONFIG_SCHEMA_VERSION;
      config.timeColonEffect = config.timeColonEffect
                                   ? CLOCK_TIME_COLON_FADE
                                   : CLOCK_TIME_COLON_STEADY;
      normalizeConfig(config);
      return clockConfigSave(config);
    }

    uint32_t schema17EmbeddedVersion = 0;
    memcpy(&schema17EmbeddedVersion, schema17Record.config,
           sizeof(schema17EmbeddedVersion));
    const bool validSchema17Record =
        schema17ReadComplete && schema17Record.magic == CONFIG_MAGIC &&
        schema17Record.schemaVersion == OPEN_METEO_SCHEMA_VERSION &&
        schema17EmbeddedVersion == OPEN_METEO_SCHEMA_VERSION &&
        schema17Record.checksum ==
            bytesChecksum(schema17Record.config, sizeof(schema17Record.config));
    if (validSchema17Record) {
      memcpy(&config, schema17Record.config, SCHEMA_17_PAYLOAD_SIZE);
      config.schemaVersion = CLOCK_CONFIG_SCHEMA_VERSION;
      config.timeColonEffect = CLOCK_TIME_COLON_STEADY;
      config.showLeadingHourZero = true;
      normalizeConfig(config);
      return clockConfigSave(config);
    }

    uint32_t embeddedSchemaVersion = 0;
    memcpy(&embeddedSchemaVersion, schema16Record.config,
           sizeof(embeddedSchemaVersion));
    const bool validPublic14Record =
        schema16ReadComplete && schema16Record.magic == CONFIG_MAGIC &&
        schema16Record.schemaVersion == PUBLIC_1_4_SCHEMA_VERSION &&
        embeddedSchemaVersion == PUBLIC_1_4_SCHEMA_VERSION &&
        schema16Record.checksum == bytesChecksum(schema16Record.config,
                                                 sizeof(schema16Record.config));
    if (!validPublic14Record) return clockConfigSave(config);

    memcpy(&config, schema16Record.config, SCHEMA_16_PAYLOAD_SIZE);
    config.schemaVersion = CLOCK_CONFIG_SCHEMA_VERSION;
    config.timeFont = CLOCK_TIME_FONT_BARLOW;
    applyOpenMeteoDefaults(config);
    if (config.homeAssistantUrl[0] != '\0' &&
        config.homeAssistantToken[0] != '\0') {
      config.dataSource = CLOCK_DATA_SOURCE_HOME_ASSISTANT;
    }
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
