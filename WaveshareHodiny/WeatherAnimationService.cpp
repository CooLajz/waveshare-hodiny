#include "WeatherAnimationService.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <mbedtls/sha256.h>
#include <time.h>

#include "ClockDashboard.h"
#include "FirmwareBuild.h"
#include "FirmwareHubCa.h"
#include "NetworkDiagnostics.h"
#include "WeatherIconMapping.h"

namespace {
constexpr char MONOCHROME_ASSET_VERSION[] =
    "3.0.0-next.10-lvgl.2-monochrome";
constexpr char FLAT_ASSET_VERSION[] = "3.0.0-next.10-lvgl.2-flat";
constexpr char LINE_ASSET_VERSION[] = "3.0.0-next.10-lvgl.2-line";
constexpr size_t MAX_ASSET_SIZE = 192 * 1024;
constexpr uint32_t NETWORK_TIMEOUT_MS = 15000;
constexpr uint32_t RETRY_DELAY_MS = 5000;
constexpr time_t VALID_TIME_THRESHOLD = 1700000000;

struct AssetMetadata {
  const char *key;
  size_t size;
  const char *sha256;
};

constexpr AssetMetadata ASSETS[] = {
    {"flat-clear-day", 45430, "aab1ce756c0495a7f09e7bca6e63be22e9bb0e409db191ddaa82475dd8251fe5"},
    {"flat-clear-night", 28383, "dc2339e724b42f84dc1a6ecc189c3775422e3e247257583996fe58a0add3b9e1"},
    {"flat-drizzle", 36725, "2c425268dc12a0921f0f1a7ad002c7bc19c4820771bccf213cf9c9c78fc83d06"},
    {"flat-mist", 23263, "bcea5d72103dba7680a3f6497126168916aab2ab9b08c9ec0f5f4ee0790b22e8"},
    {"flat-mostly-clear-day", 44014, "1d7a7991fedef717fab7b8325606d59700fedb5b138cb3b1267308260bb5caff"},
    {"flat-mostly-clear-night", 30127, "a44e76a8ab1139f298a842a3bc977f61a2070cf5a5a5fefa171745f51f5bb8c9"},
    {"flat-overcast", 33617, "e56a327d0c8b89b57e9c4aa6ec1159ffe9ec1b3ca113ff8ce5a2ae51ab8f4c78"},
    {"flat-partly-cloudy-day", 38323, "ab85c45cc8597516502af1d373a2751d71cfa6e14c3d7f26742d9090bba035c2"},
    {"flat-partly-cloudy-night", 33425, "e6599067d986c97edb350cd2b820a9aca75dee98c7157f9dae08b6b50220bab9"},
    {"flat-rain", 41033, "8541020851ed041c49a2f4495baef1fbd4bfe8d039c0bbd0175ae0f32b1afb2a"},
    {"flat-sleet", 45323, "6278aff0c183796a6a240e8b315f16f4cf778ca0eb14dadcbe4998ac60137819"},
    {"flat-snow", 36329, "3133262e1adb455a379541ffe1e4c5d37923884bc7aa394243ba067c9917dcef"},
    {"flat-thunderstorms", 39755, "5c7ebb1c5b62ee61f70d7a75140faa6b0cd5a688efda1c896114546184948efe"},
    {"line-clear-day", 48032, "2f8f7337ea642298543535d227b3c2bffa7bf3c5d1707c215af3f44a31da67bb"},
    {"line-clear-night", 32337, "ebc47d863f093cfbfbbb00d54de3985a12185d8bbffdea967581f15025b158a2"},
    {"line-drizzle", 37057, "21ea5715d1a691ee04e95b23a6d288fd448c5e046619237d1e7fa098c9940ce8"},
    {"line-mist", 23263, "bcea5d72103dba7680a3f6497126168916aab2ab9b08c9ec0f5f4ee0790b22e8"},
    {"line-mostly-clear-day", 45148, "c687377d3d26027957068da86ea8604fd55d9c94ad6bc555b5b461d44d9c6efe"},
    {"line-mostly-clear-night", 32401, "0f48ffb37360e93f53c2ebc8d1b168c9960df4d396e1475c41176f2c0c506eeb"},
    {"line-overcast", 36793, "bbd3614382fd4f84bf812a858347f1cd1a1557f934a0b5237a39d2924cff80e6"},
    {"line-partly-cloudy-day", 41442, "97c1c7f30a6bb5309dca9a959684e7e66af830cfb0e2f02188d45cf99ecb56a4"},
    {"line-partly-cloudy-night", 36369, "a79c5a2fc3f09af74eb1c21535a305484220070d192388f2072f90853a4af911"},
    {"line-rain", 39327, "0c8bf5a2f1096bcb29509d4bab0798f12673beaea371e71fa1e32e5a991a62f0"},
    {"line-sleet", 45763, "2d91f59db3e138f0557e547f3c7d3d4d7ccdd812cdcd2e7bb88d1edf3e3cec15"},
    {"line-snow", 37087, "f1c51cefcef475be4d001f6f9d14991f4eeb65c1b31b53cc8da534b1f3be23dc"},
    {"line-thunderstorms", 38543, "f421ef30d8f27b0bc07b410e4165f005c1f6bbbc70a29aee5e89413bc2ab67e0"},
    {"monochrome-clear-day", 48370, "79cefa1df9c3527dd71fb83514e64d91760dc84c3cbb72bb3177473bb0cf038b"},
    {"monochrome-clear-night", 32337, "41bb72ae190d35ddff3f06151d97d2e918f4ae0656129357beb0c7d79c70fbae"},
    {"monochrome-drizzle", 36077, "6a9814875c12a41f4dbb0b26302f8446bd4fb1537c48b5dc7d250b159ccb6901"},
    {"monochrome-mist", 23167, "5aeb305a2342b9be29fe14222989d01d5bd7b54bd54711bbcb5dca23f10152d8"},
    {"monochrome-mostly-clear-day", 43703, "da84b73666bbb1392ee5267340a5eb375ea72c70cc3a4358cc3fcdffecdc7c2c"},
    {"monochrome-mostly-clear-night", 30527, "968f216b6e0ddcd2eff10b8d818062c6c556e0d992b6ac14740fa2f6908590e9"},
    {"monochrome-overcast", 34199, "0a8fe231fcd95db8adc86bf54e3fb8ad69f061b4dd0f819a7623a6e1274f1c60"},
    {"monochrome-partly-cloudy-day", 38651, "bc20cce5625f46df0fcf842d4ca92fd724c4454329602f454711e9a4ceb105af"},
    {"monochrome-partly-cloudy-night", 33951, "1344f689dbad49ecf09b9988d7b74dad592e432768c1c2b9804a47022b0b5e1f"},
    {"monochrome-rain", 38835, "ee27536c0ded223e1ae5c5185c9a75d0dccf39fa326442266bd1ab4eaae8e05b"},
    {"monochrome-sleet", 41493, "a87046aaa09774882e4e49d85241d07a8169868035a32de64f7b3ee018663491"},
    {"monochrome-snow", 34895, "0fcea6e85419a9adb75f3c31d65ce77652fa72f60477c91bd2d97b85b8d08a5b"},
    {"monochrome-thunderstorms", 36698, "a143658776cc52ed46106845cc2e6e1efcaa3b132a6e7a7bce1add80cd1bb05d"},
};

const char *assetVersionForKey(const char *key) {
  if (strncmp(key, "flat-", 5) == 0) return FLAT_ASSET_VERSION;
  if (strncmp(key, "line-", 5) == 0) return LINE_ASSET_VERSION;
  return MONOCHROME_ASSET_VERSION;
}

enum class DownloadState : uint8_t { Idle, Downloading, Ready, Failed };

portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;
volatile DownloadState state = DownloadState::Idle;
const AssetMetadata *requestedAsset = nullptr;
const AssetMetadata *downloadedAsset = nullptr;
const AssetMetadata *installedAsset = nullptr;
uint8_t *downloadedData = nullptr;
uint8_t *installedData = nullptr;
volatile unsigned long lastFailureAt = 0;

const AssetMetadata *metadataForKey(const char *key) {
  if (key == nullptr) return nullptr;
  for (const AssetMetadata &asset : ASSETS) {
    if (strcmp(asset.key, key) == 0) return &asset;
  }
  return nullptr;
}

String digestHex(const uint8_t digest[32]) {
  static const char HEX_DIGITS[] = "0123456789abcdef";
  char result[65];
  for (size_t index = 0; index < 32; ++index) {
    result[index * 2] = HEX_DIGITS[digest[index] >> 4];
    result[index * 2 + 1] = HEX_DIGITS[digest[index] & 0x0F];
  }
  result[64] = '\0';
  return String(result);
}

void setState(DownloadState nextState) {
  portENTER_CRITICAL(&stateMux);
  state = nextState;
  portEXIT_CRITICAL(&stateMux);
}

void setFailed() {
  portENTER_CRITICAL(&stateMux);
  lastFailureAt = millis();
  state = DownloadState::Failed;
  portEXIT_CRITICAL(&stateMux);
}

void performDownload() {
  const AssetMetadata *asset = requestedAsset;
  bool success = false;
  uint8_t *data = nullptr;
  int result = 0;
  networkDiagnosticsBegin(NetworkDiagnosticKind::WeatherAnimation);
  String url = String(FIRMWARE_SERVER_URL);
  if (FIRMWARE_WEATHER_ASSET_PATH[0] != '\0') {
    url += FIRMWARE_WEATHER_ASSET_PATH;
  } else {
    url += String("/api/v1/projects/") + FIRMWARE_PROJECT_SLUG +
           "/assets/weather-icons";
  }
  url += String("/") + assetVersionForKey(asset->key) + "/" + asset->key +
         ".gif";

  WiFiClientSecure client;
  client.setCACert(FIRMWARE_RELEASE_ROOT_CA);
  client.setTimeout(NETWORK_TIMEOUT_MS);
  HTTPClient http;
  http.setConnectTimeout(NETWORK_TIMEOUT_MS);
  http.setTimeout(NETWORK_TIMEOUT_MS);
  if (asset->size <= MAX_ASSET_SIZE && http.begin(client, url)) {
    result = http.GET();
  }
  if (result == HTTP_CODE_OK &&
      http.getSize() == static_cast<int>(asset->size)) {
    data = static_cast<uint8_t *>(
        heap_caps_malloc(asset->size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (data != nullptr) {
      mbedtls_sha256_context shaContext;
      mbedtls_sha256_init(&shaContext);
      mbedtls_sha256_starts(&shaContext, 0);
      NetworkClient *stream = http.getStreamPtr();
      size_t received = 0;
      unsigned long lastDataAt = millis();
      while (received < asset->size) {
        const int available = stream->available();
        if (available <= 0) {
          if (!http.connected() || millis() - lastDataAt >= NETWORK_TIMEOUT_MS)
            break;
          delay(1);
          continue;
        }
        const size_t wanted =
            min(static_cast<size_t>(available), asset->size - received);
        const int count = stream->readBytes(data + received, wanted);
        if (count <= 0) break;
        mbedtls_sha256_update(&shaContext, data + received, count);
        received += count;
        lastDataAt = millis();
      }
      uint8_t digest[32];
      mbedtls_sha256_finish(&shaContext, digest);
      mbedtls_sha256_free(&shaContext);
      success = received == asset->size && digestHex(digest) == asset->sha256;
    }
  }
  http.end();
  networkDiagnosticsEnd(NetworkDiagnosticKind::WeatherAnimation, success,
                        result);

  if (success) {
    portENTER_CRITICAL(&stateMux);
    downloadedData = data;
    downloadedAsset = asset;
    state = DownloadState::Ready;
    portEXIT_CRITICAL(&stateMux);
  } else {
    if (data != nullptr) heap_caps_free(data);
    setFailed();
  }
}

void downloadTask(void *) {
  // Return from the C++ function before deleting the FreeRTOS task. Calling
  // vTaskDelete() directly from performDownload() would skip stack unwinding
  // and leak the HTTP/TLS objects after every animation change.
  performDownload();
  vTaskDelete(nullptr);
}
}  // namespace

void weatherAnimationServiceLoop(int weatherCode, bool isDay, uint8_t style,
                                 bool enabled) {
  char desiredKey[48] = "";
  const AssetMetadata *desired =
      enabled && weatherAnimationAssetKey(desiredKey, sizeof(desiredKey),
                                          weatherCode, isDay, style)
          ? metadataForKey(desiredKey)
          : nullptr;
  DownloadState currentState;
  portENTER_CRITICAL(&stateMux);
  currentState = state;
  portEXIT_CRITICAL(&stateMux);

  if (currentState == DownloadState::Ready) {
    uint8_t *oldData = installedData;
    clockDashboardSetWeatherAnimation(downloadedData, downloadedAsset->size,
                                      downloadedAsset->key);
    installedData = downloadedData;
    installedAsset = downloadedAsset;
    downloadedData = nullptr;
    downloadedAsset = nullptr;
    setState(DownloadState::Idle);
    if (oldData != nullptr) heap_caps_free(oldData);
    currentState = DownloadState::Idle;
  }

  if (currentState == DownloadState::Failed) {
    unsigned long failedAt;
    portENTER_CRITICAL(&stateMux);
    failedAt = lastFailureAt;
    portEXIT_CRITICAL(&stateMux);
    if (desired == requestedAsset && millis() - failedAt < RETRY_DELAY_MS)
      return;
    setState(DownloadState::Idle);
    currentState = DownloadState::Idle;
  }
  if (currentState != DownloadState::Idle || desired == nullptr ||
      desired == installedAsset || WiFi.status() != WL_CONNECTED ||
      time(nullptr) < VALID_TIME_THRESHOLD || FIRMWARE_SERVER_URL[0] == '\0' ||
      FIRMWARE_PROJECT_SLUG[0] == '\0') {
    return;
  }
  requestedAsset = desired;
  setState(DownloadState::Downloading);
  if (xTaskCreatePinnedToCore(downloadTask, "weather-animation", 8192, nullptr,
                              1, nullptr, 0) != pdPASS) {
    setFailed();
  }
}
