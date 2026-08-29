#include "NetworkCoordinator.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace {
SemaphoreHandle_t networkMutex = nullptr;
}

void networkCoordinatorBegin() {
  if (networkMutex == nullptr) networkMutex = xSemaphoreCreateMutex();
}

bool networkCoordinatorAcquire(uint32_t timeoutMs) {
  if (networkMutex == nullptr) return false;
  return xSemaphoreTake(networkMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void networkCoordinatorRelease() {
  if (networkMutex != nullptr) xSemaphoreGive(networkMutex);
}
