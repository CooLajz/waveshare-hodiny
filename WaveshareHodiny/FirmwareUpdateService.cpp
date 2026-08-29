#include "FirmwareUpdateService.h"

#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <mbedtls/sha256.h>
#include <time.h>

#include "FirmwareBuild.h"
#include "FirmwareHubCa.h"
#include "NetworkCoordinator.h"
#include "SemVer.h"

namespace {
constexpr time_t VALID_TIME_THRESHOLD = 1700000000;
constexpr uint32_t NETWORK_TIMEOUT_MS = 15000;
constexpr size_t DOWNLOAD_BUFFER_SIZE = 4096;

SemaphoreHandle_t statusMutex = nullptr;
FirmwareUpdateSnapshot status;
FirmwareUpdateLifecycleCallback lifecycleCallback = nullptr;

void lockStatus() {
  if (statusMutex != nullptr) xSemaphoreTake(statusMutex, portMAX_DELAY);
}

void unlockStatus() {
  if (statusMutex != nullptr) xSemaphoreGive(statusMutex);
}

void setMessage(FirmwareUpdateState state, const char *message, bool busy) {
  lockStatus();
  status.state = state;
  status.busy = busy;
  strlcpy(status.message, message == nullptr ? "" : message,
          sizeof(status.message));
  unlockStatus();
}

bool extractJsonString(const String &json, const char *key, String &value) {
  const String marker = String('"') + key + '"';
  const int keyAt = json.indexOf(marker);
  if (keyAt < 0) return false;
  const int colonAt = json.indexOf(':', keyAt + marker.length());
  const int quoteAt = json.indexOf('"', colonAt + 1);
  if (colonAt < 0 || quoteAt < 0) return false;
  String result;
  bool escaped = false;
  for (int index = quoteAt + 1; index < static_cast<int>(json.length()); ++index) {
    const char character = json[index];
    if (escaped) {
      if (character == '"' || character == '\\' || character == '/') {
        result += character;
      } else {
        return false;
      }
      escaped = false;
    } else if (character == '\\') {
      escaped = true;
    } else if (character == '"') {
      value = result;
      return true;
    } else if (static_cast<uint8_t>(character) >= 0x20) {
      result += character;
    }
  }
  return false;
}

bool extractJsonUnsigned(const String &json, const char *key, uint32_t &value) {
  const String marker = String('"') + key + '"';
  const int keyAt = json.indexOf(marker);
  if (keyAt < 0) return false;
  const int colonAt = json.indexOf(':', keyAt + marker.length());
  if (colonAt < 0) return false;
  int start = colonAt + 1;
  while (start < static_cast<int>(json.length()) &&
         isspace(static_cast<unsigned char>(json[start]))) {
    ++start;
  }
  int end = start;
  while (end < static_cast<int>(json.length()) && isdigit(json[end])) ++end;
  if (end == start || end - start > 10) return false;
  const unsigned long parsed = strtoul(json.substring(start, end).c_str(), nullptr, 10);
  if (parsed == 0 || parsed > UINT32_MAX) return false;
  value = static_cast<uint32_t>(parsed);
  return true;
}

bool validSha256(const String &sha256) {
  if (sha256.length() != 64) return false;
  for (char character : sha256) {
    if (!isxdigit(static_cast<unsigned char>(character))) return false;
  }
  return true;
}

String normalizedDownloadUrl(String url) {
  if (url.startsWith("/")) url = String(FIRMWARE_SERVER_URL) + url;
  const String allowedPrefix = String(FIRMWARE_SERVER_URL) + "/";
  if (!url.startsWith(allowedPrefix)) return String();
  return url;
}

String metadataUrl() {
  if (FIRMWARE_OTA_METADATA_PATH[0] != '\0') {
    return String(FIRMWARE_SERVER_URL) + FIRMWARE_OTA_METADATA_PATH;
  }
  return String(FIRMWARE_SERVER_URL) + "/api/v1/projects/" +
         FIRMWARE_PROJECT_SLUG + "/ota";
}

String sha256Hex(const uint8_t digest[32]) {
  static constexpr char HEX_DIGITS[] = "0123456789abcdef";
  char result[65];
  for (size_t index = 0; index < 32; ++index) {
    result[index * 2] = HEX_DIGITS[digest[index] >> 4];
    result[index * 2 + 1] = HEX_DIGITS[digest[index] & 0x0F];
  }
  result[64] = '\0';
  return String(result);
}

bool installFirmware(const String &url, uint32_t expectedSize,
                     const String &expectedSha256) {
  setMessage(FirmwareUpdateState::Downloading,
             "Stahuji a ověřuji nový firmware…", true);
  lockStatus();
  status.downloadedBytes = 0;
  status.totalBytes = expectedSize;
  unlockStatus();
  if (lifecycleCallback != nullptr) lifecycleCallback(true);

  WiFiClientSecure client;
  client.setCACert(FIRMWARE_RELEASE_ROOT_CA);
  client.setTimeout(NETWORK_TIMEOUT_MS);
  HTTPClient http;
  http.setConnectTimeout(NETWORK_TIMEOUT_MS);
  http.setTimeout(NETWORK_TIMEOUT_MS);
  if (!http.begin(client, url)) {
    setMessage(FirmwareUpdateState::Failed,
               "Nepodařilo se otevřít OTA adresu.", false);
    if (lifecycleCallback != nullptr) lifecycleCallback(false);
    return false;
  }
  const int responseCode = http.GET();
  if (responseCode != HTTP_CODE_OK) {
    http.end();
    setMessage(FirmwareUpdateState::Failed,
               "Server nevydal aplikační OTA obraz.", false);
    if (lifecycleCallback != nullptr) lifecycleCallback(false);
    return false;
  }
  const int contentLength = http.getSize();
  if (contentLength < 0 || static_cast<uint32_t>(contentLength) != expectedSize) {
    http.end();
    setMessage(FirmwareUpdateState::Failed,
               "Velikost OTA obrazu neodpovídá metadatům.", false);
    if (lifecycleCallback != nullptr) lifecycleCallback(false);
    return false;
  }
  if (!Update.begin(expectedSize, U_FLASH)) {
    http.end();
    setMessage(FirmwareUpdateState::Failed,
               "OTA obraz se nevejde do neaktivního slotu.", false);
    if (lifecycleCallback != nullptr) lifecycleCallback(false);
    return false;
  }

  mbedtls_sha256_context shaContext;
  mbedtls_sha256_init(&shaContext);
  mbedtls_sha256_starts(&shaContext, 0);
  NetworkClient *stream = http.getStreamPtr();
  uint8_t buffer[DOWNLOAD_BUFFER_SIZE];
  uint32_t received = 0;
  unsigned long lastDataAt = millis();
  bool writeOk = true;
  while (received < expectedSize) {
    const int available = stream->available();
    if (available <= 0) {
      if (!http.connected() || millis() - lastDataAt >= NETWORK_TIMEOUT_MS) {
        writeOk = false;
        break;
      }
      delay(1);
      continue;
    }
    const size_t wanted =
        min(static_cast<size_t>(available),
            min(sizeof(buffer), static_cast<size_t>(expectedSize - received)));
    const int count = stream->readBytes(buffer, wanted);
    if (count <= 0 || Update.write(buffer, count) != static_cast<size_t>(count)) {
      writeOk = false;
      break;
    }
    mbedtls_sha256_update(&shaContext, buffer, count);
    received += count;
    lastDataAt = millis();
    lockStatus();
    status.downloadedBytes = received;
    unlockStatus();
  }

  uint8_t digest[32];
  mbedtls_sha256_finish(&shaContext, digest);
  mbedtls_sha256_free(&shaContext);
  http.end();
  String actualSha256 = sha256Hex(digest);
  actualSha256.toLowerCase();
  String requiredSha256 = expectedSha256;
  requiredSha256.toLowerCase();
  if (!writeOk || received != expectedSize || actualSha256 != requiredSha256) {
    Update.abort();
    setMessage(FirmwareUpdateState::Failed,
               actualSha256 == requiredSha256
                   ? "Stažení OTA obrazu nebylo dokončeno."
                   : "Kontrolní SHA-256 OTA obrazu nesouhlasí.",
               false);
    if (lifecycleCallback != nullptr) lifecycleCallback(false);
    return false;
  }
  if (!Update.end(true)) {
    Update.abort();
    setMessage(FirmwareUpdateState::Failed,
               "OTA obraz se nepodařilo aktivovat.", false);
    if (lifecycleCallback != nullptr) lifecycleCallback(false);
    return false;
  }

  setMessage(FirmwareUpdateState::Restarting,
             "Aktualizace je ověřená, zařízení se restartuje…", true);
  delay(750);
  ESP.restart();
  return true;
}

void checkFirmware(bool installWhenAvailable) {
  if (WiFi.status() != WL_CONNECTED) {
    setMessage(FirmwareUpdateState::Failed,
               "Zařízení není připojené k Wi-Fi.", false);
    return;
  }
  if (FIRMWARE_SERVER_URL[0] == '\0' || FIRMWARE_PROJECT_SLUG[0] == '\0') {
    setMessage(FirmwareUpdateState::Failed,
               "Server aktualizací není v buildu nakonfigurovaný.", false);
    return;
  }
  if (time(nullptr) < VALID_TIME_THRESHOLD) {
    setMessage(FirmwareUpdateState::Failed,
               "Čas ještě není synchronizovaný pro bezpečné HTTPS.", false);
    return;
  }

  NetworkOperationGuard networkGuard(NETWORK_TIMEOUT_MS);
  if (!networkGuard) {
    setMessage(FirmwareUpdateState::Failed,
               "Síť je právě vytížená jinou operací.", false);
    return;
  }

  const String metadataEndpoint = metadataUrl();
  WiFiClientSecure client;
  client.setCACert(FIRMWARE_RELEASE_ROOT_CA);
  client.setTimeout(NETWORK_TIMEOUT_MS);
  HTTPClient http;
  http.setConnectTimeout(NETWORK_TIMEOUT_MS);
  http.setTimeout(NETWORK_TIMEOUT_MS);
  if (!http.begin(client, metadataEndpoint)) {
    setMessage(FirmwareUpdateState::Failed,
               "Nepodařilo se otevřít server aktualizací.", false);
    return;
  }
  http.addHeader("Accept", "application/json");
  const int responseCode = http.GET();
  if (responseCode == HTTP_CODE_NOT_FOUND) {
    http.end();
    setMessage(FirmwareUpdateState::Failed,
               "Server zatím nemá OTA metadata pro tento projekt.",
               false);
    return;
  }
  if (responseCode != HTTP_CODE_OK) {
    String detail;
    if (responseCode < 0) {
      char tlsError[160] = {};
      client.lastError(tlsError, sizeof(tlsError));
      detail = tlsError[0] != '\0' ? tlsError
                                   : HTTPClient::errorToString(responseCode);
    } else {
      detail = String("HTTP ") + responseCode;
    }
    http.end();
    const String message =
        String("Kontrola verze na serveru selhala: ") + detail + ".";
    setMessage(FirmwareUpdateState::Failed, message.c_str(), false);
    return;
  }
  const String payload = http.getString();
  http.end();

  String version;
  String chipFamily;
  String sha256;
  String url;
  uint32_t size = 0;
  if (!extractJsonString(payload, "version", version) ||
      !extractJsonString(payload, "chipFamily", chipFamily) ||
      !extractJsonString(payload, "sha256", sha256) ||
      !extractJsonString(payload, "url", url) ||
      !extractJsonUnsigned(payload, "size", size) ||
      !semVerIsValid(version.c_str()) ||
      chipFamily != FIRMWARE_CHIP_VARIANT || !validSha256(sha256)) {
    setMessage(FirmwareUpdateState::Failed,
               "OTA metadata ze serveru nejsou platná.", false);
    return;
  }
  url = normalizedDownloadUrl(url);
  if (url.isEmpty()) {
    setMessage(FirmwareUpdateState::Failed,
               "OTA obraz neleží na povoleném serveru.", false);
    return;
  }

  const bool newer =
      !IS_RELEASE_FIRMWARE || semVerCompare(FIRMWARE_VERSION, version.c_str()) < 0;
  lockStatus();
  strlcpy(status.serverVersion, version.c_str(), sizeof(status.serverVersion));
  status.totalBytes = size;
  status.downloadedBytes = 0;
  status.updateAvailable = newer;
  unlockStatus();
  if (!newer) {
    setMessage(FirmwareUpdateState::Current,
               "Používáš aktuální verzi firmware.", false);
    return;
  }
  if (!installWhenAvailable) {
    setMessage(FirmwareUpdateState::Available,
               IS_RELEASE_FIRMWARE
                   ? "Na serveru je dostupná novější verze."
                   : "Novější release je dostupný; development se aktualizuje kabelem.",
               false);
    return;
  }
  if (!IS_RELEASE_FIRMWARE) {
    setMessage(FirmwareUpdateState::Available,
               "Development build se aktualizuje pouze kabelem.", false);
    return;
  }
  installFirmware(url, size, sha256);
}

void updateTask(void *parameter) {
  const bool installWhenAvailable = reinterpret_cast<uintptr_t>(parameter) != 0;
  checkFirmware(installWhenAvailable);
  vTaskDelete(nullptr);
}
}  // namespace

