#pragma once

#include <Arduino.h>

void networkCoordinatorBegin();
bool networkCoordinatorAcquire(uint32_t timeoutMs);
void networkCoordinatorRelease();

class NetworkOperationGuard {
 public:
  explicit NetworkOperationGuard(uint32_t timeoutMs)
      : acquired_(networkCoordinatorAcquire(timeoutMs)) {}
  ~NetworkOperationGuard() {
    if (acquired_) networkCoordinatorRelease();
  }

  NetworkOperationGuard(const NetworkOperationGuard &) = delete;
  NetworkOperationGuard &operator=(const NetworkOperationGuard &) = delete;

  explicit operator bool() const { return acquired_; }

 private:
  bool acquired_;
};
