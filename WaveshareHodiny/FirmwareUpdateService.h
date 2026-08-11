#pragma once

#include <Arduino.h>

enum class FirmwareUpdateState : uint8_t {
  Idle,
  Checking,
  Available,
  Current,
  Downloading,
  Failed,
  Restarting,
};

struct FirmwareUpdateSnapshot {
  FirmwareUpdateState state = FirmwareUpdateState::Idle;
  char currentVersion[48] = "";
  char serverVersion[48] = "";
  char message[160] = "";
  uint32_t downloadedBytes = 0;
  uint32_t totalBytes = 0;
  bool updateAvailable = false;
  bool busy = false;
  bool installationSupported = false;
};

using FirmwareUpdateLifecycleCallback = void (*)(bool updating);

void firmwareUpdateServiceBegin(FirmwareUpdateLifecycleCallback callback);
bool firmwareUpdateServiceRequestCheck(bool installWhenAvailable);
FirmwareUpdateSnapshot firmwareUpdateServiceSnapshot();
const char *firmwareUpdateStateName(FirmwareUpdateState state);
