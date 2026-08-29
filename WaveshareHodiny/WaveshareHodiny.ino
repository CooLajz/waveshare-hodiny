#include <Arduino.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_sntp.h>
#include <esp_task_wdt.h>
#include <time.h>

#include "ClockDashboard.h"
#include "ClockConfig.h"
#include "ChmiRadarService.h"
#include "ConfigurationWeb.h"
#include "DayNightLogic.h"
#include "DisplayDriver.h"
#include "Display_ST7701.h"
#include "FirmwareBuild.h"
#include "FirmwareHubCa.h"
#include "FirmwareUpdateService.h"
#include "I2C_Driver.h"
#include "ImprovSerialService.h"
#include "NetworkDiagnostics.h"
#include "NetworkCoordinator.h"
#include "TCA9554PWR.h"
#include "WifiProvisioning.h"
#include "WeatherAnimationService.h"

// ClockConfig is intentionally copied under a mutex so the web server and
// background data task always see a consistent snapshot. Keep enough room for
// that snapshot and the display/network calls made from Arduino's loop task.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

#if !FIRMWARE_RELEASE && __has_include("local/secrets.h")
#include "local/secrets.h"
#define HAS_WIFI_SECRETS 1
#else
#define HAS_WIFI_SECRETS 0
#endif

#if HAS_WIFI_SECRETS && defined(HOME_ASSISTANT_URL) && \
    defined(HOME_ASSISTANT_TOKEN) && defined(HA_ENTITY_WEATHER_CODE) && \
    defined(HA_ENTITY_OUTSIDE_TEMPERATURE) && \
    defined(HA_ENTITY_ROOM_TEMPERATURE) && defined(HA_ENTITY_ROOM_CO2) && \
    defined(HA_ENTITY_ROOM_HUMIDITY) && defined(HA_ENTITY_SUN)
#define HAS_HOME_ASSISTANT_SECRETS 1
#else
#define HAS_HOME_ASSISTANT_SECRETS 0
#endif

