#pragma once

#include <Arduino.h>

constexpr uint16_t CHMI_RADAR_WIDTH = 480;
constexpr uint16_t CHMI_RADAR_HEIGHT = 480;

struct ChmiRadarSnapshot {
  const uint16_t *pixels = nullptr;
  uint32_t generation = 0;
  uint32_t completedAnimationCycles = 0;
  bool loading = false;
  bool ready = false;
  bool latestFrame = false;
  uint8_t currentFrameNumber = 0;
  uint8_t animationFrameCount = 0;
  uint8_t pauseSeconds = 5;
  uint16_t radiusKm = 50;
  char frameTime[6] = "";
  char message[64] = "Čekám na otevření radaru";
};

struct ChmiRadarDiagnostics {
  bool active = false;
  bool loading = false;
  bool ready = false;
  bool preparationInProgress = false;
  bool lastSuccessfulRefreshAvailable = false;
  uint8_t requestedFrameCount = 0;
  uint8_t preparedFrameCount = 0;
  uint8_t animationFrameCount = 0;
  uint8_t pendingRefreshCount = 0;
  uint16_t radiusKm = 50;
  uint32_t lastSuccessfulRefreshAgeMs = 0;
  uint32_t nextRefreshInMs = 0;
  int lastHttpStatus = 0;
  size_t lastDownloadedBytes = 0;
  int lastDecodeResult = 0;
  uint16_t lastDecodedLineCount = 0;
  bool acceptedCompleteDecodeError = false;
  char latestIndexFile[64] = "";
  char currentFile[64] = "";
  char oldestFrameTime[6] = "";
  char newestFrameTime[6] = "";
  char message[64] = "";
};

void chmiRadarServiceBegin();
void chmiRadarServiceSetActive(bool active, float latitude, float longitude,
                               uint16_t radiusKm, uint8_t frameCount,
                               uint8_t mapOpacity, uint8_t pauseSeconds);
void chmiRadarServiceSetRedNightMode(bool enabled);
void chmiRadarServiceSnapshot(ChmiRadarSnapshot &snapshot);
void chmiRadarServiceDiagnostics(ChmiRadarDiagnostics &diagnostics);
