#pragma once

#include <Arduino.h>

void wifiProvisioningBegin();
void wifiProvisioningLoop();
void wifiProvisioningPauseStartupRetries();
void wifiProvisioningStart(const String &ssid, const String &password);
bool wifiProvisioningSavePendingForRestart(const String &ssid,
                                           const String &password);
bool wifiProvisioningHasCredentials();
bool wifiProvisioningIsActive();
bool wifiProvisioningReadyForNormalStart();