namespace {
ClockValues sampleValues;
ClockConfig runtimeConfig;
ClockConfig persistedConfig;
ClockConfig configSaveBuffer;
ClockConfig dashboardConfigBuffer;
SemaphoreHandle_t runtimeConfigMutex = nullptr;
TaskHandle_t homeAssistantTaskHandle = nullptr;
String usbCommand;
bool screenshotTransferActive = false;
unsigned long displayResyncAt = 0;
int lastDisplayedSecond = -1;
bool wifiWasConnected = false;
bool timeWasSynchronized = false;
ClockValues pendingHomeAssistantValues;
portMUX_TYPE homeAssistantValuesMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool homeAssistantUpdatePending = false;
volatile bool dayNightLightRefreshRequested = false;
portMUX_TYPE dayNightLightRefreshMux = portMUX_INITIALIZER_UNLOCKED;
bool mdnsStarted = false;
String displayedWifiIp;
bool runtimeConfigurationApplyPending = false;
unsigned long runtimeConfigurationApplyAt = 0;
int lastAutomaticFirmwareCheckDate = -1;
unsigned long lastFirmwareDisplayRefreshAt = 0;
char displayedFirmwareVersion[24] = "";
bool displayedFirmwareUpdateAvailable = false;
bool firmwareDisplayInitialized = false;
#if !FIRMWARE_RELEASE
bool forceNightTestActive = false;
#endif
uint8_t currentDisplayBrightness = 35;
bool displayForcedOff = false;
volatile bool firmwareUpdateDisplayRequested = false;
volatile bool firmwareUpdateDisplayPresented = false;
volatile bool firmwareUpdateBlackRequested = false;
volatile bool firmwareUpdateBlackPresented = false;
volatile bool firmwareUpdateCountdownStarted = false;
volatile unsigned long firmwareUpdateCountdownStartedAt = 0;
uint8_t firmwareUpdateCountdownDisplayed = 0;
bool firmwareUpdateDisplayActive = false;
uint32_t displayedRadarGeneration = UINT32_MAX;
char displayedRadarTime[6] = "";
uint16_t displayedRadarRadiusKm = 50;
bool radarRadiusApplyPending = false;
unsigned long radarRadiusApplyAt = 0;
bool automaticRadarRotationPaused = true;
unsigned long displayModeStartedAt = 0;
bool radarRotationWaitingForCycle = false;
uint32_t radarRotationCycleAtTimeout = 0;
bool radarRedNightModeApplied = false;

constexpr uint32_t LOOP_WATCHDOG_TIMEOUT_MS = 20UL * 1000UL;
constexpr uint32_t NTP_SYNC_INTERVAL_MS = 60UL * 60UL * 1000UL;
constexpr uint32_t HOME_ASSISTANT_REFRESH_MS = 60UL * 1000UL;
constexpr uint32_t HOME_ASSISTANT_RETRY_MS = 5UL * 1000UL;
constexpr uint32_t HOME_ASSISTANT_CONNECT_TIMEOUT_MS = 5000;
constexpr uint32_t HOME_ASSISTANT_RESPONSE_TIMEOUT_MS = 8000;
constexpr uint8_t HOME_ASSISTANT_REQUEST_ATTEMPTS = 2;
constexpr uint32_t HOME_ASSISTANT_REQUEST_RETRY_DELAY_MS = 250;
constexpr uint32_t OPEN_METEO_REFRESH_MS = 10UL * 60UL * 1000UL;
constexpr time_t VALID_TIME_THRESHOLD = 1700000000;

const char *CZECH_WEEKDAYS[] = {
    "NEDĚLE", "PONDĚLÍ", "ÚTERÝ", "STŘEDA",
    "ČTVRTEK", "PÁTEK", "SOBOTA",
};
const char *CZECH_MONTHS[] = {
    "LEDNA", "ÚNORA", "BŘEZNA", "DUBNA", "KVĚTNA", "ČERVNA",
    "ČERVENCE", "SRPNA", "ZÁŘÍ", "ŘÍJNA", "LISTOPADU", "PROSINCE",
};

void applyDevelopmentDefaults(ClockConfig &config);

ClockConfig runtimeConfigSnapshot() {
  ClockConfig config;
  if (runtimeConfigMutex == nullptr) return runtimeConfig;
  xSemaphoreTake(runtimeConfigMutex, portMAX_DELAY);
  config = runtimeConfig;
  xSemaphoreGive(runtimeConfigMutex);
  return config;
}

void loadRuntimeConfigForWeb(ClockConfig &config) {
  if (runtimeConfigMutex == nullptr) {
    config = runtimeConfig;
    return;
  }
  xSemaphoreTake(runtimeConfigMutex, portMAX_DELAY);
  config = runtimeConfig;
  xSemaphoreGive(runtimeConfigMutex);
}

bool saveRuntimeConfig(const ClockConfig &config, bool tokenWasSubmitted) {
  configSaveBuffer = config;
  if (!tokenWasSubmitted) {
    clockConfigCopy(configSaveBuffer.homeAssistantToken,
                    sizeof(configSaveBuffer.homeAssistantToken),
                    persistedConfig.homeAssistantToken);
  }
  if (!clockConfigSave(configSaveBuffer)) return false;
  radarRadiusApplyPending = false;
  xSemaphoreTake(runtimeConfigMutex, portMAX_DELAY);
  persistedConfig = configSaveBuffer;
  runtimeConfig = configSaveBuffer;
  applyDevelopmentDefaults(runtimeConfig);
  xSemaphoreGive(runtimeConfigMutex);
  runtimeConfigurationApplyPending = true;
  runtimeConfigurationApplyAt = millis() + 250;
  if (homeAssistantTaskHandle != nullptr) xTaskNotifyGive(homeAssistantTaskHandle);
  return true;
}

void applyPendingRuntimeConfiguration() {
  if (!runtimeConfigurationApplyPending ||
      static_cast<long>(millis() - runtimeConfigurationApplyAt) < 0) {
    return;
  }
  runtimeConfigurationApplyPending = false;
  runtimeConfigurationApplyAt = 0;
  automaticRadarRotationPaused = true;
  radarRotationWaitingForCycle = false;
  xSemaphoreTake(runtimeConfigMutex, portMAX_DELAY);
  dashboardConfigBuffer = runtimeConfig;
  xSemaphoreGive(runtimeConfigMutex);
  clockDashboardApplyConfiguration(dashboardConfigBuffer);
  if (clockDashboardRadarVisible()) {
    chmiRadarServiceSetActive(true, dashboardConfigBuffer.openMeteoLatitude,
                              dashboardConfigBuffer.openMeteoLongitude,
                              dashboardConfigBuffer.radarRadiusKm,
                              dashboardConfigBuffer.radarFrameCount,
                              dashboardConfigBuffer.radarMapOpacity,
                              dashboardConfigBuffer.radarPauseSeconds);
  }
  // Zápis do flash může na ESP32-S3 rozhodit vertikální synchronizaci RGB
  // panelu. Provádíme ji až po dokončení obsluhy HTTP požadavku.
  LCD_Resync();
  displayResyncAt = millis() + 750;
}

void applyDevelopmentDefaults(ClockConfig &config) {
#if HAS_HOME_ASSISTANT_SECRETS && defined(WAVESHARE_DEVELOPMENT_BUILD)
  if (config.homeAssistantUrl[0] == '\0') {
    clockConfigCopy(config.homeAssistantUrl, sizeof(config.homeAssistantUrl),
                    HOME_ASSISTANT_URL);
  }
  if (config.homeAssistantToken[0] == '\0') {
    clockConfigCopy(config.homeAssistantToken,
                    sizeof(config.homeAssistantToken), HOME_ASSISTANT_TOKEN);
  }
  if (config.weatherEntityId[0] == '\0') {
    clockConfigCopy(config.weatherEntityId, sizeof(config.weatherEntityId),
                    HA_ENTITY_WEATHER_CODE);
  }
  if (config.leftSide.temperatureEntityId[0] == '\0') {
    clockConfigCopy(config.leftSide.temperatureEntityId,
                    sizeof(config.leftSide.temperatureEntityId),
                    HA_ENTITY_OUTSIDE_TEMPERATURE);
  }
  if (config.rightSide.temperatureEntityId[0] == '\0') {
    clockConfigCopy(config.rightSide.temperatureEntityId,
                    sizeof(config.rightSide.temperatureEntityId),
                    HA_ENTITY_ROOM_TEMPERATURE);
  }
  if (config.metricA.entityId[0] == '\0') {
    clockConfigCopy(config.metricA.entityId, sizeof(config.metricA.entityId),
                    HA_ENTITY_ROOM_CO2);
  }
  if (config.metricB.entityId[0] == '\0') {
    clockConfigCopy(config.metricB.entityId, sizeof(config.metricB.entityId),
                    HA_ENTITY_ROOM_HUMIDITY);
  }
  if (config.sunEntityId[0] == '\0') {
    clockConfigCopy(config.sunEntityId, sizeof(config.sunEntityId), HA_ENTITY_SUN);
  }
#endif
}

void handleBrightnessPreview(uint8_t brightness) {
  currentDisplayBrightness = constrain(brightness, 1, 100);
  Set_Backlight(displayForcedOff ? 0 : currentDisplayBrightness);
}

void handleDisplayPower(bool forcedOff) {
  if (displayForcedOff == forcedOff) return;
  displayForcedOff = forcedOff;
  if (displayForcedOff) {
    Set_Backlight(0);
    LCD_Sleep();
    return;
  }
  LCD_Wake();
  displayDriverRefresh();
  Set_Backlight(currentDisplayBrightness);
}

bool displayPowerForcedOff() {
  return displayForcedOff;
}

void handleSettingsOpen() {
  configurationWebEnsureActive();
  clockDashboardSetWebMode(configurationWebMode());
}

void handleRadarVisibility(bool visible) {
  displayModeStartedAt = millis();
  automaticRadarRotationPaused = false;
  radarRotationWaitingForCycle = false;
  const ClockConfig config = runtimeConfigSnapshot();
  chmiRadarServiceSetActive(visible, config.openMeteoLatitude,
                            config.openMeteoLongitude, config.radarRadiusKm,
                            config.radarFrameCount, config.radarMapOpacity,
                            config.radarPauseSeconds);
}

void handleRadarRangeChange(int8_t direction) {
  static constexpr uint16_t RADAR_RADII[] = {25, 50, 100, 200, 0};
  ClockConfig config = runtimeConfigSnapshot();
  size_t index = 1;
  for (size_t candidate = 0; candidate < 5; ++candidate) {
    if (RADAR_RADII[candidate] == config.radarRadiusKm) {
      index = candidate;
      break;
    }
  }
  if (direction > 0 && index + 1 < 5)
    ++index;
  else if (direction < 0 && index > 0)
    --index;
  else
    return;
  xSemaphoreTake(runtimeConfigMutex, portMAX_DELAY);
  runtimeConfig.radarRadiusKm = RADAR_RADII[index];
  xSemaphoreGive(runtimeConfigMutex);
  radarRadiusApplyPending = true;
  radarRadiusApplyAt = millis() + 350;
  displayModeStartedAt = millis();
  radarRotationWaitingForCycle = false;
}

void maintainRadarRangeChange() {
  if (radarRadiusApplyPending &&
      static_cast<long>(millis() - radarRadiusApplyAt) >= 0) {
    radarRadiusApplyPending = false;
    radarRadiusApplyAt = 0;
    if (clockDashboardRadarVisible()) {
      const ClockConfig config = runtimeConfigSnapshot();
      chmiRadarServiceSetActive(true, config.openMeteoLatitude,
                                config.openMeteoLongitude,
                                config.radarRadiusKm,
                                config.radarFrameCount,
                                config.radarMapOpacity,
                                config.radarPauseSeconds);
    }
  }
}

void loadRadarRangeStateForWeb(uint16_t &savedRadiusKm,
                               uint16_t &activeRadiusKm) {
  savedRadiusKm = persistedConfig.radarRadiusKm;
  activeRadiusKm = runtimeConfigSnapshot().radarRadiusKm;
}

bool previewRadarRangeFromWeb(uint16_t radiusKm) {
  xSemaphoreTake(runtimeConfigMutex, portMAX_DELAY);
  runtimeConfig.radarRadiusKm = radiusKm;
  const ClockConfig config = runtimeConfig;
  xSemaphoreGive(runtimeConfigMutex);
  radarRadiusApplyPending = false;
  radarRadiusApplyAt = 0;
  displayModeStartedAt = millis();
  radarRotationWaitingForCycle = false;
  if (clockDashboardRadarVisible()) {
    chmiRadarServiceSetActive(true, config.openMeteoLatitude,
                              config.openMeteoLongitude,
                              config.radarRadiusKm,
                              config.radarFrameCount,
                              config.radarMapOpacity,
                              config.radarPauseSeconds);
  }
  return true;
}

void maintainAutomaticRadarRotation() {
  const ClockConfig config = runtimeConfigSnapshot();
  const bool allowed =
      config.automaticRadarRotation && !displayForcedOff &&
      clockDashboardAutomaticRotationAllowed();
  if (!allowed) {
    automaticRadarRotationPaused = true;
    radarRotationWaitingForCycle = false;
    return;
  }
  const unsigned long now = millis();
  if (automaticRadarRotationPaused) {
    automaticRadarRotationPaused = false;
    displayModeStartedAt = now;
    radarRotationWaitingForCycle = false;
    return;
  }
  const bool radarVisible = clockDashboardRadarVisible();
  const unsigned long durationMs =
      static_cast<unsigned long>(radarVisible ? config.radarDisplaySeconds
                                              : config.clockDisplaySeconds) *
      1000UL;
  if (now - displayModeStartedAt < durationMs) return;
  if (radarVisible) {
    ChmiRadarSnapshot snapshot;
    chmiRadarServiceSnapshot(snapshot);
    const bool staticRadarReady =
        snapshot.ready && snapshot.animationFrameCount <= 1;
    if (!staticRadarReady) {
      if (!radarRotationWaitingForCycle) {
        // Nastavený čas je pouze minimum. Od této chvíle čekáme na dokončení
        // právě rozběhnutého cyklu včetně koncové pauzy.
        radarRotationWaitingForCycle = true;
        radarRotationCycleAtTimeout = snapshot.completedAnimationCycles;
        return;
      }
      if (snapshot.completedAnimationCycles == radarRotationCycleAtTimeout)
        return;
    }
  }
  clockDashboardSetRadarVisible(!radarVisible);
}

void maintainDisplayGestures() {
  if (displayDriverTakeHorizontalSwipe() &&
      clockDashboardAutomaticRotationAllowed()) {
    clockDashboardSetRadarVisible(!clockDashboardRadarVisible());
  }
  const int8_t verticalSwipeDirection = displayDriverTakeVerticalSwipe();
  if (verticalSwipeDirection != 0 && clockDashboardRadarVisible() &&
      clockDashboardAutomaticRotationAllowed()) {
    handleRadarRangeChange(verticalSwipeDirection);
  }
  if (displayDriverTakeSingleClick()) clockDashboardHandleShortClick();
}

void maintainRadarDisplay() {
  ChmiRadarSnapshot snapshot;
  chmiRadarServiceSnapshot(snapshot);
  if (snapshot.generation == displayedRadarGeneration &&
      strcmp(snapshot.frameTime, displayedRadarTime) == 0)
    return;
  displayedRadarGeneration = snapshot.generation;
  displayedRadarRadiusKm = snapshot.radiusKm;
  strlcpy(displayedRadarTime, snapshot.frameTime,
          sizeof(displayedRadarTime));
  clockDashboardSetRadarSnapshot(snapshot.pixels, snapshot.frameTime,
                                 displayedRadarRadiusKm,
                                 snapshot.message, snapshot.loading,
                                 snapshot.latestFrame,
                                 snapshot.currentFrameNumber,
                                 snapshot.animationFrameCount,
                                 snapshot.pauseSeconds);
}

void maintainRadarNightVisual() {
  const ClockConfig config = runtimeConfigSnapshot();
  const bool enabled =
      clockDashboardNightModeEnabled() &&
      config.nightVisualMode == CLOCK_NIGHT_VISUAL_RED;
  if (radarRedNightModeApplied == enabled) return;
  radarRedNightModeApplied = enabled;
  chmiRadarServiceSetRedNightMode(enabled);
}

void handleConfigurationWebStatus(bool active) {
  clockDashboardSetWebActive(active);
}

void loadSunTransitionTimesForWeb(uint64_t &nextSunriseTimestamp,
                                  uint64_t &nextSunsetTimestamp) {
  nextSunriseTimestamp = sampleValues.nextSunriseTimestamp;
  nextSunsetTimestamp = sampleValues.nextSunsetTimestamp;
}

bool requestHomeAssistantRefreshFromWeb() {
  if (homeAssistantTaskHandle == nullptr) return false;
  portENTER_CRITICAL(&dayNightLightRefreshMux);
  dayNightLightRefreshRequested = true;
  portEXIT_CRITICAL(&dayNightLightRefreshMux);
  xTaskNotifyGive(homeAssistantTaskHandle);
  return true;
}

bool consumeDayNightLightRefreshRequest() {
  portENTER_CRITICAL(&dayNightLightRefreshMux);
  const bool requested = dayNightLightRefreshRequested;
  dayNightLightRefreshRequested = false;
  portEXIT_CRITICAL(&dayNightLightRefreshMux);
  return requested;
}

void loadDayNightStatusForWeb(bool &sunAvailable, bool &sunIsDay,
                              bool &lightAvailable, bool &lightOn,
                              bool &nightMode) {
  sunAvailable = sampleValues.sunStateAvailable;
  sunIsDay = sampleValues.weatherIsDay;
  lightAvailable = sampleValues.dayNightLightStateAvailable;
  lightOn = sampleValues.dayNightLightOn;
  nightMode = clockDashboardNightModeEnabled();
}

void handleSettingsSave(uint8_t dayBrightness, uint8_t nightBrightness,
                        bool automaticDayNight, bool secondRingEnabled,
                        uint8_t selectedSecondEffect,
                        bool animatedWeatherIcons, uint8_t weatherIconStyle,
                        bool automaticFirmwareUpdate, uint8_t webMode) {
  ClockConfig config = runtimeConfigSnapshot();
  config.dayBrightness = constrain(dayBrightness, 1, 100);
  config.nightBrightness = constrain(nightBrightness, 1, 100);
  config.automaticDayNight = automaticDayNight;
  config.secondRingEnabled = secondRingEnabled;
  config.secondEffect = selectedSecondEffect;
  config.animatedWeatherIcons = animatedWeatherIcons;
  config.weatherIconStyle = weatherIconStyle;
  config.automaticFirmwareUpdate = automaticFirmwareUpdate;
  if (saveRuntimeConfig(config, false)) {
    configurationWebSetMode(static_cast<ConfigurationWebMode>(webMode));
    clockDashboardSetWebMode(configurationWebMode());
  }
}

void handleSettingsFirmwareCheck() {
  firmwareUpdateServiceRequestCheck(false);
}

void handleSettingsFirmwareInstall() {
  firmwareUpdateServiceRequestCheck(true);
}

void handleUsbCommands() {
#if !FIRMWARE_RELEASE
  while (Serial.available() > 0) {
    const char character = static_cast<char>(Serial.read());
    if (character == '\n' || character == '\r') {
      usbCommand.trim();
      if (usbCommand == "SCREENSHOT" && !screenshotTransferActive) {
        Serial.println("WSFB1_BEGIN");
        if (!displayDriverBeginFramebufferCapture(Serial)) {
          Serial.println("WSFB1_ERROR");
        } else {
          screenshotTransferActive = true;
        }
      } else if (usbCommand == "SETTINGS" && !screenshotTransferActive) {
        clockDashboardShowSettings();
        Serial.println("SETTINGS_OPEN");
      } else if (usbCommand == "SETTINGS2" && !screenshotTransferActive) {
        clockDashboardShowSettingsPage(1);
        Serial.println("SETTINGS_OPEN");
      } else if (usbCommand == "SETTINGS3" && !screenshotTransferActive) {
        clockDashboardShowSettingsPage(2);
        Serial.println("SETTINGS_OPEN");
      } else if (usbCommand == "NIGHT" && !screenshotTransferActive) {
        clockDashboardSetNightMode(true);
        Serial.println("NIGHT_OPEN");
      } else if (usbCommand == "NIGHTTEST" && !screenshotTransferActive) {
        forceNightTestActive = true;
        sampleValues.weatherIsDay = false;
        sampleValues.sunStateAvailable = true;
        clockDashboardUpdate(sampleValues);
        Serial.println("NIGHT_TEST_ON");
      } else if (usbCommand == "NIGHTTESTOFF" && !screenshotTransferActive) {
        forceNightTestActive = false;
        if (homeAssistantTaskHandle != nullptr)
          xTaskNotifyGive(homeAssistantTaskHandle);
        Serial.println("NIGHT_TEST_OFF");
      } else if (usbCommand == "WEBLOCK" && !screenshotTransferActive) {
        configurationWebLockForTest();
        Serial.println("WEB_CONFIG_LOCKED");
      } else if (usbCommand == "WEBUNLOCK" && !screenshotTransferActive) {
        configurationWebUnlockForTest();
        Serial.println("WEB_CONFIG_UNLOCKED");
      }
      usbCommand = "";
    } else if (usbCommand.length() < 32) {
      usbCommand += character;
    } else {
      usbCommand = "";
    }
  }
#endif
}

void streamScreenshot() {
#if !FIRMWARE_RELEASE
  if (!screenshotTransferActive) return;
  if (!displayDriverStreamFramebufferChunk(Serial)) return;

  Serial.println();
  Serial.println("WSFB1_END");
  screenshotTransferActive = false;
  LCD_Resync();
#endif
}

void maintainDisplaySync() {
  if (displayResyncAt == 0 ||
      static_cast<long>(millis() - displayResyncAt) < 0) {
    return;
  }
  LCD_Resync();
  displayResyncAt = 0;
}

void initializeNetworkTime() {
  wifiProvisioningBegin();
  configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3", "pool.ntp.org",
               "time.cloudflare.com");
  sntp_set_sync_interval(NTP_SYNC_INTERVAL_MS);
#if !FIRMWARE_RELEASE
#if HAS_WIFI_SECRETS
  Serial.println("Wi-Fi a NTP inicializovany");
#else
  Serial.println("Wi-Fi konfigurace chybi; cas zustava demonstracni");
#endif
#endif
}

