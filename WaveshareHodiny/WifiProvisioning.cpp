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
constexpr char WIFI_PENDING_SSID_KEY[] = "next-ssid";
constexpr char WIFI_PENDING_PASSWORD_KEY[] = "next-password";
constexpr char WIFI_PENDING_VALID_KEY[] = "next-valid";
constexpr uint32_t WIFI_RETRY_MS = 15000;
constexpr uint32_t PROVISIONING_TIMEOUT_MS = 30000;

String storedSsid;
String storedPassword;
String startupSsid;
String startupPassword;
String pendingSsid;
String pendingPassword;
unsigned long lastWifiAttempt = 0;
unsigned long provisioningStartedAt = 0;
bool provisioningActive = false;
bool startupCandidateActive = false;
bool normalStartReady = false;
bool startupRetriesEnabled = true;

void loadStoredCredentials() {
#if FIRMWARE_RELEASE
  Preferences preferences;
  if (!preferences.begin(WIFI_NAMESPACE, true)) return;
  storedSsid = preferences.getString(WIFI_SSID_KEY);
  storedPassword = preferences.getString(WIFI_PASSWORD_KEY);
  if (preferences.getBool(WIFI_PENDING_VALID_KEY, false)) {
    startupSsid = preferences.getString(WIFI_PENDING_SSID_KEY);
    startupPassword = preferences.getString(WIFI_PENDING_PASSWORD_KEY);
    startupCandidateActive = !startupSsid.isEmpty();
  }
  preferences.end();
#elif HAS_DEVELOPMENT_WIFI
  storedSsid = WIFI_SSID;
  storedPassword = WIFI_PASSWORD;
#endif

  if (!startupCandidateActive) {
    startupSsid = storedSsid;
    startupPassword = storedPassword;
  }
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

bool clearPendingCredentials() {
#if FIRMWARE_RELEASE
  Preferences preferences;
  if (!preferences.begin(WIFI_NAMESPACE, false)) return false;
  const bool ok = preferences.remove(WIFI_PENDING_VALID_KEY);
  preferences.remove(WIFI_PENDING_SSID_KEY);
  preferences.remove(WIFI_PENDING_PASSWORD_KEY);
  preferences.end();
  return ok;
#else
  return true;
#endif
}

bool savePendingCredentials(const String &ssid, const String &password) {
#if FIRMWARE_RELEASE
  Preferences preferences;
  if (!preferences.begin(WIFI_NAMESPACE, false)) return false;
  preferences.remove(WIFI_PENDING_VALID_KEY);
  const bool valuesWritten =
      preferences.putString(WIFI_PENDING_SSID_KEY, ssid) == ssid.length() &&
      preferences.putString(WIFI_PENDING_PASSWORD_KEY, password) ==
          password.length();
  const bool ok =
      valuesWritten && preferences.putBool(WIFI_PENDING_VALID_KEY, true) == 1;
  preferences.end();
  return ok;
#else
  (void)ssid;
  (void)password;
  return false;
#endif
}

void beginWifiConnection(const String &ssid, const String &password) {
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  WiFi.begin(ssid.c_str(), password.c_str());
}

void connectStoredCredentials() {
  if (startupSsid.isEmpty()) return;
  beginWifiConnection(startupSsid, startupPassword);
  lastWifiAttempt = millis();
}
}  // namespace

void wifiProvisioningBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  normalStartReady = false;
  startupRetriesEnabled = true;
  startupCandidateActive = false;
  startupSsid = "";
  startupPassword = "";
  loadStoredCredentials();
  connectStoredCredentials();
}

void wifiProvisioningLoop() {
  if (provisioningActive) {
    if (WiFi.status() == WL_CONNECTED) {
      if (saveStoredCredentials(pendingSsid, pendingPassword)) {
        clearPendingCredentials();
        storedSsid = pendingSsid;
        storedPassword = pendingPassword;
        startupSsid = storedSsid;
        startupPassword = storedPassword;
        startupCandidateActive = false;
        normalStartReady = true;
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
      if (startupRetriesEnabled) connectStoredCredentials();
      return;
    }
    if (millis() - provisioningStartedAt >= PROVISIONING_TIMEOUT_MS) {
      WiFi.disconnect();
      provisioningActive = false;
      pendingSsid = "";
      pendingPassword = "";
      improvSerialServiceProvisioningFailed();
      if (startupRetriesEnabled) connectStoredCredentials();
    }
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (startupCandidateActive) {
      if (!saveStoredCredentials(startupSsid, startupPassword) ||
          !clearPendingCredentials()) {
        WiFi.disconnect();
        startupCandidateActive = false;
        startupSsid = storedSsid;
        startupPassword = storedPassword;
        return;
      }
      storedSsid = startupSsid;
      storedPassword = startupPassword;
      startupCandidateActive = false;
    }
    normalStartReady = true;
    improvSerialServiceSetProvisioned();
    return;
  }
  normalStartReady = false;
  if (startupRetriesEnabled && !startupSsid.isEmpty() &&
      millis() - lastWifiAttempt >= WIFI_RETRY_MS) {
    connectStoredCredentials();
  }
}

void wifiProvisioningPauseStartupRetries() {
  startupRetriesEnabled = false;
  normalStartReady = false;
  WiFi.setAutoReconnect(false);
  WiFi.disconnect();
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
  beginWifiConnection(pendingSsid, pendingPassword);
  lastWifiAttempt = millis();
#else
  (void)ssid;
  (void)password;
#endif
}

bool wifiProvisioningSavePendingForRestart(const String &ssid,
                                           const String &password) {
#if FIRMWARE_RELEASE
  if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 64)
    return false;
  return savePendingCredentials(ssid, password);
#else
  (void)ssid;
  (void)password;
  return false;
#endif
}

bool wifiProvisioningHasCredentials() { return !startupSsid.isEmpty(); }

bool wifiProvisioningIsActive() { return provisioningActive; }

bool wifiProvisioningReadyForNormalStart() { return normalStartReady; }
