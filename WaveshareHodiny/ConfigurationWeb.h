#pragma once

#include "ClockConfig.h"

using ClockConfigLoadCallback = void (*)(ClockConfig &config);
using ClockConfigSaveCallback = bool (*)(const ClockConfig &config,
                                         bool tokenWasSubmitted);
using ConfigurationWebStatusCallback = void (*)(bool active);
using SunTransitionTimesCallback = void (*)(uint64_t &nextSunriseTimestamp,
                                            uint64_t &nextSunsetTimestamp);
using HomeAssistantRefreshCallback = bool (*)();
using DisplayPowerCallback = void (*)(bool forcedOff);
using DisplayPowerStatusCallback = bool (*)();
using RadarRangeStateCallback = void (*)(uint16_t &savedRadiusKm,
                                         uint16_t &activeRadiusKm);
using RadarRangePreviewCallback = bool (*)(uint16_t radiusKm);
using ClockAppearanceStateCallback = void (*)(
    ClockAppearanceConfig &savedAppearance,
    ClockAppearanceConfig &activeAppearance);
using ClockAppearanceChangeCallback = bool (*)(
    const ClockAppearanceConfig &appearance);
using DayNightStatusCallback = void (*)(bool &sunAvailable, bool &sunIsDay,
                                        bool &lightAvailable, bool &lightOn,
                                        bool &nightMode);

enum ConfigurationWebMode : uint8_t {
  CONFIGURATION_WEB_TIMED = 0,
  CONFIGURATION_WEB_ALWAYS = 1,
  CONFIGURATION_WEB_DISABLED = 2,
};

void configurationWebBegin(ClockConfigLoadCallback loadCallback,
                           ClockConfigSaveCallback saveCallback,
                           ConfigurationWebStatusCallback statusCallback,
                           SunTransitionTimesCallback sunTimesCallback,
                           HomeAssistantRefreshCallback refreshCallback,
                           DayNightStatusCallback dayNightStatusCallback,
                           DisplayPowerCallback displayPowerCallback,
                           DisplayPowerStatusCallback displayPowerStatusCallback,
                           RadarRangeStateCallback radarRangeStateCallback,
                           RadarRangePreviewCallback radarRangePreviewCallback,
                           ClockAppearanceStateCallback appearanceStateCallback,
                           ClockAppearanceChangeCallback appearancePreviewCallback,
                           ClockAppearanceChangeCallback appearanceSaveCallback);
void configurationWebLoop();
void configurationWebEnsureActive();
void configurationWebExtendAvailability();
ConfigurationWebMode configurationWebMode();
bool configurationWebSetMode(ConfigurationWebMode mode);
void configurationWebLockForTest();
void configurationWebUnlockForTest();