void maintainNetworkTime() {
  const bool wifiConnected = WiFi.status() == WL_CONNECTED;
  if (!wifiConnected) {
    if (!displayedWifiIp.isEmpty()) {
      displayedWifiIp = "";
      clockDashboardSetWifiAddress("");
    }
    if (wifiWasConnected) clockDashboardSetWifiConnected(false);
    wifiWasConnected = false;
    return;
  }

  const String wifiIp = WiFi.localIP().toString();
  if (wifiIp != displayedWifiIp) {
    displayedWifiIp = wifiIp;
    clockDashboardSetWifiAddress(displayedWifiIp.c_str());
  }

  if (!wifiWasConnected) {
    wifiWasConnected = true;
    clockDashboardSetWifiConnected(true);
    displayResyncAt = millis() + 2000;
#if !FIRMWARE_RELEASE
    Serial.println("Wi-Fi pripojena, cekam na NTP");
#endif
    if (!mdnsStarted) {
      mdnsStarted = MDNS.begin("waveshare-hodiny");
      if (mdnsStarted) {
        MDNS.addService("http", "tcp", 80);
#if !FIRMWARE_RELEASE
        Serial.println("Nastaveni: http://waveshare-hodiny.local/");
#endif
      }
    }
  }

  time_t now;
  time(&now);
  if (now < VALID_TIME_THRESHOLD) return;
  if (!timeWasSynchronized) {
    timeWasSynchronized = true;
    if (homeAssistantTaskHandle != nullptr) {
      xTaskNotifyGive(homeAssistantTaskHandle);
    }
#if !FIRMWARE_RELEASE
    Serial.println("NTP synchronizovano");
#endif
  }

  struct tm localTime;
  localtime_r(&now, &localTime);
  if (localTime.tm_sec == lastDisplayedSecond) return;
  lastDisplayedSecond = localTime.tm_sec;

  const ClockConfig config = runtimeConfigSnapshot();
  char timeText[6];
  snprintf(timeText, sizeof(timeText), config.showLeadingHourZero ? "%02d:%02d"
                                                                  : "%d:%02d",
           localTime.tm_hour, localTime.tm_min);
  clockDashboardSetTime(timeText);
  clockDashboardSetSecond(static_cast<uint8_t>(localTime.tm_sec));

  char dateText[64];
  switch (config.dateFormat) {
    case CLOCK_DATE_FORMAT_HIDDEN:
      dateText[0] = '\0';
      break;
    case CLOCK_DATE_FORMAT_NUMERIC:
      snprintf(dateText, sizeof(dateText), "%02d.%02d.%04d", localTime.tm_mday,
               localTime.tm_mon + 1, localTime.tm_year + 1900);
      break;
    case CLOCK_DATE_FORMAT_DAY_MONTH_YEAR:
      snprintf(dateText, sizeof(dateText), "%d. %s %d", localTime.tm_mday,
               CZECH_MONTHS[localTime.tm_mon], localTime.tm_year + 1900);
      break;
    case CLOCK_DATE_FORMAT_WEEKDAY_DAY_MONTH_YEAR:
      snprintf(dateText, sizeof(dateText), "%s, %d. %s %d",
               CZECH_WEEKDAYS[localTime.tm_wday], localTime.tm_mday,
               CZECH_MONTHS[localTime.tm_mon], localTime.tm_year + 1900);
      break;
    case CLOCK_DATE_FORMAT_WEEKDAY_DAY_MONTH:
    default:
      snprintf(dateText, sizeof(dateText), "%s, %d. %s",
               CZECH_WEEKDAYS[localTime.tm_wday], localTime.tm_mday,
               CZECH_MONTHS[localTime.tm_mon]);
      break;
  }
  clockDashboardSetDate(dateText);
}