void firmwareUpdateServiceBegin(FirmwareUpdateLifecycleCallback callback) {
  lifecycleCallback = callback;
  if (statusMutex == nullptr) statusMutex = xSemaphoreCreateMutex();
  lockStatus();
  status = FirmwareUpdateSnapshot{};
  strlcpy(status.currentVersion, FIRMWARE_VERSION,
          sizeof(status.currentVersion));
  strlcpy(status.message,
          IS_RELEASE_FIRMWARE
              ? "Aktualizace zatím nebyla zkontrolována."
              : "Development build se nadále nahrává přes USB.",
          sizeof(status.message));
  status.installationSupported = IS_RELEASE_FIRMWARE;
  unlockStatus();
}

bool firmwareUpdateServiceRequestCheck(bool installWhenAvailable) {
  lockStatus();
  if (status.busy) {
    unlockStatus();
    return false;
  }
  status.busy = true;
  status.state = FirmwareUpdateState::Checking;
  strlcpy(status.message, "Kontroluji novou verzi…",
          sizeof(status.message));
  unlockStatus();
  const BaseType_t created = xTaskCreatePinnedToCore(
      updateTask, "firmware-update", 12288,
      reinterpret_cast<void *>(installWhenAvailable ? 1 : 0), 1, nullptr, 0);
  if (created != pdPASS) {
    setMessage(FirmwareUpdateState::Failed,
               "OTA úlohu se nepodařilo spustit.", false);
    return false;
  }
  return true;
}

FirmwareUpdateSnapshot firmwareUpdateServiceSnapshot() {
  lockStatus();
  const FirmwareUpdateSnapshot snapshot = status;
  unlockStatus();
  return snapshot;
}

const char *firmwareUpdateStateName(FirmwareUpdateState state) {
  switch (state) {
    case FirmwareUpdateState::Idle: return "idle";
    case FirmwareUpdateState::Checking: return "checking";
    case FirmwareUpdateState::Available: return "available";
    case FirmwareUpdateState::Current: return "current";
    case FirmwareUpdateState::Downloading: return "downloading";
    case FirmwareUpdateState::Failed: return "failed";
    case FirmwareUpdateState::Restarting: return "restarting";
  }
  return "unknown";
}
