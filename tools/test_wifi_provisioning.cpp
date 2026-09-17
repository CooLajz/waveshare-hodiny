#include <cassert>
#include <cstdio>
#include <Preferences.h>
#include <WiFi.h>
#include "../WaveshareHodiny/WifiProvisioning.h"

unsigned successes = 0;
unsigned failures = 0;
void improvSerialServiceSetProvisioned() {}
void improvSerialServiceProvisioningSucceeded() { ++successes; }
void improvSerialServiceProvisioningFailed() { ++failures; }

void advance(unsigned long milliseconds) {
  testMillis += milliseconds;
  wifiProvisioningLoop();
}

int main(int argc, char **argv) {
  assert(argc == 2);
  const String scenario = argv[1];
  Preferences storage;
  assert(storage.begin("clock-wifi"));
  if (scenario != "empty") {
    storage.putString("ssid", "saved-network");
    storage.putString("password", "synthetic-password");
  }
  const auto originalStorage = fakeNvs::values;
  if (scenario == "promotion-failure") {
    storage.putString("next-ssid", "candidate-network");
    storage.putString("next-password", "synthetic-candidate-password");
    storage.putBool("next-valid", true);
  }
  wifiProvisioningBegin();
  assert(!WiFi.driverFlashStorage);
  assert(wifiProvisioningNextRetrySeconds() == (scenario == "empty" ? -1 : 60));
  advance(30000);
  assert(wifiProvisioningNextRetrySeconds() == (scenario == "empty" ? -1 : 30));
  assert(WiFi.attempts.size() == (scenario == "empty" ? 0 : 1));
  wifiProvisioningBeginPortal();
  assert(WiFi.currentMode == WIFI_AP_STA);
  assert(WiFi.autoReconnect);
  advance(29999);
  assert(wifiProvisioningNextRetrySeconds() == (scenario == "empty" ? -1 : 1));
  assert(WiFi.attempts.size() == (scenario == "empty" ? 0 : 1));
  advance(1);
  assert(wifiProvisioningNextRetrySeconds() == (scenario == "empty" ? -1 : 60));
  assert(WiFi.attempts.size() == (scenario == "empty" ? 0 : 2));

  if (scenario == "late-router") {
    for (int i = 0; i < 10; ++i) {
      const auto attempts = WiFi.attempts.size();
      advance(59999);
      assert(wifiProvisioningNextRetrySeconds() == 1);
      assert(WiFi.attempts.size() == attempts);
      advance(1);
      assert(wifiProvisioningNextRetrySeconds() == 60);
      assert(WiFi.attempts.size() == attempts + 1);
    }
    assert(WiFi.attempts.size() == 12);
    assert(WiFi.configurationApplications == 1);
    assert(!wifiProvisioningReadyForNormalStart());
    assert(fakeNvs::values == originalStorage);
    WiFi.connectionStatus = WL_CONNECTED;
    wifiProvisioningLoop();
    assert(wifiProvisioningReadyForNormalStart());
    assert(wifiProvisioningNextRetrySeconds() == -1);
    advance(60000);
    assert(WiFi.attempts.size() == 12);
    assert(fakeNvs::values == originalStorage);
  } else if (scenario == "scan") {
    const auto attempts = WiFi.attempts.size();
    WiFi.scanStatus = WIFI_SCAN_RUNNING;
    advance(60000);
    assert(wifiProvisioningNextRetrySeconds() == 0);
    assert(WiFi.attempts.size() == attempts);
    WiFi.scanStatus = 4;
    wifiProvisioningLoop();
    assert(WiFi.attempts.size() == attempts + 1);
    assert(wifiProvisioningNextRetrySeconds() == 60);
  } else if (scenario == "improv-success") {
    // Starting a manual request must also clear a previous ready flag.
    WiFi.connectionStatus = WL_CONNECTED;
    wifiProvisioningLoop();
    wifiProvisioningStart("manual-network", "synthetic-new-password");
    assert(wifiProvisioningNextRetrySeconds() == -1);
    assert(!wifiProvisioningReadyForNormalStart());
    const auto attempts = WiFi.attempts.size();
    advance(15000);
    assert(WiFi.attempts.size() == attempts);
    WiFi.connectionStatus = WL_CONNECTED;
    wifiProvisioningLoop();
    assert(successes == 1 && failures == 0);
    assert(storage.getString("ssid") == "manual-network");
    assert(wifiProvisioningReadyForNormalStart());
  } else if (scenario == "improv-failure") {
    wifiProvisioningStart("unavailable-network", "synthetic-new-password");
    const auto attempts = WiFi.attempts.size();
    advance(15000);
    assert(WiFi.attempts.size() == attempts);
    advance(15000);
    assert(failures == 1 && successes == 0);
    assert(WiFi.attempts.back() == "saved-network");
    assert(WiFi.configuredSsid == "saved-network");
    assert(fakeNvs::values == originalStorage);
    advance(60000);
    assert(WiFi.attempts.size() == attempts + 2);
  } else if (scenario == "promotion-failure") {
    assert(WiFi.configuredSsid == "candidate-network");
    fakeNvs::fault = fakeNvs::WifiCredentialWrite;
    WiFi.connectionStatus = WL_CONNECTED;
    wifiProvisioningLoop();
    assert(!wifiProvisioningReadyForNormalStart());
    assert(WiFi.configuredSsid == "saved-network");
    assert(storage.getString("ssid") == "saved-network");
    fakeNvs::fault = fakeNvs::None;
    advance(60000);
    assert(WiFi.attempts.back() == "saved-network");
  } else if (scenario == "portal-save") {
    assert(!wifiProvisioningSavePendingForRestart("", "invalid"));
    assert(WiFi.autoReconnect);
    assert(fakeNvs::values == originalStorage);
    wifiProvisioningStart("usb-network", "synthetic-usb-password");
    assert(wifiProvisioningSavePendingForRestart("portal-network", "synthetic-portal-password"));
    assert(!wifiProvisioningIsActive());
    assert(!wifiProvisioningReadyForNormalStart());
    assert(!WiFi.autoReconnect);
    assert(wifiProvisioningNextRetrySeconds() == -1);
    const auto attempts = WiFi.attempts.size();
    advance(60000);
    assert(WiFi.attempts.size() == attempts);
    assert(storage.getString("ssid") == "saved-network");
    assert(storage.getString("next-ssid") == "portal-network");
    assert(storage.getBool("next-valid"));
    assert(successes == 0);
    // Simulate the scheduled restart and successful candidate promotion.
    wifiProvisioningBegin();
    assert(WiFi.attempts.back() == "portal-network");
    WiFi.connectionStatus = WL_CONNECTED;
    wifiProvisioningLoop();
    assert(storage.getString("ssid") == "portal-network");
    assert(!storage.getBool("next-valid"));
    assert(wifiProvisioningReadyForNormalStart());
  } else {
    assert(scenario == "empty");
    advance(300000);
    assert(WiFi.attempts.empty());
    assert(!wifiProvisioningReadyForNormalStart());
    assert(fakeNvs::values == originalStorage);
  }
  assert(WiFi.currentMode == (scenario == "portal-save" ? WIFI_STA : WIFI_AP_STA));
  printf("PASS: %s\n", scenario.c_str());
}