void handleFirmwareUpdateLifecycle(bool updating) {
  if (homeAssistantTaskHandle != nullptr) {
    if (updating) {
      vTaskSuspend(homeAssistantTaskHandle);
    } else {
      vTaskResume(homeAssistantTaskHandle);
    }
  }
  firmwareUpdateDisplayRequested = updating;
  if (updating) {
    displayResyncAt = 0;
    firmwareUpdateDisplayPresented = false;
    const unsigned long deadline = millis() + 1000;
    while (!firmwareUpdateDisplayPresented &&
           static_cast<long>(deadline - millis()) > 0) {
      delay(5);
    }
    firmwareUpdateBlackPresented = false;
    firmwareUpdateCountdownStartedAt = millis();
    firmwareUpdateCountdownStarted = true;
    const unsigned long blackDeadline = millis() + 7000;
    while (!firmwareUpdateBlackPresented &&
           static_cast<long>(blackDeadline - millis()) > 0) {
      delay(5);
    }
    // Necháme několik obnovovacích cyklů naplnit framebuffer i RGB bounce
    // buffery čistou černou ještě před prvním zápisem OTA do flash.
    delay(500);
  } else {
    firmwareUpdateCountdownStarted = false;
    firmwareUpdateBlackRequested = false;
    displayResyncAt = millis() + 500;
  }
}

