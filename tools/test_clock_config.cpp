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
  puts("PASS: independent dial/hand tint defaults and all four saved combinations");

  ClockConfig defaults;
  assert(clockConfigLoad(defaults));
  assert(defaults.automaticFirmwareUpdate);
  assert(defaults.firmwareUpdateMinuteOfDay == 250);
  for (bool enabled : {false, true}) {
    for (const auto schema : {20, 24, 25, 26, 27, 28, 29, 30}) {
      ClockConfig old;
      clockConfigApplyDefaults(old);
      old.schemaVersion = schema;
      old.automaticFirmwareUpdate = enabled;
      const size_t size = schema == 20 ? 2096 : schema <= 26 ? 2108 :
                          schema == 27 ? 2452 : schema == 28 ? 2688 :
                          schema == 29 ? 2752 : 2884;
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
      assert(migrated.firmwareUpdateMinuteOfDay == 250);
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
