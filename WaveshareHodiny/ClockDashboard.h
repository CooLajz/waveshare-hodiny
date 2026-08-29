#pragma once

#include <Arduino.h>

#include "ClockConfig.h"

struct ClockValues {
  int weatherCode = -1;
  bool weatherIsDay = true;
  bool sunStateAvailable = false;
  uint64_t nextSunriseTimestamp = 0;
  uint64_t nextSunsetTimestamp = 0;
  bool dayNightLightStateAvailable = false;
  bool dayNightLightOn = false;
  float leftTemperatureC = NAN;
  float rightTemperatureC = NAN;
  float metricAValue = NAN;
  float metricBValue = NAN;
  bool homeAssistantOnline = false;
};

using BrightnessPreviewCallback = void (*)(uint8_t brightness);
using SettingsOpenCallback = void (*)();
using SettingsSaveCallback = void (*)(uint8_t dayBrightness,
                                      uint8_t nightBrightness,
                                      bool automaticDayNight,
                                      bool secondRingEnabled,
                                      uint8_t secondEffect,
                                      bool animatedWeatherIcons,
                                      uint8_t weatherIconStyle,
                                      bool automaticFirmwareUpdate,
                                      uint8_t webMode);
using SettingsActionCallback = void (*)();
using RadarVisibilityCallback = void (*)(bool visible);
using RadarRangeCallback = void (*)(int8_t direction);

void clockDashboardInit(const ClockValues &values, uint8_t dayBrightness,
                        uint8_t nightBrightness, bool automaticDayNight,
                        BrightnessPreviewCallback brightnessPreview,
                        SettingsOpenCallback settingsOpen,
                        SettingsSaveCallback settingsSave,
                        SettingsActionCallback firmwareCheck,
                        SettingsActionCallback firmwareInstall,
                        RadarVisibilityCallback radarVisibility,
                        RadarRangeCallback radarRange);
void clockDashboardLoop();
void clockDashboardShowSettings();
void clockDashboardShowSettingsPage(uint8_t page);
void clockDashboardSetNightMode(bool enabled);
bool clockDashboardNightModeEnabled();
void clockDashboardHandleShortClick();
bool clockDashboardRadarVisible();
void clockDashboardSetRadarVisible(bool visible);
bool clockDashboardAutomaticRotationAllowed();
void clockDashboardSetWifiAddress(const char *ipAddress);
void clockDashboardSetFirmwareVersion(const char *version,
                                      bool updateAvailable);
void clockDashboardSetFirmwareUpdateActive(bool active);
void clockDashboardSetFirmwareUpdateBlack(bool black);
void clockDashboardSetFirmwareUpdateCountdown(uint8_t seconds);
void clockDashboardSetWebActive(bool active);
void clockDashboardSetWifiConnected(bool connected);
void clockDashboardSetWebMode(uint8_t mode);
void clockDashboardApplyConfiguration(const ClockConfig &config);
void clockDashboardUpdate(const ClockValues &values);
void clockDashboardSetDate(const char *dateText);
void clockDashboardSetSecond(uint8_t second);
void clockDashboardSetTime(const char *timeText);
void clockDashboardSetWeatherAnimation(const uint8_t *gifData, size_t size,
                                       const char *iconKey);
void clockDashboardSetRadarSnapshot(const uint16_t *pixels,
                                    const char *frameTime, uint16_t radiusKm,
                                    const char *message, bool loading,
                                    bool latestFrame,
                                    uint8_t currentFrameNumber,
                                    uint8_t animationFrameCount,
                                    uint8_t pauseSeconds);