void applyFirmwareUpdateDisplayRequest() {
  const bool requested = firmwareUpdateDisplayRequested;
  if (firmwareUpdateDisplayActive == requested) return;
  firmwareUpdateDisplayActive = requested;
  clockDashboardSetFirmwareUpdateActive(requested);
  if (!requested) clockDashboardSetFirmwareUpdateBlack(false);
}

void maintainAutomaticFirmwareUpdate() {
  if (!IS_RELEASE_FIRMWARE || !timeWasSynchronized ||
      WiFi.status() != WL_CONNECTED) {
    return;
  }
  const ClockConfig config = runtimeConfigSnapshot();
  if (!config.automaticFirmwareUpdate) return;
  time_t now;
  time(&now);
  struct tm localTime;
  localtime_r(&now, &localTime);
  if (localTime.tm_hour < 4 ||
      (localTime.tm_hour == 4 && localTime.tm_min < 10)) {
    return;
  }
  const int dateKey = (localTime.tm_year + 1900) * 1000 + localTime.tm_yday;
  if (dateKey == lastAutomaticFirmwareCheckDate) return;
  if (firmwareUpdateServiceRequestCheck(true)) {
    lastAutomaticFirmwareCheckDate = dateKey;
  }
}

void maintainFirmwareDisplayStatus() {
  const unsigned long now = millis();
  if (firmwareDisplayInitialized &&
      now - lastFirmwareDisplayRefreshAt < 250) {
    return;
  }
  lastFirmwareDisplayRefreshAt = now;
  const FirmwareUpdateSnapshot snapshot = firmwareUpdateServiceSnapshot();
  if (firmwareDisplayInitialized &&
      strcmp(displayedFirmwareVersion, snapshot.currentVersion) == 0 &&
      displayedFirmwareUpdateAvailable == snapshot.updateAvailable) {
    return;
  }
  strlcpy(displayedFirmwareVersion, snapshot.currentVersion,
          sizeof(displayedFirmwareVersion));
  displayedFirmwareUpdateAvailable = snapshot.updateAvailable;
  firmwareDisplayInitialized = true;
  clockDashboardSetFirmwareVersion(displayedFirmwareVersion,
                                   displayedFirmwareUpdateAvailable);
}

bool extractJsonStringField(const String &payload, const char *key,
                            String &value) {
  const String quotedKey = String('"') + key + '"';
  const int keyPosition = payload.indexOf(quotedKey);
  if (keyPosition < 0) return false;
  const int colonPosition = payload.indexOf(':', keyPosition + quotedKey.length());
  const int openingQuote = payload.indexOf('"', colonPosition + 1);
  if (colonPosition < 0 || openingQuote < 0) return false;
  value = "";
  bool escaped = false;
  for (int index = openingQuote + 1; index < payload.length(); ++index) {
    const char character = payload[index];
    if (escaped) {
      value += character;
      escaped = false;
    } else if (character == '\\') {
      escaped = true;
    } else if (character == '"') {
      return true;
    } else {
      value += character;
    }
  }
  return false;
}

bool extractJsonNumberField(const String &payload, const char *key,
                            double &value) {
  const String quotedKey = String('"') + key + '"';
  int searchFrom = 0;
  while (true) {
    const int keyPosition = payload.indexOf(quotedKey, searchFrom);
    if (keyPosition < 0) return false;
    const int colonPosition =
        payload.indexOf(':', keyPosition + quotedKey.length());
    if (colonPosition < 0) return false;
    const char *start = payload.c_str() + colonPosition + 1;
    while (*start == ' ' || *start == '\t' || *start == '\r' ||
           *start == '\n' || *start == '[')
      ++start;
    char *end = nullptr;
    value = strtod(start, &end);
    if (end != start && std::isfinite(value)) return true;
    searchFrom = keyPosition + quotedKey.length();
  }
}

bool stateAsFloat(const String &state, float &value);
int weatherCodeForState(const String &state);

int openMeteoWeatherCode(int wmoCode) {
  if (wmoCode == 0) return 800;
  if (wmoCode == 1 || wmoCode == 2) return 801;
  if (wmoCode == 3) return 804;
  if (wmoCode == 45 || wmoCode == 48) return 741;
  if (wmoCode == 51 || wmoCode == 53 || wmoCode == 55) return 300;
  if (wmoCode == 56 || wmoCode == 57) return 511;
  if (wmoCode == 61 || wmoCode == 63 || wmoCode == 80 || wmoCode == 81)
    return 500;
  if (wmoCode == 65 || wmoCode == 82) return 502;
  if (wmoCode == 66 || wmoCode == 67) return 511;
  if (wmoCode == 71 || wmoCode == 73 || wmoCode == 77 || wmoCode == 85)
    return 600;
  if (wmoCode == 75 || wmoCode == 86) return 602;
  if (wmoCode == 95) return 200;
  if (wmoCode == 96 || wmoCode == 99) return 202;
  return -1;
}

bool fetchOpenMeteo(const ClockConfig &config, ClockValues &values) {
  networkDiagnosticsBegin(NetworkDiagnosticKind::OpenMeteoRuntime);
  NetworkOperationGuard networkGuard(HOME_ASSISTANT_RESPONSE_TIMEOUT_MS);
  if (!networkGuard) {
    networkDiagnosticsEnd(NetworkDiagnosticKind::OpenMeteoRuntime, false,
                          HTTPC_ERROR_CONNECTION_REFUSED);
    return false;
  }
  String url = F("https://api.open-meteo.com/v1/forecast?latitude=");
  url += String(config.openMeteoLatitude, 5);
  url += F("&longitude=");
  url += String(config.openMeteoLongitude, 5);
  url += F("&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,precipitation,rain,showers,snowfall,weather_code,cloud_cover,pressure_msl,surface_pressure,wind_speed_10m,wind_direction_10m,wind_gusts_10m,uv_index&daily=sunrise,sunset&timeformat=unixtime&timezone=auto&forecast_days=1");
  WiFiClientSecure client;
  client.setCACert(FIRMWARE_RELEASE_ROOT_CA);
  HTTPClient http;
  http.setConnectTimeout(HOME_ASSISTANT_CONNECT_TIMEOUT_MS);
  http.setTimeout(HOME_ASSISTANT_RESPONSE_TIMEOUT_MS);
  int status = HTTPC_ERROR_CONNECTION_REFUSED;
  String payload;
  if (http.begin(client, url)) {
    status = http.GET();
    if (status == HTTP_CODE_OK) payload = http.getString();
    http.end();
  }
  if (status != HTTP_CODE_OK) {
    networkDiagnosticsEnd(NetworkDiagnosticKind::OpenMeteoRuntime, false,
                          status);
    return false;
  }
  double number = 0;
  bool ok = extractJsonNumberField(payload, "weather_code", number);
  if (ok) values.weatherCode = openMeteoWeatherCode(lround(number));
  if (extractJsonNumberField(payload, "is_day", number)) {
    values.weatherIsDay = number >= 0.5;
    values.sunStateAvailable = true;
  }
  if (extractJsonNumberField(payload, "sunrise", number))
    values.nextSunriseTimestamp = static_cast<uint64_t>(number);
  if (extractJsonNumberField(payload, "sunset", number))
    values.nextSunsetTimestamp = static_cast<uint64_t>(number);
  if (values.nextSunriseTimestamp > 0 && values.nextSunsetTimestamp > 0) {
    const time_t now = time(nullptr);
    if (now >= VALID_TIME_THRESHOLD) {
      const time_t dayStarts =
          static_cast<time_t>(values.nextSunriseTimestamp) +
          config.sunriseOffsetMinutes * 60;
      const time_t dayEnds = static_cast<time_t>(values.nextSunsetTimestamp) +
                             config.sunsetOffsetMinutes * 60;
      values.weatherIsDay = now >= dayStarts && now < dayEnds;
      values.sunStateAvailable = true;
    }
  }
  float *destinations[] = {&values.leftTemperatureC, &values.rightTemperatureC,
                           &values.metricAValue, &values.metricBValue};
  for (size_t index = 0; index < 4; ++index) {
    if (extractJsonNumberField(payload, config.openMeteoSlots[index].value,
                               number)) {
      *destinations[index] = static_cast<float>(number);
    } else {
      *destinations[index] = NAN;
      ok = false;
    }
  }
  values.homeAssistantOnline = false;
  networkDiagnosticsSetDetail(NetworkDiagnosticKind::OpenMeteoRuntime,
                              ok ? F("Aktuální data načtena")
                                 : F("Odpověď neobsahuje všechny hodnoty"));
  networkDiagnosticsEnd(NetworkDiagnosticKind::OpenMeteoRuntime, ok, status);
  return ok;
}

