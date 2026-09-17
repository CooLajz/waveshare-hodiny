#pragma once

#include <Arduino.h>

void wifiProvisioningBegin();
void wifiProvisioningLoop();
void wifiProvisioningBeginPortal();
void wifiProvisioningStart(const String &ssid, const String &password);
bool wifiProvisioningSavePendingForRestart(const String &ssid,
                                           const String &password);
bool wifiProvisioningHasCredentials();
bool wifiProvisioningIsActive();
bool wifiProvisioningReadyForNormalStart();
// Remaining time for the scheduled retry, or -1 while retries are inactive.
int32_t wifiProvisioningNextRetrySeconds();
