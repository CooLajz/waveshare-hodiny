#pragma once

#include <Arduino.h>
#include <vector>

constexpr int WIFI_STA = 1;
constexpr int WIFI_AP_STA = 3;
constexpr int WL_DISCONNECTED = 6;
constexpr int WL_CONNECTED = 3;
constexpr int WIFI_SCAN_RUNNING = -1;
constexpr int WIFI_SCAN_FAILED = -2;
constexpr int WIFI_ALL_CHANNEL_SCAN = 1;
constexpr int WIFI_CONNECT_AP_BY_SIGNAL = 1;

inline unsigned long testMillis = 0;
inline unsigned long millis() { return testMillis; }

struct TestWiFi {
  int currentMode = WIFI_STA;
  int connectionStatus = WL_DISCONNECTED;
  int scanStatus = WIFI_SCAN_FAILED;
  bool autoReconnect = false;
  bool persistentRequested = true;
  bool driverInitialized = false;
  bool driverFlashStorage = true;
  unsigned configurationApplications = 0;
  String configuredSsid;
  std::vector<String> attempts;
  void mode(int value) {
    if (!driverInitialized) {
      driverInitialized = true;
      driverFlashStorage = persistentRequested;
    }
    currentMode = value;
  }
  void setAutoReconnect(bool value) { autoReconnect = value; }
  void persistent(bool value) { persistentRequested = value; }
  void setScanMethod(int) {}
  void setSortMethod(int) {}
  void begin(const char *ssid, const char *) {
    ++configurationApplications;
    configuredSsid = ssid;
    attempts.emplace_back(ssid);
    connectionStatus = WL_DISCONNECTED;
  }
  bool reconnect() {
    attempts.push_back(configuredSsid);
    connectionStatus = WL_DISCONNECTED;
    return true;
  }
  int status() { return connectionStatus; }
  int scanComplete() { return scanStatus; }
  void disconnect() { connectionStatus = WL_DISCONNECTED; }
};

inline TestWiFi WiFi;