bool applyHomeAssistantState(const ClockConfig &config, const String &entityId,
                             const String &state, ClockValues &values) {
  float number;
  if (entityId == config.weatherEntityId) {
    values.weatherCode = weatherCodeForState(state);
  } else if (entityId == config.leftSide.temperatureEntityId &&
             stateAsFloat(state, number)) {
    values.leftTemperatureC = number;
  } else if (entityId == config.rightSide.temperatureEntityId &&
             stateAsFloat(state, number)) {
    values.rightTemperatureC = number;
  } else if (entityId == config.metricA.entityId && stateAsFloat(state, number)) {
    values.metricAValue = number;
  } else if (entityId == config.metricB.entityId && stateAsFloat(state, number)) {
    values.metricBValue = number;
  } else if (entityId == config.sunEntityId &&
             (state == "above_horizon" || state == "below_horizon")) {
    values.weatherIsDay = state == "above_horizon";
    values.sunStateAvailable = true;
  } else if (entityId == config.dayNightLightEntityId &&
             (state == "on" || state == "off")) {
    values.dayNightLightOn = state == "on";
    values.dayNightLightStateAvailable = true;
  } else {
    return false;
  }
  return true;
}

bool shouldRetryHomeAssistantRequest(int status) {
  return status <= 0 || status == 408 || status == 429 ||
         status >= 500;
}

bool requestHomeAssistantState(NetworkClient &client,
                               const ClockConfig &config,
                               const char *entityId, String &payload,
                               int &lastStatus) {
  if (entityId[0] == '\0') return false;
  NetworkOperationGuard networkGuard(HOME_ASSISTANT_RESPONSE_TIMEOUT_MS);
  if (!networkGuard) {
    lastStatus = HTTPC_ERROR_CONNECTION_REFUSED;
    return false;
  }
  const String url = String(config.homeAssistantUrl) + "/api/states/" + entityId;
  for (uint8_t attempt = 0; attempt < HOME_ASSISTANT_REQUEST_ATTEMPTS;
       ++attempt) {
    HTTPClient http;
    http.setConnectTimeout(HOME_ASSISTANT_CONNECT_TIMEOUT_MS);
    http.setTimeout(HOME_ASSISTANT_RESPONSE_TIMEOUT_MS);
    http.setReuse(true);
    if (!http.begin(client, url)) {
      lastStatus = HTTPC_ERROR_CONNECTION_REFUSED;
    } else {
      http.addHeader("Authorization",
                     String("Bearer ") + config.homeAssistantToken);
      http.addHeader("Accept", "application/json");
      lastStatus = http.GET();
      if (lastStatus == HTTP_CODE_OK) payload = http.getString();
      http.end();
      if (lastStatus == HTTP_CODE_OK) return true;
    }
    if (!shouldRetryHomeAssistantRequest(lastStatus) ||
        attempt + 1 >= HOME_ASSISTANT_REQUEST_ATTEMPTS) {
      break;
    }
    delay(HOME_ASSISTANT_REQUEST_RETRY_DELAY_MS);
  }
  return false;
}

bool parseIso8601Timestamp(const String &value, time_t &timestamp) {
  if (value.length() < 19) return false;
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  if (sscanf(value.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day,
             &hour, &minute, &second) != 6) {
    return false;
  }
  const char *end = value.c_str() + 19;
  while ((*end >= '0' && *end <= '9') || *end == '.') ++end;
  long offsetSeconds = 0;
  if (*end == '+' || *end == '-') {
    const int direction = *end == '+' ? 1 : -1;
    int hours = 0;
    int minutes = 0;
    if (sscanf(end + 1, "%2d:%2d", &hours, &minutes) != 2) return false;
    offsetSeconds = direction * (hours * 3600L + minutes * 60L);
  } else if (*end != 'Z' && *end != '\0') {
    return false;
  }
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
  const unsigned dayOfYear =
      (153U * static_cast<unsigned>(month + (month > 2 ? -3 : 9)) + 2U) /
          5U +
      static_cast<unsigned>(day - 1);
  const unsigned dayOfEra =
      yearOfEra * 365U + yearOfEra / 4U - yearOfEra / 100U + dayOfYear;
  const int64_t daysSinceEpoch =
      static_cast<int64_t>(era) * 146097 + dayOfEra - 719468;
  timestamp = static_cast<time_t>(daysSinceEpoch * 86400 + hour * 3600 +
                                  minute * 60 + second - offsetSeconds);
  return timestamp > 0;
}

bool previousLocalDayTimestamp(time_t nextTimestamp,
                               time_t &previousTimestamp) {
  if (nextTimestamp <= 0) return false;
  struct tm localTransition;
  if (localtime_r(&nextTimestamp, &localTransition) == nullptr) return false;
  --localTransition.tm_mday;
  localTransition.tm_isdst = -1;
  previousTimestamp = mktime(&localTransition);
  return previousTimestamp > 0 && previousTimestamp < nextTimestamp;
}

bool applySunState(const ClockConfig &config, const String &payload,
                   const String &state, ClockValues &values) {
  values.sunStateAvailable = false;
  if (state != "above_horizon" && state != "below_horizon") return false;
  String sunriseText;
  String sunsetText;
  time_t sunrise = 0;
  time_t sunset = 0;
  if (extractJsonStringField(payload, "next_rising", sunriseText) &&
      parseIso8601Timestamp(sunriseText, sunrise)) {
    values.nextSunriseTimestamp = static_cast<uint64_t>(sunrise);
  }
  if (extractJsonStringField(payload, "next_setting", sunsetText) &&
      parseIso8601Timestamp(sunsetText, sunset)) {
    values.nextSunsetTimestamp = static_cast<uint64_t>(sunset);
  }
  const bool horizonIsDay = state == "above_horizon";
  String lastChangedText;
  time_t lastChanged = 0;
  time_t expectedCompletedTransition = 0;
  const time_t nextSameTransition = horizonIsDay ? sunrise : sunset;
  const time_t nextUpcomingTransition = horizonIsDay ? sunset : sunrise;
  const time_t now = time(nullptr);
  const bool lastChangedAvailable =
      extractJsonStringField(payload, "last_changed", lastChangedText) &&
      parseIso8601Timestamp(lastChangedText, lastChanged);
  const bool expectedCompletedTransitionAvailable = previousLocalDayTimestamp(
      nextSameTransition, expectedCompletedTransition);
  int64_t completedTransition = 0;
  const bool completedTransitionAvailable =
      clockSelectCompletedTransitionTimestamp(
          lastChangedAvailable, static_cast<int64_t>(lastChanged),
          expectedCompletedTransitionAvailable,
          static_cast<int64_t>(expectedCompletedTransition),
          completedTransition);
  const bool nextTransitionAvailable = nextUpcomingTransition > 0;
  const ClockSunDecision decision = clockEvaluateSunDecision(
      horizonIsDay, config.sunriseOffsetMinutes, config.sunsetOffsetMinutes,
      static_cast<int64_t>(now), completedTransitionAvailable,
      completedTransition, nextTransitionAvailable,
      static_cast<int64_t>(nextUpcomingTransition));
  if (decision == ClockSunDecision::Unavailable) return false;
  values.weatherIsDay = decision == ClockSunDecision::Day;
  values.sunStateAvailable = true;
  return true;
}

