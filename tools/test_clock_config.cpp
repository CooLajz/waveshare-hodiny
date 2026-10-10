#include <cassert>
#include <cstdio>
#include <vector>
#include "backup_test_support/Preferences.h"
#include "../WaveshareHodiny/ClockConfig.h"
#include "../WaveshareHodiny/SettingsStore.h"

// Host-only dependencies, shared with the configuration backup harness.
void settingsStoreTestReset();
bool clockTimezoneSupported(const char *name) {
  return !strcmp(name, "Europe/Prague");
}

int main() {
  fakeNvs::values.clear();
  settingsStoreTestReset();
  assert(settingsStoreBegin());

  ClockAppearanceConfig appearance;
  assert(clockAppearanceLoad(appearance));
  assert(!appearance.analogTintedDialEnabled);
  assert(!appearance.analogTintedHandsEnabled);
  for (bool dial : {false, true}) {
    for (bool hands : {false, true}) {
      appearance.analogTintedDialEnabled = dial;
      appearance.analogTintedHandsEnabled = hands;
      assert(clockAppearanceSave(appearance));
      settingsStoreTestReset();
      assert(settingsStoreBegin());
      assert(clockAppearanceLoad(appearance));
      assert(appearance.analogTintedDialEnabled == dial);
      assert(appearance.analogTintedHandsEnabled == hands);
    }
  }
  ClockConfig alarmConfig;
  clockConfigApplyDefaults(alarmConfig);
  alarmConfig.alarms.entries[0] = {425, 31, 1};
  alarmConfig.alarms.entries[11] = {550, 96, 1};
  alarmConfig.alarms.skippedEpoch = 1791781500;
  alarmConfig.alarms.lastMinuteKey = 66200000;
  assert(clockConfigSave(alarmConfig));
  settingsStoreTestReset(); assert(settingsStoreBegin());
  ClockConfig reloaded;
  assert(clockConfigLoad(reloaded));
  assert(!memcmp(&alarmConfig.alarms, &reloaded.alarms, sizeof(AlarmSettings)));
  uint8_t baseline[SETTINGS_IMAGE_CAPACITY], exported[SETTINGS_IMAGE_CAPACITY];
  size_t baselineSize = 0, exportedSize = 0;
  assert(settingsExport(baseline, sizeof(baseline), baselineSize));
  for (unsigned field = 0; field < 3; ++field) {
    const uint32_t value = field == 0 ? 0 : 12345678 + field;
    fakeNvs::writes.clear();
    assert(settingsSaveAlarmValue(field, value));
    assert(fakeNvs::writes.size() == 1 && fakeNvs::writes[0].second == 8);
    fakeNvs::writes.clear(); assert(settingsSaveAlarmValue(field, value));
    assert(fakeNvs::writes.empty());
    settingsStoreTestReset(); assert(settingsStoreBegin()); assert(clockConfigLoad(reloaded));
    assert((field == 0 ? reloaded.alarms.enabled : field == 1 ? reloaded.alarms.skippedEpoch : reloaded.alarms.lastMinuteKey) == value);
    assert(!memcmp(reloaded.alarms.entries, alarmConfig.alarms.entries, sizeof(reloaded.alarms.entries)));
  }
  assert(settingsExport(exported, sizeof(exported), exportedSize));
  fakeNvs::fault = fakeNvs::StyleWrite;
  assert(!settingsSaveAlarmValue(0, 1));
  fakeNvs::fault = fakeNvs::None;
  settingsStoreTestReset(); assert(settingsStoreBegin()); assert(clockConfigLoad(reloaded));
  assert(reloaded.alarms.enabled == 0);
  assert(settingsTransactionBegin()); assert(!settingsSaveAlarmValue(0, 1)); settingsTransactionAbort();
  // Unrelated snapshots incorporate overrides; full config saves supersede them.
  SettingsPreferences extra; assert(extra.begin("web-mode", false)); assert(extra.putUChar("mode", 1));
  settingsStoreTestReset(); assert(settingsStoreBegin()); assert(clockConfigLoad(reloaded)); assert(!reloaded.alarms.enabled);
  const auto alarmOverrideDisk = fakeNvs::values;
  for (auto fault : {fakeNvs::SlotWrite, fakeNvs::SlotRead, fakeNvs::SelectorWrite,
                     fakeNvs::PowerAfterSlot, fakeNvs::PowerAfterSelector}) {
    fakeNvs::values = alarmOverrideDisk; settingsStoreTestReset(); assert(settingsStoreBegin());
    fakeNvs::slotWritten = false; fakeNvs::fault = fault;
    try { assert(!clockConfigSave(alarmConfig)); } catch (const std::runtime_error &) {}
    fakeNvs::fault = fakeNvs::None; settingsStoreTestReset(); assert(settingsStoreBegin()); assert(clockConfigLoad(reloaded));
    assert(reloaded.alarms.enabled == (fault == fakeNvs::PowerAfterSelector ? 1 : 0));
  }
  assert(clockConfigSave(alarmConfig));
  settingsStoreTestReset(); assert(settingsStoreBegin()); assert(clockConfigLoad(reloaded)); assert(reloaded.alarms.enabled == 1);
  assert(settingsImport(exported, exportedSize)); assert(settingsTransactionCommit());
  settingsStoreTestReset(); assert(settingsStoreBegin()); assert(clockConfigLoad(reloaded)); assert(!reloaded.alarms.enabled);
  for (int i = 0; i < 4; ++i) {
    assert(settingsImport(baseline, baselineSize)); assert(settingsTransactionCommit());
    settingsStoreTestReset(); assert(settingsStoreBegin()); assert(clockConfigLoad(reloaded));
    assert(!memcmp(&reloaded.alarms, &alarmConfig.alarms, sizeof(AlarmSettings)));
  }
  fakeNvs::fault = fakeNvs::PowerAfterStyle;
  try { settingsSaveAlarmValue(0, 0); assert(false); } catch (const std::runtime_error &) {}
  fakeNvs::fault = fakeNvs::None;
  settingsStoreTestReset(); assert(settingsStoreBegin()); assert(clockConfigLoad(reloaded)); assert(!reloaded.alarms.enabled);
  assert(clockConfigSave(alarmConfig));
  puts("PASS: alarm controls use one 8-byte write; reboot, failures, power loss, snapshot and export/restore precedence");
  alarmConfig.alarms.entries[0].days = 128;
  assert(!clockConfigValidate(alarmConfig));
  puts("PASS: alarm schedules and skip survive storage reload; invalid masks rejected");
  puts("PASS: independent dial/hand tint defaults and all four saved combinations");

  ClockConfig defaults;
  assert(clockConfigLoad(defaults));
  assert(defaults.automaticFirmwareUpdate);
  assert(defaults.firmwareUpdateMinuteOfDay == 250);
  for (bool enabled : {false, true}) {
    for (const auto schema : {20, 24, 25, 26, 27, 28, 29, 30, 31, 32}) {
      ClockConfig old;
      clockConfigApplyDefaults(old);
      old.schemaVersion = schema;
      old.automaticFirmwareUpdate = enabled;
      if (schema >= 31) old.firmwareUpdateMinuteOfDay = 754;
      const size_t size = schema == 20 ? 2096 : schema <= 26 ? 2108 :
                          schema == 27 ? 2452 : schema == 28 ? 2688 :
                          schema == 29 ? 2752 : schema == 30 ? 2884 : schema == 31 ? 2888 : 2892;
      std::vector<uint8_t> record(size + 12);
      uint32_t magic = 0x57484346, version = schema, checksum = 2166136261u;
      memcpy(record.data(), &magic, 4);
      memcpy(record.data() + 4, &version, 4);
      memcpy(record.data() + 8, &old, size);
      for (size_t i = 8; i < size + 8; ++i) checksum = (checksum ^ record[i]) * 16777619u;
      memcpy(record.data() + size + 8, &checksum, 4);
      ClockConfig migrated;
      assert(clockConfigDecodeRecord(record.data(), record.size(), migrated));
      assert(migrated.automaticFirmwareUpdate == enabled);
      assert(migrated.alarms.enabled == 1);
      for (const auto &alarm : migrated.alarms.entries) assert(!alarm.enabled && !alarm.days);
      assert(migrated.firmwareUpdateMinuteOfDay == (schema >= 31 ? 754 : 250));
      assert(migrated.radarSource == CLOCK_RADAR_SOURCE_MAX_Z);
    }
    for (uint16_t minute : {0, 250, 754, 1439}) {
      defaults.automaticFirmwareUpdate = enabled;
      defaults.firmwareUpdateMinuteOfDay = minute;
      assert(clockConfigSave(defaults));
      settingsStoreTestReset();
      assert(settingsStoreBegin());
      ClockConfig loaded;
      assert(clockConfigLoad(loaded));
      assert(loaded.automaticFirmwareUpdate == enabled);
      assert(loaded.firmwareUpdateMinuteOfDay == minute);
    }
  }
  for (uint8_t source : {CLOCK_RADAR_SOURCE_MAX_Z, CLOCK_RADAR_SOURCE_MAX_Z_MASKED}) {
    defaults.radarSource = source;
    assert(clockConfigSave(defaults));
    settingsStoreTestReset();
    assert(settingsStoreBegin());
    ClockConfig loaded;
    assert(clockConfigLoad(loaded));
    assert(loaded.radarSource == source);
  }
  for (uint8_t country : {CLOCK_LOCATION_COUNTRY_CZECHIA, CLOCK_LOCATION_COUNTRY_OTHER, CLOCK_LOCATION_COUNTRY_SLOVAKIA}) {
    defaults.openMeteoCountry = country;
    defaults.radarSource = CLOCK_RADAR_SOURCE_MAX_Z_MASKED;
    assert(clockConfigSave(defaults));
    settingsStoreTestReset();
    assert(settingsStoreBegin());
    ClockConfig loaded;
    assert(clockConfigLoad(loaded));
    assert(loaded.openMeteoCountry == country);
    assert(clockConfigRadarAvailable(loaded) == (country != CLOCK_LOCATION_COUNTRY_OTHER));
    assert(clockConfigEffectiveRadarSource(loaded) == (country == CLOCK_LOCATION_COUNTRY_SLOVAKIA ? CLOCK_RADAR_SOURCE_SHMU : CLOCK_RADAR_SOURCE_MAX_Z_MASKED));
    assert(loaded.radarSource == CLOCK_RADAR_SOURCE_MAX_Z_MASKED);
  }
  defaults.openMeteoCountry = CLOCK_LOCATION_COUNTRY_CZECHIA;
  defaults.radarSource = 2;
  assert(!clockConfigValidate(defaults));
  defaults.radarSource = CLOCK_RADAR_SOURCE_MAX_Z;
  puts("PASS: radar source migration, persistence and invalid source rejection");
  defaults.firmwareUpdateMinuteOfDay = 1440;
  assert(!clockConfigValidate(defaults));
  puts("PASS: fresh defaults, legacy update preferences and custom time persistence");

  for (uint8_t decimals = 0; decimals <= 2; ++decimals) {
    ClockConfig config;
    clockConfigApplyDefaults(config);
    config.dataSource = CLOCK_DATA_SOURCE_OPEN_METEO;
    for (auto &slot : config.tmepSlots) {
      slot = ClockTmepSlotConfig{};
      slot.decimals = decimals;
    }
    assert(clockConfigSave(config));
    // Drop the in-memory settings cache to exercise persisted readback.
    settingsStoreTestReset();
    assert(settingsStoreBegin());
    ClockConfig loaded;
    assert(clockConfigLoad(loaded));
    for (const auto &slot : loaded.tmepSlots) {
      assert(!slot.enabled);
      assert(slot.decimals == decimals);
    }

    // Mixed Open-Meteo/TMEP configurations must retain both precisions.
    auto &tmep = loaded.tmepSlots[1];
    tmep.enabled = true;
    strcpy(tmep.sensorId, "123");
    strcpy(tmep.field, "temperature");
    strcpy(tmep.unit, "°C");
    tmep.decimals = (decimals + 1) % 3;
    assert(clockConfigSave(loaded));
    settingsStoreTestReset();
    assert(settingsStoreBegin());
    assert(clockConfigLoad(loaded));
    for (size_t i = 0; i < 4; ++i) {
      assert(loaded.tmepSlots[i].enabled == (i == 1));
      assert(loaded.tmepSlots[i].decimals ==
             (i == 1 ? (decimals + 1) % 3 : decimals));
    }
    assert(!strcmp(loaded.tmepSlots[1].sensorId, "123"));
    assert(!strcmp(loaded.tmepSlots[1].field, "temperature"));
    assert(!strcmp(loaded.tmepSlots[1].unit, "°C"));
  }
  puts("PASS: Open-Meteo and mixed TMEP precision 0/1/2 survives save and reload");
}
