#include "TmepService.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <freertos/semphr.h>

#include "FirmwareHubCa.h"
#include "NetworkCoordinator.h"

namespace {
constexpr uint32_t TMEP_CONNECT_TIMEOUT_MS = 5000;
constexpr uint32_t TMEP_RESPONSE_TIMEOUT_MS = 8000;
constexpr size_t TMEP_MAX_RESPONSE_BYTES = 64 * 1024;
constexpr size_t TMEP_UNKNOWN_RESPONSE_RESERVE_BYTES = 4096;
StaticSemaphore_t tmepCatalogMutexStorage;
SemaphoreHandle_t tmepCatalogMutex = nullptr;
TmepCatalog *tmepCatalog = nullptr;
String tmepCachedExportId;
String tmepCachedExportKey;
bool tmepCacheValid = false;

class BoundedStringStream : public Stream {
 public:
  BoundedStringStream(String &destination, size_t maximumLength)
      : destination_(destination), maximumLength_(maximumLength) {}

  using Print::write;

  size_t write(uint8_t value) override { return write(&value, 1); }

  size_t write(const uint8_t *buffer, size_t size) override {
    if (buffer == nullptr || size == 0) return 0;
    const size_t remaining = maximumLength_ - destination_.length();
    const size_t accepted = size < remaining ? size : remaining;
    if (accepted > 0 &&
        !destination_.concat(reinterpret_cast<const char *>(buffer), accepted)) {
      allocationFailed_ = true;
      setWriteError();
      return 0;
    }
    if (accepted != size) {
      overflowed_ = true;
      setWriteError();
    }
    return accepted;
  }

  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}

  bool overflowed() const { return overflowed_; }
  bool allocationFailed() const { return allocationFailed_; }

 private:
  String &destination_;
  size_t maximumLength_;
  bool overflowed_ = false;
  bool allocationFailed_ = false;
};

TmepCatalog *ensureCatalog() {
  if (tmepCatalog == nullptr) {
    tmepCatalog = static_cast<TmepCatalog *>(heap_caps_calloc(
        1, sizeof(TmepCatalog), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  }
  return tmepCatalog;
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
  if (tmepCatalogMutex == nullptr)
    tmepCatalogMutex = xSemaphoreCreateMutexStatic(&tmepCatalogMutexStorage);
}

bool tmepFetchCatalog(const char *exportId, const char *exportKey,
                      TmepCatalogVisitor visitor, void *visitorContext,
                      NetworkDiagnosticKind diagnosticKind, int &httpStatus,
                      String &error) {
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

  if (tmepCatalogMutex == nullptr ||
      xSemaphoreTake(tmepCatalogMutex, portMAX_DELAY) != pdTRUE) {
    error = F("Katalog TMEP nyní není dostupný.");
    networkDiagnosticsSetDetail(diagnosticKind, error);
    networkDiagnosticsEnd(diagnosticKind, false, httpStatus);
    return false;
  }
  TmepCatalog *catalog = ensureCatalog();
  if (catalog == nullptr) {
    error = F("Pro katalog TMEP není dostatek PSRAM.");
    xSemaphoreGive(tmepCatalogMutex);
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
        const size_t reserveSize =
            declaredSize >= 0
                ? static_cast<size_t>(declaredSize)
                : TMEP_UNKNOWN_RESPONSE_RESERVE_BYTES;
        if (!payload.reserve(reserveSize)) {
          error = F("Pro TMEP export není dostatek paměti.");
        } else {
          BoundedStringStream response(payload, TMEP_MAX_RESPONSE_BYTES);
          const int bytesRead = http.writeToStream(&response);
          if (response.overflowed()) {
            error = F("TMEP export je příliš velký.");
          } else if (response.allocationFailed()) {
            error = F("Pro TMEP export není dostatek paměti.");
          } else if (bytesRead < 0) {
            error = F("TMEP.cz nyní není dostupný.");
          }
        }
      }
    }
    http.end();
  }

  bool catalogModified = false;
  if (httpStatus != HTTP_CODE_OK) {
    error = F("TMEP.cz nyní není dostupný.");
  } else if (error.isEmpty()) {
    catalogModified = true;
    char parseError[160];
    if (!tmepParseExport(payload.c_str(), payload.length(), *catalog,
                         parseError, sizeof(parseError))) {
      error = parseError;
    }
  }

  const bool ok = error.isEmpty();
  if (ok) {
    tmepCachedExportId = exportId;
    tmepCachedExportKey = exportKey;
    tmepCacheValid = true;
    if (visitor != nullptr) visitor(*catalog, visitorContext);
    String detail = F("Načteno čidel: ");
    detail += catalog->count;
    if (catalog->truncated) detail += F(" (seznam zkrácen)");
    networkDiagnosticsSetDetail(diagnosticKind, detail);
  } else {
    if (catalogModified) tmepCacheValid = false;
    networkDiagnosticsSetDetail(diagnosticKind, error);
  }
  xSemaphoreGive(tmepCatalogMutex);
  networkDiagnosticsEnd(diagnosticKind, ok, httpStatus);
  return ok;
}

bool tmepVisitCachedCatalog(const char *exportId, const char *exportKey,
                            TmepCatalogVisitor visitor, void *visitorContext) {
  if (exportId == nullptr || exportKey == nullptr) return false;
  if (tmepCatalogMutex == nullptr ||
      xSemaphoreTake(tmepCatalogMutex, pdMS_TO_TICKS(50)) != pdTRUE)
    return false;
  const bool matches =
      tmepCatalog != nullptr && tmepCacheValid && tmepCachedExportId == exportId &&
      tmepCachedExportKey == exportKey;
  if (matches && visitor != nullptr) visitor(*tmepCatalog, visitorContext);
  xSemaphoreGive(tmepCatalogMutex);
  return matches;
}

void tmepClearCachedCatalog() {
  if (tmepCatalogMutex == nullptr ||
      xSemaphoreTake(tmepCatalogMutex, portMAX_DELAY) != pdTRUE)
    return;
  if (tmepCatalog != nullptr) *tmepCatalog = TmepCatalog{};
  tmepCachedExportId = "";
  tmepCachedExportKey = "";
  tmepCacheValid = false;
  xSemaphoreGive(tmepCatalogMutex);
}