bool fetchHomeAssistantStates(NetworkClient &client, const ClockConfig &config,
                              ClockValues &values) {
  networkDiagnosticsBegin(NetworkDiagnosticKind::HomeAssistantRuntime);
  const char *entityIds[] = {
      config.weatherEntityId,
      config.leftSide.temperatureEntityId,
      config.rightSide.temperatureEntityId,
      config.metricA.entityId,
      config.metricB.entityId,
      config.sunEntityId,
      config.dayNightLightEntityId,
  };
  values.sunStateAvailable = false;
  values.dayNightLightStateAvailable = false;
  uint8_t configuredCount = 0;
  uint8_t successfulCount = 0;
  int lastStatus = 0;
  for (size_t index = 0; index < 7; ++index) {
    if (entityIds[index][0] == '\0') continue;
    ++configuredCount;
    String payload;
    if (!requestHomeAssistantState(client, config, entityIds[index], payload,
                                   lastStatus)) {
      continue;
    }
    String state;
    if (!extractJsonStringField(payload, "state", state)) continue;
    bool applied = false;
    if (index == 5) {
      applied = applySunState(config, payload, state, values);
    } else {
      applied = applyHomeAssistantState(config, entityIds[index], state, values);
    }
    if (applied) ++successfulCount;
  }
  const bool apiResponded = successfulCount > 0;
  String detail = String(successfulCount) + '/' + configuredCount +
                  F(" entit načteno");
  if (!apiResponded && lastStatus != 0) {
    detail += F(", poslední výsledek ");
    detail += lastStatus;
  }
  networkDiagnosticsSetDetail(NetworkDiagnosticKind::HomeAssistantRuntime,
                              detail);
  networkDiagnosticsEnd(NetworkDiagnosticKind::HomeAssistantRuntime,
                        apiResponded,
                        apiResponded ? HTTP_CODE_OK : lastStatus);
  return apiResponded;
}

bool fetchHomeAssistantStates(const ClockConfig &config, ClockValues &values) {
  if (config.homeAssistantUrl[0] == '\0' ||
      config.homeAssistantToken[0] == '\0') {
    return false;
  }
  if (String(config.homeAssistantUrl).startsWith("https://")) {
    WiFiClientSecure client;
    client.setInsecure();
    return fetchHomeAssistantStates(client, config, values);
  }
  WiFiClient client;
  return fetchHomeAssistantStates(client, config, values);
}

bool fetchDayNightStates(NetworkClient &client, const ClockConfig &config,
                         ClockValues &values) {
  values.sunStateAvailable = false;
  values.dayNightLightStateAvailable = false;
  bool sunUpdated = false;
  bool lightUpdated = false;
  String payload;
  int status = 0;
  if (requestHomeAssistantState(client, config, config.sunEntityId, payload,
                                status)) {
    String state;
    if (extractJsonStringField(payload, "state", state)) {
      sunUpdated = applySunState(config, payload, state, values);
    }
  }
  payload = "";
  if (requestHomeAssistantState(client, config,
                                config.dayNightLightEntityId, payload,
                                status)) {
    String state;
    if (extractJsonStringField(payload, "state", state)) {
      lightUpdated = applyHomeAssistantState(
          config, config.dayNightLightEntityId, state, values);
    }
  }
  return sunUpdated || lightUpdated;
}

bool fetchDayNightStates(const ClockConfig &config, ClockValues &values) {
  if (config.homeAssistantUrl[0] == '\0' ||
      config.homeAssistantToken[0] == '\0') {
    return false;
  }
  if (String(config.homeAssistantUrl).startsWith("https://")) {
    WiFiClientSecure client;
    client.setInsecure();
    return fetchDayNightStates(client, config, values);
  }
  WiFiClient client;
  return fetchDayNightStates(client, config, values);
}


bool stateAsFloat(const String &state, float &value) {
  char *end = nullptr;
  value = strtof(state.c_str(), &end);
  return end != state.c_str() && *end == '\0' && std::isfinite(value);
}

void publishHomeAssistantValues(const ClockValues &values) {
  portENTER_CRITICAL(&homeAssistantValuesMux);
  pendingHomeAssistantValues = values;
  homeAssistantUpdatePending = true;
  portEXIT_CRITICAL(&homeAssistantValuesMux);
}

int weatherCodeForState(const String &state) {
  char *end = nullptr;
  const long numericCode = strtol(state.c_str(), &end, 10);
  if (end != state.c_str() && *end == '\0') return numericCode;
  if (state == "sunny" || state == "clear-night") return 800;
  if (state == "partlycloudy") return 801;
  if (state == "cloudy") return 804;
  if (state == "fog") return 741;
  if (state == "rainy") return 500;
  if (state == "pouring") return 502;
  if (state == "lightning") return 200;
  if (state == "lightning-rainy") return 202;
  if (state == "exceptional") return 200;
  if (state == "snowy") return 600;
  if (state == "snowy-rainy") return 616;
  if (state == "hail") return 511;
  if (state == "windy" || state == "windy-variant") return 771;
  return -1;
}

void homeAssistantTask(void *) {
  ClockValues lastAvailableValues;
  for (;;) {
    const ClockConfig config = runtimeConfigSnapshot();
    ClockValues values = lastAvailableValues;
    if (WiFi.status() != WL_CONNECTED) {
      publishHomeAssistantValues(ClockValues{});
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(HOME_ASSISTANT_RETRY_MS));
      continue;
    }

    if (config.dataSource == CLOCK_DATA_SOURCE_OPEN_METEO) {
      const bool apiResponded = fetchOpenMeteo(config, values);
      if (apiResponded) lastAvailableValues = values;
      publishHomeAssistantValues(values);
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(
                                   apiResponded ? OPEN_METEO_REFRESH_MS
                                                : HOME_ASSISTANT_RETRY_MS));
      continue;
    }

    if (config.homeAssistantUrl[0] == '\0' ||
        config.homeAssistantToken[0] == '\0') {
      publishHomeAssistantValues(ClockValues{});
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(HOME_ASSISTANT_RETRY_MS));
      continue;
    }

    const bool apiResponded = fetchHomeAssistantStates(config, values);
    values.homeAssistantOnline = apiResponded;
    if (apiResponded) {
      lastAvailableValues = values;
    }
    publishHomeAssistantValues(values);
    if (!apiResponded) {
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(HOME_ASSISTANT_RETRY_MS));
      continue;
    }

    const unsigned long fullRefreshAt = millis() + HOME_ASSISTANT_REFRESH_MS;
    while (static_cast<long>(millis() - fullRefreshAt) < 0) {
      const unsigned long remaining = fullRefreshAt - millis();
      if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(remaining)) == 0) break;
      if (!consumeDayNightLightRefreshRequest()) break;

      static ClockConfig lightConfig;
      static ClockValues lightValues;
      lightConfig = runtimeConfigSnapshot();
      lightValues = lastAvailableValues;
      fetchDayNightStates(lightConfig, lightValues);
      lastAvailableValues.weatherIsDay = lightValues.weatherIsDay;
      lastAvailableValues.sunStateAvailable = lightValues.sunStateAvailable;
      lastAvailableValues.dayNightLightStateAvailable =
          lightValues.dayNightLightStateAvailable;
      lastAvailableValues.dayNightLightOn = lightValues.dayNightLightOn;
      publishHomeAssistantValues(lightValues);
    }
  }
}

