#include "WifiProvisioning.h"

#include <Preferences.h>
#include <WiFi.h>

#include "FirmwareBuild.h"
#include "ImprovSerialService.h"

#if !FIRMWARE_RELEASE && __has_include("local/secrets.h")
#include "local/secrets.h"
#define HAS_DEVELOPMENT_WIFI 1
#else
#define HAS_DEVELOPMENT_WIFI 0
#endif

namespace {
constexpr char WIFI_NAMESPACE[] = "clock-wifi";
constexpr char WIFI_SSID_KEY[] = "ssid";
constexpr char WIFI_PASSWORD_KEY[] = "password";
constexpr uint32_t WIFI_RETRY_MS = 15000;
constexpr uint32_t PROVISIONING_TIMEOUT_MS = 30000;

String storedSsid;
String storedPassword;
String pendingSsid;
String pendingPassword;
unsigned long lastWifiAttempt = 0;
unsigned long provisioningStartedAt = 0;
bool provisioningActive = false;

void loadStoredCredentials() {
#if FIRMWARE_RELEASE
  Preferences preferences;
  if (!preferences.begin(WIFI_NAMESPACE, true)) return;
  storedSsid = preferences.getString(WIFI_SSID_KEY);
  storedPassword = preferences.getString(WIFI_PASSWORD_KEY);
  preferences.end();
#elif HAS_DEVELOPMENT_WIFI
  storedSsid = WIFI_SSID;
  storedPassword = WIFI_PASSWORD;
#endif
}

bool saveStoredCredentials(const String &ssid, const String &password) {
#if FIRMWARE_RELEASE
  Preferences preferences;
  if (!preferences.begin(WIFI_NAMESPACE, false)) return false;
  const bool ok = preferences.putString(WIFI_SSID_KEY, ssid) == ssid.length() &&
                  preferences.putString(WIFI_PASSWORD_KEY, password) ==
                      password.length();
  preferences.end();
  return ok;
#else
  return true;
#endif
}

void connectStoredCredentials() {
  if (storedSsid.isEmpty()) return;
  WiFi.begin(storedSsid.c_str(), storedPassword.c_str());
  lastWifiAttempt = millis();
}
}  // namespace

void wifiProvisioningBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  loadStoredCredentials();
  connectStoredCredentials();
}

void wifiProvisioningLoop() {
  if (provisioningActive) {
    if (WiFi.status() == WL_CONNECTED) {
      if (saveStoredCredentials(pendingSsid, pendingPassword)) {
        storedSsid = pendingSsid;
        storedPassword = pendingPassword;
        provisioningActive = false;
        pendingSsid = "";
        pendingPassword = "";
        improvSerialServiceProvisioningSucceeded();
        return;
      }
      WiFi.disconnect();
      provisioningActive = false;
      pendingSsid = "";
      pendingPassword = "";
      improvSerialServiceProvisioningFailed();
      connectStoredCredentials();
      return;
    }
    if (millis() - provisioningStartedAt >= PROVISIONING_TIMEOUT_MS) {
      WiFi.disconnect();
      provisioningActive = false;
      pendingSsid = "";
      pendingPassword = "";
      improvSerialServiceProvisioningFailed();
      connectStoredCredentials();
    }
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    improvSerialServiceSetProvisioned();
    return;
  }
  if (!storedSsid.isEmpty() && millis() - lastWifiAttempt >= WIFI_RETRY_MS) {
    connectStoredCredentials();
  }
}

void wifiProvisioningStart(const String &ssid, const String &password) {
#if FIRMWARE_RELEASE
  if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 64) {
    improvSerialServiceProvisioningFailed();
    return;
  }
  pendingSsid = ssid;
  pendingPassword = password;
  provisioningActive = true;
  provisioningStartedAt = millis();
  WiFi.disconnect();
  WiFi.begin(pendingSsid.c_str(), pendingPassword.c_str());
  lastWifiAttempt = millis();
#else
  (void)ssid;
  (void)password;
#endif
}

bool wifiProvisioningHasCredentials() { return !storedSsid.isEmpty(); }

bool wifiProvisioningIsActive() { return provisioningActive; }
