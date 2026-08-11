#pragma once

#include <Arduino.h>

using ImprovCredentialsCallback = void (*)(const String &ssid,
                                           const String &password);

void improvSerialServiceInit(ImprovCredentialsCallback credentialsCallback);
void improvSerialServiceLoop();
void improvSerialServiceSetProvisioned();
void improvSerialServiceProvisioningSucceeded();
void improvSerialServiceProvisioningFailed();