void applyPendingHomeAssistantValues() {
  if (!homeAssistantUpdatePending) return;
  ClockValues values;
  portENTER_CRITICAL(&homeAssistantValuesMux);
  values = pendingHomeAssistantValues;
  homeAssistantUpdatePending = false;
  portEXIT_CRITICAL(&homeAssistantValuesMux);
#if !FIRMWARE_RELEASE
  if (forceNightTestActive) {
    values.weatherIsDay = false;
    values.sunStateAvailable = true;
  }
#endif
  sampleValues = values;
  clockDashboardUpdate(sampleValues);
}
}  // namespace

void setup() {
  Serial.setTxBufferSize(4096);
  Serial.begin(FIRMWARE_RELEASE ? 115200 : 921600);
  delay(300);
#if !FIRMWARE_RELEASE
  Serial.println("Waveshare Hodiny startuji");
#endif

  I2C_Init();
  Set_EXIOS(0x0C);
  TCA9554PWR_Init(0x70);
  runtimeConfigMutex = xSemaphoreCreateMutex();
  // Případná migrace konfigurace zapisuje do flash. Proveď ji dříve, než
  // spustíme RGB panel nad framebufferem v PSRAM, jinak může první start po
  // OTA rozhodit řádkovou synchronizaci displeje.
  if (!clockConfigBegin() || !clockConfigLoad(persistedConfig)) {
    clockConfigApplyDefaults(persistedConfig);
#if !FIRMWARE_RELEASE
    Serial.println("Konfiguracni pamet se nepodarilo nacist");
#endif
  }
  runtimeConfig = persistedConfig;
  applyDevelopmentDefaults(runtimeConfig);
  networkCoordinatorBegin();
  LCD_Init();
  currentDisplayBrightness = runtimeConfig.dayBrightness;
  Set_Backlight(currentDisplayBrightness);
  displayDriverInit();
  clockDashboardInit(sampleValues, runtimeConfig.dayBrightness,
                       runtimeConfig.nightBrightness,
                       runtimeConfig.automaticDayNight,
                       handleBrightnessPreview, handleSettingsOpen,
                       handleSettingsSave, handleSettingsFirmwareCheck,
                       handleSettingsFirmwareInstall, handleRadarVisibility,
                       handleRadarRangeChange);
  clockDashboardApplyConfiguration(runtimeConfig);
  chmiRadarServiceBegin();
  clockDashboardSetSecond(60);
  displayResyncAt = millis() + 2000;
#if FIRMWARE_RELEASE
  improvSerialServiceInit(wifiProvisioningStart);
#endif
  initializeNetworkTime();
  xTaskCreatePinnedToCore(homeAssistantTask, "home-assistant", 12288, nullptr, 1,
                          &homeAssistantTaskHandle, 0);
  firmwareUpdateServiceBegin(handleFirmwareUpdateLifecycle);
  maintainFirmwareDisplayStatus();
  configurationWebBegin(loadRuntimeConfigForWeb, saveRuntimeConfig,
                        handleConfigurationWebStatus,
                        loadSunTransitionTimesForWeb,
                        requestHomeAssistantRefreshFromWeb,
                        loadDayNightStatusForWeb, handleDisplayPower,
                        displayPowerForcedOff, loadRadarRangeStateForWeb,
                        previewRadarRangeFromWeb);
  clockDashboardSetWebMode(configurationWebMode());

  const esp_task_wdt_config_t watchdogConfig = {
      .timeout_ms = LOOP_WATCHDOG_TIMEOUT_MS,
      .idle_core_mask = 0,
      .trigger_panic = true,
  };
  if (esp_task_wdt_reconfigure(&watchdogConfig) == ESP_OK) enableLoopWDT();
}

void loop() {
#if FIRMWARE_RELEASE
  improvSerialServiceLoop();
#else
  handleUsbCommands();
#endif
  wifiProvisioningLoop();
  applyFirmwareUpdateDisplayRequest();
  maintainNetworkTime();
  maintainAutomaticFirmwareUpdate();
  maintainFirmwareDisplayStatus();
  applyPendingHomeAssistantValues();
  const ClockConfig animationConfig = runtimeConfigSnapshot();
  const uint8_t weatherIconStyle =
      clockDashboardNightModeEnabled()
          ? CLOCK_WEATHER_ICON_STYLE_MONOCHROME
          : animationConfig.weatherIconStyle;
  weatherAnimationServiceLoop(sampleValues.weatherCode,
                              sampleValues.weatherIsDay,
                              weatherIconStyle,
                              !clockDashboardRadarVisible() &&
                                  animationConfig.animatedWeatherIcons &&
                                  (animationConfig.dataSource ==
                                       CLOCK_DATA_SOURCE_OPEN_METEO ||
                                   strcmp(animationConfig.leftSide.icon,
                                          "weather") == 0 ||
                                   strcmp(animationConfig.rightSide.icon,
                                          "weather") == 0));
  configurationWebLoop();
  applyPendingRuntimeConfiguration();
  maintainDisplayGestures();
  maintainRadarNightVisual();
  maintainRadarRangeChange();
  maintainRadarDisplay();
  maintainAutomaticRadarRotation();
  clockDashboardLoop();
  displayDriverLoop();
  if (firmwareUpdateDisplayRequested && firmwareUpdateDisplayActive) {
    firmwareUpdateDisplayPresented = true;
  }
  if (firmwareUpdateCountdownStarted && !firmwareUpdateBlackPresented) {
    const unsigned long elapsed = millis() - firmwareUpdateCountdownStartedAt;
    if (elapsed < 5000) {
      const uint8_t seconds = 5 - static_cast<uint8_t>(elapsed / 1000);
      if (seconds != firmwareUpdateCountdownDisplayed) {
        firmwareUpdateCountdownDisplayed = seconds;
        clockDashboardSetFirmwareUpdateCountdown(seconds);
      }
    } else {
      firmwareUpdateBlackRequested = true;
      firmwareUpdateCountdownStarted = false;
    }
  }
  if (firmwareUpdateBlackRequested && !firmwareUpdateBlackPresented) {
    clockDashboardSetFirmwareUpdateBlack(true);
    displayDriverLoop();
    firmwareUpdateBlackPresented = true;
  }
#if !FIRMWARE_RELEASE
  streamScreenshot();
#endif
  maintainDisplaySync();
  delay(5);
}
