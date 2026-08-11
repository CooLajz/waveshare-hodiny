#pragma once

#include <Arduino.h>

void wifiProvisioningBegin();
void wifiProvisioningLoop();
void wifiProvisioningStart(const String &ssid, const String &password);
bool wifiProvisioningHasCredentials();
bool wifiProvisioningIsActive();
