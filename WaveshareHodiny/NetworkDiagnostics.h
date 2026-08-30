#pragma once

#include <Arduino.h>

enum class NetworkDiagnosticKind : uint8_t {
  HomeAssistantRuntime = 0,
  HomeAssistantTest = 1,
  WeatherAnimation = 2,
  OpenMeteoRuntime = 3,
  OpenMeteoTest = 4,
  TmepRuntime = 5,
  TmepTest = 6,
  Count = 7,
};

struct NetworkMemorySnapshot {
  size_t internalFree = 0;
  size_t internalLargest = 0;
  size_t psramFree = 0;
  size_t psramLargest = 0;
};

struct NetworkDiagnosticSnapshot {
  uint32_t attempts = 0;
  uint32_t successes = 0;
  uint32_t failures = 0;
  int lastResult = 0;
  bool lastSuccess = false;
  unsigned long lastStartedAt = 0;
  unsigned long lastFinishedAt = 0;
  NetworkMemorySnapshot before;
  NetworkMemorySnapshot after;
  char detail[192] = {};
};

void networkDiagnosticsBegin(NetworkDiagnosticKind kind);
void networkDiagnosticsEnd(NetworkDiagnosticKind kind, bool success,
                           int result);
void networkDiagnosticsSetDetail(NetworkDiagnosticKind kind,
                                 const String &detail);
void networkDiagnosticsReset(NetworkDiagnosticKind kind);
NetworkDiagnosticSnapshot networkDiagnosticsSnapshot(
    NetworkDiagnosticKind kind);
NetworkMemorySnapshot networkDiagnosticsCurrentMemory();
