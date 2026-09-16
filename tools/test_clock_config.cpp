#include <cassert>
#include <cstdio>
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
