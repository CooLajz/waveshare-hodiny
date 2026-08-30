#include "NetworkDiagnostics.h"

#include <esp_heap_caps.h>

namespace {
portMUX_TYPE diagnosticsMux = portMUX_INITIALIZER_UNLOCKED;
NetworkDiagnosticSnapshot snapshots[
    static_cast<size_t>(NetworkDiagnosticKind::Count)];

size_t indexFor(NetworkDiagnosticKind kind) {
  return static_cast<size_t>(kind);
}
}  // namespace

NetworkMemorySnapshot networkDiagnosticsCurrentMemory() {
  NetworkMemorySnapshot snapshot;
  snapshot.internalFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  snapshot.internalLargest =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  snapshot.psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  snapshot.psramLargest =
      heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  return snapshot;
}

void networkDiagnosticsBegin(NetworkDiagnosticKind kind) {
  const NetworkMemorySnapshot memory = networkDiagnosticsCurrentMemory();
  portENTER_CRITICAL(&diagnosticsMux);
  NetworkDiagnosticSnapshot &snapshot = snapshots[indexFor(kind)];
  ++snapshot.attempts;
  snapshot.lastStartedAt = millis();
  snapshot.before = memory;
  snapshot.detail[0] = '\0';
  portEXIT_CRITICAL(&diagnosticsMux);
}

void networkDiagnosticsSetDetail(NetworkDiagnosticKind kind,
                                 const String &detail) {
  portENTER_CRITICAL(&diagnosticsMux);
  strlcpy(snapshots[indexFor(kind)].detail, detail.c_str(),
          sizeof(snapshots[indexFor(kind)].detail));
  portEXIT_CRITICAL(&diagnosticsMux);
}

void networkDiagnosticsEnd(NetworkDiagnosticKind kind, bool success,
                           int result) {
  const NetworkMemorySnapshot memory = networkDiagnosticsCurrentMemory();
  portENTER_CRITICAL(&diagnosticsMux);
  NetworkDiagnosticSnapshot &snapshot = snapshots[indexFor(kind)];
  snapshot.lastSuccess = success;
  if (success) {
    ++snapshot.successes;
  } else {
    ++snapshot.failures;
  }
  snapshot.lastResult = result;
  snapshot.lastFinishedAt = millis();
  snapshot.after = memory;
  portEXIT_CRITICAL(&diagnosticsMux);
}

NetworkDiagnosticSnapshot networkDiagnosticsSnapshot(
    NetworkDiagnosticKind kind) {
  portENTER_CRITICAL(&diagnosticsMux);
  const NetworkDiagnosticSnapshot snapshot = snapshots[indexFor(kind)];
  portEXIT_CRITICAL(&diagnosticsMux);
  return snapshot;
}
