#include "TmepService.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <freertos/semphr.h>

#include "FirmwareHubCa.h"
#include "NetworkCoordinator.h"

namespace {
constexpr uint32_t TMEP_CONNECT_TIMEOUT_MS = 5000;
constexpr uint32_t TMEP_RESPONSE_TIMEOUT_MS = 8000;
constexpr size_t TMEP_MAX_RESPONSE_BYTES = 64 * 1024;
StaticSemaphore_t tmepCacheMutexStorage;
SemaphoreHandle_t tmepCacheMutex = nullptr;
TmepCatalog tmepCachedCatalog;
String tmepCachedExportId;
String tmepCachedExportKey;
bool tmepCacheValid = false;

SemaphoreHandle_t cacheMutex() {
  return tmepCacheMutex;
}

void storeCachedCatalog(const char *exportId, const char *exportKey,
                        const TmepCatalog &catalog) {
  SemaphoreHandle_t mutex = cacheMutex();
  if (mutex == nullptr || xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) return;
  tmepCachedCatalog = catalog;
  tmepCachedExportId = exportId;
  tmepCachedExportKey = exportKey;
  tmepCacheValid = true;
  xSemaphoreGive(mutex);
}

String urlEncode(const char *value) {
  static constexpr char HEX_DIGITS[] = "0123456789ABCDEF";
  String result;
  const size_t length = value == nullptr ? 0 : strlen(value);
  result.reserve(length * 2);
  for (size_t index = 0; index < length; ++index) {
    const uint8_t character = static_cast<uint8_t>(value[index]);
    if ((character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9') || character == '-' ||
        character == '_' || character == '.' || character == '~') {
      result += static_cast<char>(character);
    } else {
      result += '%';
      result += HEX_DIGITS[character >> 4];
      result += HEX_DIGITS[character & 0x0F];
    }
  }
  return result;
}
}  // namespace

void tmepServiceBegin() {
  if (tmepCacheMutex == nullptr)
    tmepCacheMutex = xSemaphoreCreateMutexStatic(&tmepCacheMutexStorage);
}

bool tmepFetchCatalog(const char *exportId, const char *exportKey,
                      TmepCatalog &catalog,
                      NetworkDiagnosticKind diagnosticKind, int &httpStatus,
                      String &error) {
  catalog = TmepCatalog{};
  httpStatus = HTTPC_ERROR_CONNECTION_REFUSED;
  error = "";
  if (exportId == nullptr || exportId[0] == '\0' || exportKey == nullptr ||
      exportKey[0] == '\0') {
    error = F("Exportní URL TMEP není uložená.");
    return false;
  }

  networkDiagnosticsBegin(diagnosticKind);
  NetworkOperationGuard networkGuard(TMEP_RESPONSE_TIMEOUT_MS);
  if (!networkGuard) {
    error = F("Síť je právě vytížená jinou operací.");
    networkDiagnosticsSetDetail(diagnosticKind, error);
    networkDiagnosticsEnd(diagnosticKind, false, httpStatus);
    return false;
  }

  String url = F("https://tmep.cz/vystup-json.php?id=");
  url += urlEncode(exportId);
  url += F("&extended=1&all=1&export_key=");
  url += urlEncode(exportKey);
  WiFiClientSecure client;
  client.setCACert(FIRMWARE_RELEASE_ROOT_CA);
  HTTPClient http;
  http.setConnectTimeout(TMEP_CONNECT_TIMEOUT_MS);
  http.setTimeout(TMEP_RESPONSE_TIMEOUT_MS);
  String payload;
  if (http.begin(client, url)) {
    httpStatus = http.GET();
    if (httpStatus == HTTP_CODE_OK) {
      const int declaredSize = http.getSize();
      if (declaredSize > static_cast<int>(TMEP_MAX_RESPONSE_BYTES)) {
        error = F("TMEP export je příliš velký.");
      } else {
        payload = http.getString();
        if (payload.length() > TMEP_MAX_RESPONSE_BYTES)
          error = F("TMEP export je příliš velký.");
      }
    }
    http.end();
  }

  if (httpStatus != HTTP_CODE_OK) {
    error = F("TMEP.cz nyní není dostupný.");
  } else if (error.isEmpty()) {
    char parseError[160];
    if (!tmepParseExport(payload.c_str(), payload.length(), catalog,
                         parseError, sizeof(parseError))) {
      error = parseError;
    }
  }

  const bool ok = error.isEmpty();
  if (ok) {
    storeCachedCatalog(exportId, exportKey, catalog);
    String detail = F("Načteno čidel: ");
    detail += catalog.count;
    if (catalog.truncated) detail += F(" (seznam zkrácen)");
    networkDiagnosticsSetDetail(diagnosticKind, detail);
  } else {
    networkDiagnosticsSetDetail(diagnosticKind, error);
  }
  networkDiagnosticsEnd(diagnosticKind, ok, httpStatus);
  return ok;
}

bool tmepGetCachedCatalog(const char *exportId, const char *exportKey,
                          TmepCatalog &catalog) {
  if (exportId == nullptr || exportKey == nullptr) return false;
  SemaphoreHandle_t mutex = cacheMutex();
  if (mutex == nullptr ||
      xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) != pdTRUE)
    return false;
  const bool matches =
      tmepCacheValid && tmepCachedExportId == exportId &&
      tmepCachedExportKey == exportKey;
  if (matches) catalog = tmepCachedCatalog;
  xSemaphoreGive(mutex);
  return matches;
}

void tmepClearCachedCatalog() {
  SemaphoreHandle_t mutex = cacheMutex();
  if (mutex == nullptr || xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) return;
  tmepCachedCatalog = TmepCatalog{};
  tmepCachedExportId = "";
  tmepCachedExportKey = "";
  tmepCacheValid = false;
  xSemaphoreGive(mutex);
}
