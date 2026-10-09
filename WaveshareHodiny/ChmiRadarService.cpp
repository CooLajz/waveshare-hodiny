#include "ClockTimezone.h"
#include "ChmiRadarService.h"
#include "ClockConfig.h"

#include <HTTPClient.h>
#include <PNGdec.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <freertos/idf_additions.h>

#include <atomic>
#include <cctype>
#include <cmath>
#include <cstring>
#include <new>
#include <utility>
#include <time.h>

#include "ChmiCa.h"
#include "CzechMapData.h"
#include "SlovakMapData.h"
#include "SlovakRadarManifest.h"
#include "SlovakRadarFrame.h"
#include "SlovakRadarRendering.h"
#include "FirmwareHubCa.h"
#include <mbedtls/sha256.h>
#include "NetworkCoordinator.h"
#include "RadarClientIdentity.h"
#include "RadarHttpBody.h"

namespace {
constexpr char INDEX_URL[] =
    "https://opendata.chmi.cz/meteorology/weather/radar/composite/maxz/png/";
constexpr char MASKED_INDEX_URL[] =
    "https://opendata.chmi.cz/meteorology/weather/radar/composite/maxz/png_masked/";
// The worker keeps one source for its entire request, including all downloads.
uint8_t requestedSource = CLOCK_RADAR_SOURCE_MAX_Z;
uint8_t workerSource = CLOCK_RADAR_SOURCE_MAX_Z;
const char *radarIndexUrl() {
  return workerSource == CLOCK_RADAR_SOURCE_MAX_Z_MASKED ? MASKED_INDEX_URL : INDEX_URL;
}
constexpr char FILE_PREFIX[] = "pacz2gmaps3.z_max3d.";
constexpr size_t FILE_NAME_CAPACITY = slovakRadar::PATH_CAPACITY;
bool slovakSource() { return workerSource == CLOCK_RADAR_SOURCE_SHMU; }
slovakRadar::Frame *slovakFrames = nullptr;
size_t slovakFrameCount = 0;
void slovakCacheName(const slovakRadar::Frame &frame, char *name) {
  // Content hash covers both precipitation and coverage.
  strlcpy(name, frame.image.path, FILE_NAME_CAPACITY);
}
const slovakRadar::Frame *slovakFrame(const char *name) {
  for (size_t i = 0; i < slovakFrameCount; ++i) {
    char expected[FILE_NAME_CAPACITY];
    slovakCacheName(slovakFrames[i], expected);
    if (strcmp(expected, name) == 0) return &slovakFrames[i];
  }
  return nullptr;
}
// Download workspace; cached frames allocate only their actual compressed size.
constexpr size_t PNG_CAPACITY = slovakRadar::FRAME_LIMIT;
constexpr size_t MAX_ANIMATION_FRAME_COUNT = 15;
constexpr size_t MAX_PENDING_REFRESH_FRAMES = 4;
constexpr size_t DISPLAY_BUFFER_COUNT = 2;
constexpr size_t RADAR_PIXEL_COUNT = CHMI_RADAR_WIDTH * CHMI_RADAR_HEIGHT;
constexpr unsigned long REFRESH_INTERVAL_MS = 300000;
constexpr unsigned long RETRY_INTERVAL_MS = 60000;
constexpr time_t VALID_TIME_THRESHOLD = 1700000000;
constexpr unsigned long ANIMATION_STEP_MS = 500;
constexpr unsigned long PREPARATION_FRAME_MIN_MS = 500;
constexpr int RADAR_SOURCE_WIDTH = 680;
constexpr int RADAR_SOURCE_HEIGHT = 460;


constexpr float LON_LEFT = 11.267f;
constexpr float LON_RIGHT = 20.770f;
constexpr float LAT_TOP = 52.167f;
constexpr float LAT_BOTTOM = 48.047f;
constexpr float LON_DATA_RIGHT = 19.624f;
constexpr float LAT_DATA_TOP = 51.458f;
constexpr float WHOLE_COUNTRY_LATITUDE = 49.805f;
constexpr float WHOLE_COUNTRY_LONGITUDE = 15.475f;
constexpr uint16_t WHOLE_COUNTRY_RADIUS_KM = 260;

TaskHandle_t taskHandle = nullptr;
std::atomic<bool> memoryReclaimRequested{false};

void *allocateRadarMemory(size_t bytes, uint32_t caps) {
  void *result = heap_caps_malloc(bytes, caps);
  if (result || !(caps & MALLOC_CAP_SPIRAM) ||
      xTaskGetCurrentTaskHandle() != taskHandle) return result;
  // Only the UI task may invalidate LVGL sources and release its caches.
  memoryReclaimRequested.store(true, std::memory_order_release);
  const TickType_t started = xTaskGetTickCount();
  while (memoryReclaimRequested.load(std::memory_order_acquire) &&
         xTaskGetTickCount() - started < pdMS_TO_TICKS(500)) {
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  return heap_caps_malloc(bytes, caps);
}

portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;
bool active = false;
bool visible = false;
float centerLatitude = 49.1951f;
float centerLongitude = 16.6068f;
uint16_t centerRadiusKm = 50;
uint8_t requestedFrameCount = 6;
uint8_t mapOpacity = 100;
uint8_t pauseSeconds = 5;
uint16_t *displayBuffers[DISPLAY_BUFFER_COUNT] = {};
uint8_t *preparedFrames[MAX_ANIMATION_FRAME_COUNT] = {};
bool preparedFrameReady[MAX_ANIMATION_FRAME_COUNT] = {};
uint32_t preparedFrameRevisions[MAX_ANIMATION_FRAME_COUNT] = {};
char preparedFrameTimes[MAX_ANIMATION_FRAME_COUNT][6] = {};
char (*preparedFrameNames)[FILE_NAME_CAPACITY] = nullptr;
uint8_t activeDisplayBuffer = 0;
uint16_t activeRadiusKm = 50;
bool rebuildFromCacheRequested = false;
bool reloadRequested = false;
bool showBaseMapRequested = false;
bool restartAnimationRequested = false;
bool redNightMode = false;
bool nightVisualRedrawRequested = false;
size_t animationFrameCount = 0;
int displayedFrame = -1;
uint32_t generation = 0;
uint32_t completedAnimationCycles = 0;
bool loading = false;
bool ready = false;
bool animationPause = false;
bool preparationInProgress = false;
bool fullPreparationInProgress = false;
unsigned long lastAnimationStepAt = 0;
unsigned long animationPauseStartedAt = 0;
unsigned long lastProgressiveFrameShownAt = 0;
char frameTime[6] = "";
char statusMessage[64] = "Čekám na otevření radaru";
unsigned long nextAttemptAt = 0;
unsigned long lastSuccessfulRefreshAt = 0;
uint32_t requestRevision = 0;
int lastHttpStatus = 0;
size_t lastDownloadedBytes = 0;
int lastDecodeResult = PNG_SUCCESS;
uint16_t lastDecodedLineCount = 0;
bool acceptedCompleteDecodeError = false;
char latestIndexFile[FILE_NAME_CAPACITY] = "";
char currentFile[FILE_NAME_CAPACITY] = "";

PNG *pngDecoder = nullptr;
uint8_t *pngBuffer = nullptr;
size_t pngBufferCapacity = 0;
uint8_t *cachedPngFrames[MAX_ANIMATION_FRAME_COUNT] = {};
size_t cachedPngSizes[MAX_ANIMATION_FRAME_COUNT] = {};
size_t cachedPngCapacities[MAX_ANIMATION_FRAME_COUNT] = {};
char (*cachedPngNames)[FILE_NAME_CAPACITY] = nullptr;
size_t cachedPngCount = 0;
uint8_t *pendingPreparedFrames[MAX_PENDING_REFRESH_FRAMES] = {};
uint8_t *pendingPngFrames[MAX_PENDING_REFRESH_FRAMES] = {};
size_t pendingPngSizes[MAX_PENDING_REFRESH_FRAMES] = {};
size_t pendingPngCapacities[MAX_PENDING_REFRESH_FRAMES] = {};
char (*pendingFrameNames)[FILE_NAME_CAPACITY] = nullptr;
char pendingFrameTimes[MAX_PENDING_REFRESH_FRAMES][6] = {};
uint32_t pendingFrameRevisions[MAX_PENDING_REFRESH_FRAMES] = {};
size_t pendingRefreshCount = 0;
uint16_t *lineBuffer = nullptr;
size_t lineCapacity = 0;
uint8_t *decodeTarget = nullptr;
int imageWidth = 0;
int imageHeight = 0;
int sourceX[CHMI_RADAR_WIDTH] = {};
int sourceY[CHMI_RADAR_HEIGHT] = {};
slovakRadarRender::View slovakView{};
int dataX1 = 0;
int dataY0 = 0;
uint16_t decodedLineCount = 0;
bool decodedLinesSequential = true;

void advanceAnimation(unsigned long now);
bool showPreparedFrame(size_t index, unsigned long now);
int firstPreparedFrame();
uint8_t rgb565ToRgb332(uint16_t color);
uint16_t rgb332ToRgb565(uint8_t color);
uint16_t nightRadarColor(uint8_t color);
void applyNightRadarPalette(uint16_t *buffer);
uint8_t encodePreparedColor(uint16_t color) {
  return slovakSource() ? slovakRadarRender::encode(color) : rgb565ToRgb332(color);
}
uint16_t decodePreparedColor(uint8_t color) {
  return slovakSource() ? slovakRadarRender::decode(color) : rgb332ToRgb565(color);
}

bool ensurePngDecoder() {
  if (pngDecoder != nullptr) return true;
  void *storage = allocateRadarMemory(sizeof(PNG),
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (storage == nullptr) return false;
  pngDecoder = new (storage) PNG();
  return true;
}

unsigned long millisecondsUntilNextRefreshSlot() {
  // SHMÚ manifest je levná kontrola; nezměněné snímky zůstávají v cache.
  if (slovakSource()) return 60000UL;
  const time_t now = time(nullptr);
  if (now < VALID_TIME_THRESHOLD) return REFRESH_INTERVAL_MS;
  struct tm localTime = {};
  if (clockLocaltime(&now, &localTime) == nullptr) return REFRESH_INTERVAL_MS;

  // ČHMÚ publikuje pravidelné snímky po pěti minutách. Kontrolujeme je
  // pevně o minutu později (:01, :06, :11, ...), aby se interval neposouval
  // podle délky předchozího stahování.
  int targetMinute = (localTime.tm_min / 5) * 5 + 1;
  if (targetMinute < localTime.tm_min ||
      (targetMinute == localTime.tm_min && localTime.tm_sec >= 0))
    targetMinute += 5;
  int delaySeconds =
      (targetMinute - localTime.tm_min) * 60 - localTime.tm_sec;
  if (delaySeconds <= 0) delaySeconds += 300;
  return static_cast<unsigned long>(delaySeconds) * 1000UL;
}

bool requestMatches(uint32_t revision) {
  portENTER_CRITICAL(&stateMux);
  const bool matches = active && revision == requestRevision;
  portEXIT_CRITICAL(&stateMux);
  return matches;
}

float mercatorY(float latitude) {
  const float radians = latitude * 0.017453292519943295f;
  return logf(tanf(0.7853981633974483f + radians * 0.5f));
}

long daysFromCivil(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
  const unsigned dayOfYear =
      (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned dayOfEra =
      yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return static_cast<long>(era) * 146097 + static_cast<long>(dayOfEra) -
         719468;
}

int longitudeToX(float longitude) {
  if (slovakSource()) return lround(slovakRadarRender::edgeX(longitude) - 0.5);
  return lroundf((longitude - LON_LEFT) * (imageWidth - 1) /
                 (LON_RIGHT - LON_LEFT));
}

int latitudeToY(float latitude) {
  if (slovakSource()) return lround(slovakRadarRender::edgeY(latitude) - 0.5);
  const float top = mercatorY(LAT_TOP);
  const float bottom = mercatorY(LAT_BOTTOM);
  return lroundf((top - mercatorY(latitude)) * (imageHeight - 1) /
                 (top - bottom));
}

void setStatus(bool isLoading, const char *message) {
  portENTER_CRITICAL(&stateMux);
  loading = isLoading;
  strlcpy(statusMessage, message, sizeof(statusMessage));
  portEXIT_CRITICAL(&stateMux);
}

bool ensureBuffers() {
  for (uint16_t *&buffer : displayBuffers) {
    if (buffer == nullptr) {
      buffer = static_cast<uint16_t *>(allocateRadarMemory(
          RADAR_PIXEL_COUNT * sizeof(uint16_t),
          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    }
    if (buffer == nullptr) return false;
  }
  const size_t capacity = slovakSource() ? PNG_CAPACITY : 131072;
  if (pngBufferCapacity != capacity) {
    // No source decoder retains this workspace between requests.
    heap_caps_free(pngBuffer);
    pngBuffer = nullptr;
    pngBufferCapacity = 0;
  }
  if (!pngBuffer) {
    pngBuffer = static_cast<uint8_t *>(allocateRadarMemory(
        capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (pngBuffer) pngBufferCapacity = capacity;
  }
  return pngBuffer != nullptr;
}

bool ensurePreparedFrame(size_t index) {
  if (index >= MAX_ANIMATION_FRAME_COUNT) return false;
  if (preparedFrames[index] == nullptr) {
    preparedFrames[index] = static_cast<uint8_t *>(allocateRadarMemory(
        RADAR_PIXEL_COUNT, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  }
  return preparedFrames[index] != nullptr;
}

// Source files are optional: prepared animation frames are never evicted.
// Bound SK source retention independently of the server's worst-case file size.
bool reserveSourceCache(size_t extra) {
  if (!slovakSource()) return true;
  constexpr size_t sourceBudget = 1024 * 1024;
  constexpr size_t freeReserve = 768 * 1024;
  auto fits = [&]() {
    size_t used = 0;
    for (size_t n : cachedPngCapacities) used += n;
    for (size_t n : pendingPngCapacities) used += n;
    return used + extra <= sourceBudget &&
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM) >= extra + freeReserve;
  };
  // Oldest sources first; names/readiness continue to identify prepared frames.
  for (size_t i = 0; !fits() && i < MAX_ANIMATION_FRAME_COUNT; ++i) {
    portENTER_CRITICAL(&stateMux);
    uint8_t *old = cachedPngFrames[i];
    cachedPngFrames[i] = nullptr;
    cachedPngSizes[i] = cachedPngCapacities[i] = 0;
    portEXIT_CRITICAL(&stateMux);
    heap_caps_free(old);
  }
  return fits();
}

bool cacheDownloadedPng(size_t index, size_t size, const char *fileName) {
  if (index >= MAX_ANIMATION_FRAME_COUNT || size == 0 || size > PNG_CAPACITY)
    return false;
  if (!reserveSourceCache(0)) return false;
  if (cachedPngCapacities[index] < size) {
    if (!reserveSourceCache(size)) return false;
    uint8_t *replacement = static_cast<uint8_t *>(allocateRadarMemory(
        size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (replacement == nullptr) return false;
    if (cachedPngFrames[index] != nullptr) heap_caps_free(cachedPngFrames[index]);
    cachedPngFrames[index] = replacement;
    cachedPngCapacities[index] = size;
  }
  memcpy(cachedPngFrames[index], pngBuffer, size);
  cachedPngSizes[index] = size;
  strlcpy(cachedPngNames[index], fileName, FILE_NAME_CAPACITY);
  return true;
}

void insertLatestName(char names[][FILE_NAME_CAPACITY], size_t &count,
                      const char *candidate) {
  for (size_t index = 0; index < count; ++index) {
    if (strcmp(names[index], candidate) == 0) return;
  }
  if (count < MAX_ANIMATION_FRAME_COUNT) {
    size_t position = count++;
    while (position > 0 && strcmp(names[position - 1], candidate) > 0) {
      strlcpy(names[position], names[position - 1], FILE_NAME_CAPACITY);
      --position;
    }
    strlcpy(names[position], candidate, FILE_NAME_CAPACITY);
    return;
  }
  if (strcmp(candidate, names[0]) <= 0) return;
  size_t position = 0;
  while (position + 1 < MAX_ANIMATION_FRAME_COUNT &&
         strcmp(names[position + 1], candidate) < 0) {
    strlcpy(names[position], names[position + 1], FILE_NAME_CAPACITY);
    ++position;
  }
  strlcpy(names[position], candidate, FILE_NAME_CAPACITY);
}

void parseIndexChunk(const uint8_t *data, size_t length, size_t &prefixMatch,
                     char *candidate, size_t &candidateLength,
                     char latestNames[][FILE_NAME_CAPACITY], size_t &count) {
  constexpr size_t prefixLength = sizeof(FILE_PREFIX) - 1;
  for (size_t index = 0; index < length; ++index) {
    const char value = static_cast<char>(data[index]);
    if (candidateLength > 0) {
      if (candidateLength + 1 >= FILE_NAME_CAPACITY || value == '<' ||
          value == '"' || value == '\'' || value == ' ') {
        candidateLength = 0;
        prefixMatch = 0;
        continue;
      }
      candidate[candidateLength++] = value;
      candidate[candidateLength] = '\0';
      if (candidateLength >= 4 &&
          strcmp(candidate + candidateLength - 4, ".png") == 0) {
        insertLatestName(latestNames, count, candidate);
        candidateLength = 0;
        prefixMatch = 0;
      }
      continue;
    }

    if (value == FILE_PREFIX[prefixMatch]) {
      ++prefixMatch;
      if (prefixMatch == prefixLength) {
        memcpy(candidate, FILE_PREFIX, prefixLength);
        candidateLength = prefixLength;
        candidate[candidateLength] = '\0';
        prefixMatch = 0;
      }
    } else {
      prefixMatch = value == FILE_PREFIX[0] ? 1 : 0;
    }
  }
}

// One worker-owned HTTP/1.1 session per complete refresh. The network guard
// spans the batch so another TLS operation cannot exhaust internal memory.
uint32_t lastBatchMs = 0, lastBatchConnections = 0, lastBatchRequests = 0;
uint32_t lastBatchBytes = 0;
class RadarTlsClient : public WiFiClientSecure {
 public:
  uint32_t connections = 0;
  int connect(const char *host, uint16_t port, int32_t timeout) override {
    ++connections;
    return WiFiClientSecure::connect(host, port, timeout);
  }
};
struct RadarHttpBatch {
  NetworkOperationGuard guard{15000};
  RadarTlsClient client;
  HTTPClient http;
  uint32_t started = millis(), requests = 0, bytes = 0;
  RadarHttpBatch() {
    client.setCACert(slovakSource() ? FIRMWARE_RELEASE_ROOT_CA : CHMI_ROOT_CA);
    const char *headers[] = {"Transfer-Encoding"};
    http.collectHeaders(headers, 1);
    http.useHTTP10(false);
    http.setReuse(true);
    http.setConnectTimeout(6000);
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  }
  ~RadarHttpBatch() {
    http.setReuse(false);
    http.end();
    client.stop();
    portENTER_CRITICAL(&stateMux);
    lastBatchMs = millis() - started;
    lastBatchConnections = client.connections;
    lastBatchRequests = requests;
    lastBatchBytes = bytes;
    portEXIT_CRITICAL(&stateMux);
  }
  bool begin(const String &url) {
    if (!guard || !http.begin(client, url)) return false;
    if (slovakSource()) addRadarClientHeaders(http);
    http.addHeader(F("Cache-Control"), F("no-cache"));
    ++requests;
    const int status = http.GET();
    portENTER_CRITICAL(&stateMux);
    lastHttpStatus = status;
    portEXIT_CRITICAL(&stateMux);
    if (status == HTTP_CODE_OK) return true;
    client.stop();  // Never reuse an unread error body.
    http.end();
    return false;
  }
};
RadarHttpBatch *httpBatch = nullptr;

// Body parsing is cooperative even while waiting between CHMI HTTP chunks.
// A partial/cancelled body always closes TLS.
class RadarBodySink : public Stream {
 public:
  using Consumer = bool (*)(const uint8_t *, size_t, void *);
  Consumer consumer;
  void *context;
  size_t limit, received = 0;
  uint32_t revision, yieldedAt = millis();
  bool failed = false;
  RadarBodySink(Consumer c, void *ctx, size_t maxBytes, uint32_t rev)
      : consumer(c), context(ctx), limit(maxBytes), revision(rev) {}
  size_t write(uint8_t value) override { return write(&value, 1); }
  size_t write(const uint8_t *data, size_t size) override {
    if (failed || !requestMatches(revision) || size > limit - received ||
        !consumer(data, size, context)) { failed = true; return 0; }
    received += size;
    advanceAnimation(millis());
    if (millis() - yieldedAt >= 10) { delay(1); yieldedAt = millis(); }
    return size;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
};
bool readRadarBody(RadarBodySink &sink) {
  const int declared = httpBatch->http.getSize();
  if (declared > static_cast<int>(sink.limit)) {
    httpBatch->client.stop(); httpBatch->http.end(); return false;
  }
  String encoding = httpBatch->http.header("Transfer-Encoding");
  encoding.trim();
  encoding.toLowerCase();
  const uint32_t started = millis();
  uint32_t lastData = started;
  auto readSome = [&](uint8_t *out, size_t wanted) -> int {
    while (requestMatches(sink.revision)) {
      const uint32_t now = millis();
      if (now - lastData >= 15000 || now - started >= 45000) return -1;
      const int available = httpBatch->client.available();
      if (available > 0) {
        const size_t amount = min(wanted, static_cast<size_t>(available));
        const int n = httpBatch->client.read(out, amount);
        if (n > 0) { lastData = millis(); return n; }
      } else if (!httpBatch->client.connected()) {
        return 0;
      }
      advanceAnimation(now);
      delay(1);  // No busy wait: leave the UI, TCP/IP and watchdog time to run.
    }
    return -1;
  };
  const bool framingSupported = encoding.isEmpty() || encoding == "chunked";
  const bool valid = framingSupported &&
      radarHttp::readBody(readSome, sink, declared, encoding == "chunked") &&
      !sink.failed && requestMatches(sink.revision);
  httpBatch->bytes += sink.received;
  if (!valid) httpBatch->client.stop();
  httpBatch->http.end();  // Keep-alive only after a complete response.
  portENTER_CRITICAL(&stateMux);
  lastDownloadedBytes = sink.received;
  portEXIT_CRITICAL(&stateMux);
  return valid;
}
struct RadarBuffer { uint8_t *data; size_t used = 0; };
bool appendRadarBuffer(const uint8_t *data, size_t size, void *context) {
  auto &buffer = *static_cast<RadarBuffer *>(context);
  memcpy(buffer.data + buffer.used, data, size);
  buffer.used += size;
  return true;
}

bool latestSlovakNames(char output[][FILE_NAME_CAPACITY], size_t &count,
                       uint32_t revision) {
  count = 0;
  if (!httpBatch || !httpBatch->begin(slovakRadar::MANIFEST_URL)) return false;
  constexpr size_t maxManifestBytes = 16384;
  char *body = static_cast<char *>(allocateRadarMemory(maxManifestBytes + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!body) { httpBatch->client.stop(); httpBatch->http.end(); return false; }
  RadarBuffer buffer{reinterpret_cast<uint8_t *>(body)};
  RadarBodySink sink(appendRadarBuffer, &buffer, maxManifestBytes, revision);
  const bool complete = readRadarBody(sink);
  const size_t received = buffer.used;
  body[received] = '\0';
  bool valid = complete && requestMatches(revision) &&
      slovakRadar::parse(body, time(nullptr), slovakFrames, slovakFrameCount);
  heap_caps_free(body);
  if (!valid) return false;
  count = slovakFrameCount;
  for (size_t i = 0; i < count; ++i) slovakCacheName(slovakFrames[i], output[i]);
  portENTER_CRITICAL(&stateMux);
  lastDownloadedBytes = received;
  strlcpy(latestIndexFile, output[count - 1], sizeof(latestIndexFile));
  portEXIT_CRITICAL(&stateMux);
  return true;
}

bool latestFileNames(char output[][FILE_NAME_CAPACITY], size_t &count,
                     uint32_t revision) {
  if (slovakSource()) return latestSlovakNames(output, count, revision);
  count = 0;
  memset(output, 0,
         MAX_ANIMATION_FRAME_COUNT * FILE_NAME_CAPACITY * sizeof(char));
  if (!httpBatch || !httpBatch->begin(String(radarIndexUrl()) + F("?clock=") + millis())) return false;
  struct IndexState {
    size_t prefixMatch = 0, candidateLength = 0;
    char candidate[FILE_NAME_CAPACITY] = {};
    char (*names)[FILE_NAME_CAPACITY];
    size_t *count;
  } state{};
  state.names = output;
  state.count = &count;
  RadarBodySink sink([](const uint8_t *data, size_t size, void *context) {
    auto &state = *static_cast<IndexState *>(context);
    parseIndexChunk(data, size, state.prefixMatch, state.candidate,
                   state.candidateLength, state.names, *state.count);
    return true;
  }, &state, 2 * 1024 * 1024, revision);
  if (!readRadarBody(sink)) return false;
  const size_t indexBytesRead = sink.received;
  portENTER_CRITICAL(&stateMux);
  lastDownloadedBytes = indexBytesRead;
  if (count > 0)
    strlcpy(latestIndexFile, output[count - 1], sizeof(latestIndexFile));
  portEXIT_CRITICAL(&stateMux);
  return count > 0;
}

bool downloadSinglePng(const char *fileName, size_t &outputSize,
                 uint32_t revision, size_t offset = 0) {
  outputSize = 0;
  const String url = String(slovakSource() ? slovakRadar::ORIGIN : radarIndexUrl()) + fileName;
  portENTER_CRITICAL(&stateMux);
  strlcpy(currentFile, fileName, sizeof(currentFile));
  lastDownloadedBytes = 0;
  portEXIT_CRITICAL(&stateMux);
  if (!httpBatch || !httpBatch->begin(url)) return false;
  const size_t limit = slovakSource() ? slovakRadar::FRAME_LIMIT : 131072;
  RadarBuffer buffer{pngBuffer + offset};
  RadarBodySink sink(appendRadarBuffer, &buffer, limit, revision);
  const bool complete = readRadarBody(sink);
  outputSize = buffer.used;
  if (!complete) return false;
  if (slovakSource()) return outputSize >= 2236 && memcmp(pngBuffer, "NRD2", 4) == 0;
  static const uint8_t signature[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a,
                                      '\n'};
  return outputSize >= sizeof(signature) &&
         memcmp(pngBuffer + offset, signature, sizeof(signature)) == 0;
}

bool verifySlovakImage(const slovakRadar::Image &image, size_t size, size_t offset) {
  if (size != image.bytes) return false;
  uint8_t digest[32];
  if (mbedtls_sha256(pngBuffer + offset, size, digest, 0) != 0) return false;
  char hex[65];
  for (size_t i = 0; i < 32; ++i) snprintf(hex + i * 2, 3, "%02x", digest[i]);
  return strcmp(hex, image.sha256) == 0;
}

bool downloadPng(const char *fileName, size_t &outputSize, uint32_t revision) {
  if (!slovakSource()) return downloadSinglePng(fileName, outputSize, revision);
  const auto *frame = slovakFrame(fileName);
  if (!frame || !downloadSinglePng(frame->image.path, outputSize, revision) ||
      !verifySlovakImage(frame->image, outputSize, 0)) return false;
  nrd2::Frame packed;
  if (!packed.open(pngBuffer, outputSize) || packed.measured != frame->time) return false;
  // Validate all rows before admitting the complete source frame into the cache.
  for (unsigned y = 0; y < 550; ++y) {
    if (!packed.validateRow(y) || !requestMatches(revision)) return false;
    if ((y & 31) == 0) { advanceAnimation(millis()); delay(1); }
  }
  return true;
}

bool downloadPngWithRetry(const char *fileName, size_t &outputSize,
                          uint32_t revision) {
  constexpr uint8_t maxAttempts = 5;
  for (uint8_t attempt = 0; attempt < maxAttempts; ++attempt) {
    if (downloadPng(fileName, outputSize, revision)) return true;
    if (!requestMatches(revision)) return false;
    if (attempt + 1 < maxAttempts) delay(500UL * (attempt + 1));
  }
  return false;
}

void drawDecodedLine(PNGDRAW *draw) {
  if (decodeTarget == nullptr || lineBuffer == nullptr) return;
  if (draw->y != decodedLineCount) decodedLinesSequential = false;
  ++decodedLineCount;
  pngDecoder->getLineAsRGB565(draw, lineBuffer, PNG_RGB565_LITTLE_ENDIAN,
                              0x00000000);
  for (int targetY = 0; targetY < CHMI_RADAR_HEIGHT; ++targetY) {
    if (sourceY[targetY] != draw->y) continue;
    uint8_t *row = decodeTarget + targetY * CHMI_RADAR_WIDTH;
    const long dy = targetY - CHMI_RADAR_HEIGHT / 2;
    for (int targetX = 0; targetX < CHMI_RADAR_WIDTH; ++targetX) {
      const long dx = targetX - CHMI_RADAR_WIDTH / 2;
      if (dx * dx + dy * dy > 238L * 238L) continue;
      const int source = sourceX[targetX];
      if (source >= 0 && source < imageWidth && source <= dataX1 &&
          draw->y >= dataY0) {
        row[targetX] = encodePreparedColor(lineBuffer[source]);
      }
    }
    if ((targetY & 7) == 0) {
      advanceAnimation(millis());
      delay(1);
    }
  }
}

uint16_t blendRgb565(uint16_t background, uint16_t foreground,
                     uint8_t opacity) {
  if (opacity == 0) return background;
  if (opacity >= 100) return foreground;
  const uint16_t inverse = 100 - opacity;
  const uint16_t red =
      (((background >> 11) & 0x1f) * inverse +
       ((foreground >> 11) & 0x1f) * opacity + 50) /
      100;
  const uint16_t green =
      (((background >> 5) & 0x3f) * inverse +
       ((foreground >> 5) & 0x3f) * opacity + 50) /
      100;
  const uint16_t blue =
      ((background & 0x1f) * inverse + (foreground & 0x1f) * opacity + 50) /
      100;
  return static_cast<uint16_t>((red << 11) | (green << 5) | blue);
}

void blendMapPixel(uint16_t &pixel, uint16_t color, uint8_t opacity) {
  pixel = blendRgb565(pixel, color, opacity);
}

void blendMapPixel(uint8_t &pixel, uint16_t color, uint8_t opacity) {
  // SHMU uses indexed RGB565 colors; Czech frames retain RGB332.
  pixel = encodePreparedColor(blendRgb565(decodePreparedColor(pixel), color, opacity));
}

template <typename Pixel>
void setMapPixel(Pixel *buffer, int x, int y, uint16_t color,
                 uint8_t opacity) {
  if (x >= 0 && x < CHMI_RADAR_WIDTH && y >= 0 && y < CHMI_RADAR_HEIGHT) {
    blendMapPixel(buffer[y * CHMI_RADAR_WIDTH + x], color, opacity);
  }
}

uint8_t lineOutCode(int x, int y) {
  return (x < 0 ? 1 : 0) | (x >= CHMI_RADAR_WIDTH ? 2 : 0) |
         (y < 0 ? 4 : 0) | (y >= CHMI_RADAR_HEIGHT ? 8 : 0);
}

template <typename Pixel>
void drawMapLine(Pixel *buffer, int x0, int y0, int x1, int y1,
                 uint16_t color, uint8_t opacity) {
  uint8_t code0 = lineOutCode(x0, y0);
  uint8_t code1 = lineOutCode(x1, y1);
  while (code0 || code1) {
    if (code0 & code1) return;
    const uint8_t code = code0 ? code0 : code1;
    int x = 0;
    int y = 0;
    if (code & 8) {
      y = CHMI_RADAR_HEIGHT - 1;
      x = x0 + static_cast<int64_t>(x1 - x0) * (y - y0) / (y1 - y0);
    } else if (code & 4) {
      y = 0;
      x = x0 + static_cast<int64_t>(x1 - x0) * (y - y0) / (y1 - y0);
    } else if (code & 2) {
      x = CHMI_RADAR_WIDTH - 1;
      y = y0 + static_cast<int64_t>(y1 - y0) * (x - x0) / (x1 - x0);
    } else {
      x = 0;
      y = y0 + static_cast<int64_t>(y1 - y0) * (x - x0) / (x1 - x0);
    }
    if (code == code0) {
      x0 = x;
      y0 = y;
      code0 = lineOutCode(x0, y0);
    } else {
      x1 = x;
      y1 = y;
      code1 = lineOutCode(x1, y1);
    }
  }
  const int deltaX = abs(x1 - x0);
  const int stepX = x0 < x1 ? 1 : -1;
  const int deltaY = -abs(y1 - y0);
  const int stepY = y0 < y1 ? 1 : -1;
  int error = deltaX + deltaY;
  for (;;) {
    setMapPixel(buffer, x0, y0, color, opacity);
    if (x0 == x1 && y0 == y1) break;
    const int doubled = 2 * error;
    if (doubled >= deltaY) {
      error += deltaY;
      x0 += stepX;
    }
    if (doubled <= deltaX) {
      error += deltaX;
      y0 += stepY;
    }
  }
}

const uint8_t *mapGlyph(char character) {
  static const uint8_t glyphs[26][5] = {
      {0x7e, 0x11, 0x11, 0x11, 0x7e}, {0x7f, 0x49, 0x49, 0x49, 0x36},
      {0x3e, 0x41, 0x41, 0x41, 0x22}, {0x7f, 0x41, 0x41, 0x22, 0x1c},
      {0x7f, 0x49, 0x49, 0x49, 0x41}, {0x7f, 0x09, 0x09, 0x09, 0x01},
      {0x3e, 0x41, 0x49, 0x49, 0x7a}, {0x7f, 0x08, 0x08, 0x08, 0x7f},
      {0x00, 0x41, 0x7f, 0x41, 0x00}, {0x20, 0x40, 0x41, 0x3f, 0x01},
      {0x7f, 0x08, 0x14, 0x22, 0x41}, {0x7f, 0x40, 0x40, 0x40, 0x40},
      {0x7f, 0x02, 0x0c, 0x02, 0x7f}, {0x7f, 0x04, 0x08, 0x10, 0x7f},
      {0x3e, 0x41, 0x41, 0x41, 0x3e}, {0x7f, 0x09, 0x09, 0x09, 0x06},
      {0x3e, 0x41, 0x51, 0x21, 0x5e}, {0x7f, 0x09, 0x19, 0x29, 0x46},
      {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7f, 0x01, 0x01},
      {0x3f, 0x40, 0x40, 0x40, 0x3f}, {0x1f, 0x20, 0x40, 0x20, 0x1f},
      {0x3f, 0x40, 0x38, 0x40, 0x3f}, {0x63, 0x14, 0x08, 0x14, 0x63},
      {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43},
  };
  return character >= 'A' && character <= 'Z' ? glyphs[character - 'A']
                                                : nullptr;
}

template <typename Pixel>
void fillMapRect(Pixel *buffer, int x, int y, int width, int height,
                 uint16_t color, uint8_t opacity) {
  for (int row = y; row < y + height; ++row)
    for (int column = x; column < x + width; ++column)
      setMapPixel(buffer, column, row, color, opacity);
}

template <typename Pixel>
void drawMapText(Pixel *buffer, int x, int y, const char *text,
                 uint16_t color, uint8_t opacity) {
  for (size_t index = 0; text[index] != '\0'; ++index) {
    const char character = static_cast<char>(
        toupper(static_cast<unsigned char>(text[index])));
    const uint8_t *glyph = mapGlyph(character);
    if (glyph != nullptr) {
      for (int column = 0; column < 5; ++column)
        for (int row = 0; row < 7; ++row)
          if (glyph[column] & (1U << row))
            setMapPixel(buffer, x + index * 6 + column, y + row, color,
                        opacity);
    } else if (character == '-') {
      for (int column = 1; column < 5; ++column)
        setMapPixel(buffer, x + index * 6 + column, y + 3, color, opacity);
    } else if (character == '.') {
      setMapPixel(buffer, x + index * 6 + 2, y + 6, color, opacity);
    }
  }
}

void projectRadarPoint(float latitude, float longitude, int cropX1, int cropX2,
                       int cropY1, int cropY2, int &x, int &y) {
  if (slovakSource()) {
    x = slovakView.mapX(longitude);
    y = slovakView.mapY(latitude);
    return;
  }
  x = static_cast<int64_t>(longitudeToX(longitude) - cropX1) *
      CHMI_RADAR_WIDTH / (cropX2 - cropX1 + 1);
  y = static_cast<int64_t>(latitudeToY(latitude) - cropY1) *
      CHMI_RADAR_HEIGHT / (cropY2 - cropY1 + 1);
}

struct MapLabelBox {
  int x;
  int y;
  int width;
  int height;
};

bool mapBoxesOverlap(const MapLabelBox &left, const MapLabelBox &right) {
  return left.x < right.x + right.width && left.x + left.width > right.x &&
         left.y < right.y + right.height && left.y + left.height > right.y;
}

template <typename Pixel>
void drawMapOverlay(Pixel *buffer, float markerLatitude,
                    float markerLongitude, uint16_t radiusKm, int cropX1,
                    int cropX2, int cropY1, int cropY2, uint8_t opacity) {
  if (opacity == 0) return;
  constexpr uint16_t borderColor = 0xbdf7;
  constexpr uint16_t cityColor = 0x07ff;
  const CzechMapPoint *border = slovakSource() ? SLOVAK_MAP_BORDER : CZECH_MAP_BORDER;
  const size_t borderCount = slovakSource() ? sizeof(SLOVAK_MAP_BORDER) / sizeof(SLOVAK_MAP_BORDER[0])
      : sizeof(CZECH_MAP_BORDER) / sizeof(CZECH_MAP_BORDER[0]);
  const CzechMapCity *cities = slovakSource() ? SLOVAK_MAP_CITIES : CZECH_MAP_CITIES;
  const size_t cityCount = slovakSource() ? sizeof(SLOVAK_MAP_CITIES) / sizeof(SLOVAK_MAP_CITIES[0])
      : sizeof(CZECH_MAP_CITIES) / sizeof(CZECH_MAP_CITIES[0]);
  int previousX = 0;
  int previousY = 0;
  for (size_t index = 0;
       index < borderCount;
       ++index) {
    const float longitude = CZECH_MAP_LON_ORIGIN +
                            border[index].longitude *
                                CZECH_MAP_COORD_SCALE;
    const float latitude = CZECH_MAP_LAT_ORIGIN +
                           border[index].latitude *
                               CZECH_MAP_COORD_SCALE;
    int x = 0;
    int y = 0;
    projectRadarPoint(latitude, longitude, cropX1, cropX2, cropY1, cropY2, x,
                      y);
    if (index > 0)
      drawMapLine(buffer, previousX, previousY, x, y, borderColor, opacity);
    previousX = x;
    previousY = y;
  }

  MapLabelBox occupied[sizeof(CZECH_MAP_CITIES) / sizeof(CZECH_MAP_CITIES[0]) +
                       sizeof(SLOVAK_MAP_CITIES) / sizeof(SLOVAK_MAP_CITIES[0])] = {};
  size_t occupiedCount = 0;
  const bool showFullNames = radiusKm > 0 && radiusKm <= 50;
  for (uint8_t tier = 1; tier <= 2; ++tier) {
    for (size_t cityIndex = 0; cityIndex < cityCount; ++cityIndex) {
      const CzechMapCity &city = cities[cityIndex];
      if (city.tier != tier) continue;
      int x = 0;
      int y = 0;
      projectRadarPoint(city.latitude, city.longitude, cropX1, cropX2, cropY1,
                        cropY2, x, y);
      const int deltaX = x - CHMI_RADAR_WIDTH / 2;
      const int deltaY = y - CHMI_RADAR_HEIGHT / 2;
      if (deltaX * deltaX + deltaY * deltaY > 225 * 225) continue;

      const char *label = showFullNames ? city.name : city.label;
      const int textWidth = strlen(label) * 6 - 1;
      MapLabelBox box = {x + 6, y - 5, textWidth + 4, 11};
      if (box.x + box.width >= CHMI_RADAR_WIDTH)
        box.x = x - 6 - box.width;
      if (box.x < 0 || box.y < 54 || box.y + box.height >= CHMI_RADAR_HEIGHT)
        continue;
      bool overlaps = false;
      for (size_t index = 0; index < occupiedCount; ++index)
        if (mapBoxesOverlap(box, occupied[index])) {
          overlaps = true;
          break;
        }
      if (overlaps) continue;

      for (int offsetY = -2; offsetY <= 2; ++offsetY)
        for (int offsetX = -2; offsetX <= 2; ++offsetX)
          if (offsetX * offsetX + offsetY * offsetY <= 4)
            setMapPixel(buffer, x + offsetX, y + offsetY, 0xffff, opacity);
      fillMapRect(buffer, box.x, box.y, box.width, box.height, 0x0000,
                  opacity);
      drawMapText(buffer, box.x + 2, box.y + 2, label, cityColor, opacity);
      occupied[occupiedCount++] = box;
    }
  }

  int markerX = 0;
  int markerY = 0;
  projectRadarPoint(markerLatitude, markerLongitude, cropX1, cropX2, cropY1,
                    cropY2, markerX, markerY);
  constexpr uint16_t white = 0xffff;
  const int markerDeltaX = markerX - CHMI_RADAR_WIDTH / 2;
  const int markerDeltaY = markerY - CHMI_RADAR_HEIGHT / 2;
  if (markerDeltaX * markerDeltaX + markerDeltaY * markerDeltaY < 225 * 225) {
    for (int offset = -9; offset <= 9; ++offset) {
      setMapPixel(buffer, markerX + offset, markerY, white, opacity);
      setMapPixel(buffer, markerX, markerY + offset, white, opacity);
    }
  }
}

template <typename Pixel>
void drawDisplayRing(Pixel *buffer) {
  const int centerX = CHMI_RADAR_WIDTH / 2;
  const int centerY = CHMI_RADAR_HEIGHT / 2;
  constexpr uint16_t gray = 0x4208;
  for (int degree = 0; degree < 360; ++degree) {
    const float angle = degree * 0.017453292519943295f;
    const int x = centerX + lroundf(cosf(angle) * 238.0f);
    const int y = centerY + lroundf(sinf(angle) * 238.0f);
    if (x >= 0 && x < CHMI_RADAR_WIDTH && y >= 0 && y < CHMI_RADAR_HEIGHT)
      setMapPixel(buffer, x, y, gray, 100);
  }
}

void radarProjectionBounds(float latitude, float longitude, uint16_t radiusKm,
                           int &cropX1, int &cropX2, int &cropY1,
                           int &cropY2) {
  const uint16_t projectionRadiusKm =
      radiusKm == 0 ? WHOLE_COUNTRY_RADIUS_KM : radiusKm;
  if (radiusKm == 0) {
    latitude = slovakSource() ? 48.70f : WHOLE_COUNTRY_LATITUDE;
    longitude = slovakSource() ? 19.65f : WHOLE_COUNTRY_LONGITUDE;
  }
  if (slovakSource()) slovakView.set(latitude, longitude, projectionRadiusKm);
  const float latitudeSpan = projectionRadiusKm / 111.32f;
  const float longitudeSpan =
      projectionRadiusKm /
      (111.32f * cosf(latitude * 0.017453292519943295f));
  cropX1 = longitudeToX(longitude - longitudeSpan);
  cropX2 = longitudeToX(longitude + longitudeSpan);
  cropY1 = latitudeToY(latitude + latitudeSpan);
  cropY2 = latitudeToY(latitude - latitudeSpan);
}

bool showBaseMap(float latitude, float longitude, uint16_t radiusKm,
                 uint8_t mapOpacityValue, uint32_t revision) {
  if (!ensureBuffers()) return false;
  imageWidth = slovakSource() ? 800 : RADAR_SOURCE_WIDTH;
  imageHeight = slovakSource() ? 550 : RADAR_SOURCE_HEIGHT;
  int cropX1 = 0;
  int cropX2 = 0;
  int cropY1 = 0;
  int cropY2 = 0;
  radarProjectionBounds(latitude, longitude, radiusKm, cropX1, cropX2, cropY1,
                        cropY2);
  const uint8_t targetBuffer = 1 - activeDisplayBuffer;
  uint16_t *target = displayBuffers[targetBuffer];
  memset(target, 0, RADAR_PIXEL_COUNT * sizeof(uint16_t));
  drawMapOverlay(target, latitude, longitude, radiusKm, cropX1, cropX2, cropY1,
                 cropY2, mapOpacityValue);
  drawDisplayRing(target);
  portENTER_CRITICAL(&stateMux);
  const bool nightVisual = redNightMode;
  portEXIT_CRITICAL(&stateMux);
  if (nightVisual) applyNightRadarPalette(target);
  portENTER_CRITICAL(&stateMux);
  if (!active || revision != requestRevision) {
    portEXIT_CRITICAL(&stateMux);
    return false;
  }
  activeDisplayBuffer = targetBuffer;
  activeRadiusKm = radiusKm;
  displayedFrame = -2;
  ready = false;
  frameTime[0] = '\0';
  ++generation;
  portEXIT_CRITICAL(&stateMux);
  return true;
}

bool decodeSlovakRadar(const uint8_t *data, size_t size, float latitude,
                       float longitude, uint16_t radiusKm, uint8_t opacity,
                       uint8_t *target) {
  nrd2::Frame packed;
  if (!target || !packed.open(data, size)) return false;
  imageWidth = 800;
  imageHeight = 550;
  int x1, x2, y1, y2;
  radarProjectionBounds(latitude, longitude, radiusKm, x1, x2, y1, y2);
  memset(target, 0, RADAR_PIXEL_COUNT);
  uint8_t sourceRow[800];
  int previousY = -1;
  for (int x = 0; x < CHMI_RADAR_WIDTH; ++x) sourceX[x] = slovakView.sourceX(x);
  for (int y = 0; y < CHMI_RADAR_HEIGHT; ++y) {
    const int sy = slovakView.sourceY(y);
    if (sy < 0 || sy >= 550) continue;
    if (sy != previousY) {
      if (!packed.row(sy, sourceRow)) return false;
      previousY = sy;
    }
    for (int x = 0; x < CHMI_RADAR_WIDTH; ++x) {
      const long dx = x - CHMI_RADAR_WIDTH / 2, dy = y - CHMI_RADAR_HEIGHT / 2;
      if (dx * dx + dy * dy > 238L * 238L || sourceX[x] < 0 || sourceX[x] >= 800) continue;
      const uint8_t value = sourceRow[sourceX[x]];
      target[y * CHMI_RADAR_WIDTH + x] = value == 255
          ? ((x + y) % 12 < 2 ? slovakRadarRender::encode(0x4208) : 0) : value;
    }
    if ((y & 31) == 0) { advanceAnimation(millis()); delay(1); }
  }
  portENTER_CRITICAL(&stateMux);
  lastDecodeResult = PNG_SUCCESS;
  lastDecodedLineCount = 550;
  acceptedCompleteDecodeError = false;
  portEXIT_CRITICAL(&stateMux);
  drawMapOverlay(target, latitude, longitude, radiusKm, x1, x2, y1, y2, opacity);
  drawDisplayRing(target);
  return true;
}

bool decodeRadar(const uint8_t *pngData, size_t pngSize, float latitude,
                 float longitude, uint16_t radiusKm, uint8_t mapOpacityValue,
                 uint8_t *target) {
  if (slovakSource()) return decodeSlovakRadar(pngData, pngSize, latitude,
      longitude, radiusKm, mapOpacityValue, target);
  if (pngData == nullptr || !ensurePngDecoder() ||
      pngDecoder->openRAM(const_cast<uint8_t *>(pngData),
                          static_cast<int>(pngSize), drawDecodedLine) !=
      PNG_SUCCESS)
    return false;
  imageWidth = pngDecoder->getWidth();
  imageHeight = pngDecoder->getHeight();
  if (imageWidth <= 0 || imageHeight <= 0 || imageWidth > 2048 ||
      (slovakSource() && (imageWidth != 800 || imageHeight != 550))) {
    pngDecoder->close();
    return false;
  }
  if (lineCapacity < static_cast<size_t>(imageWidth)) {
    if (lineBuffer != nullptr) heap_caps_free(lineBuffer);
    lineBuffer = static_cast<uint16_t *>(allocateRadarMemory(
        imageWidth * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    lineCapacity = lineBuffer == nullptr ? 0 : imageWidth;
  }
  if (lineBuffer == nullptr) {
    pngDecoder->close();
    return false;
  }

  const float markerLatitude = latitude;
  const float markerLongitude = longitude;
  int cropX1 = 0;
  int cropX2 = 0;
  int cropY1 = 0;
  int cropY2 = 0;
  radarProjectionBounds(latitude, longitude, radiusKm, cropX1, cropX2, cropY1,
                        cropY2);
  for (int x = 0; x < CHMI_RADAR_WIDTH; ++x) {
    sourceX[x] = slovakSource() ? slovakView.sourceX(x) : cropX1 + static_cast<int64_t>(x) * (cropX2 - cropX1 + 1) /
                              CHMI_RADAR_WIDTH;
  }
  for (int y = 0; y < CHMI_RADAR_HEIGHT; ++y) {
    sourceY[y] = slovakSource() ? slovakView.sourceY(y) : cropY1 + static_cast<int64_t>(y) * (cropY2 - cropY1 + 1) /
                              CHMI_RADAR_HEIGHT;
  }
  dataX1 = slovakSource() ? imageWidth - 1 : longitudeToX(LON_DATA_RIGHT);
  dataY0 = slovakSource() ? 0 : latitudeToY(LAT_DATA_TOP);
  memset(target, 0, RADAR_PIXEL_COUNT);
  // Pixels outside the source image remain black, including the whole-country view.
  decodedLineCount = 0;
  decodedLinesSequential = true;
  decodeTarget = target;
  const int result = pngDecoder->decode(nullptr, 0);
  decodeTarget = nullptr;
  pngDecoder->close();
  const bool completeImage =
      decodedLinesSequential && decodedLineCount == imageHeight;
  portENTER_CRITICAL(&stateMux);
  lastDecodeResult = result;
  lastDecodedLineCount = decodedLineCount;
  acceptedCompleteDecodeError = result != PNG_SUCCESS && completeImage;
  portEXIT_CRITICAL(&stateMux);
  // Některé validní PNG soubory ČHMÚ vrátí v PNGdec chybu až po předání
  // posledního řádku. Přijmeme je pouze tehdy, když dekodér postupně předal
  // přesně celý neprokládaný obraz; částečný nebo neuspořádaný výstup dál
  // odmítáme.
  if (result != PNG_SUCCESS && !completeImage) return false;
  drawMapOverlay(target, markerLatitude, markerLongitude, radiusKm, cropX1,
                 cropX2, cropY1, cropY2, mapOpacityValue);
  drawDisplayRing(target);
  return true;
}

void frameTimeFromName(const char *fileName, char *output) {
  if (slovakSource()) {
    // Timestamp stays available even when a cached PNG is reprojected after manifest rotation.
    struct tm utc = {};
    int year, month, day, hour, minute, second;
    if (sscanf(fileName, "/v2/sk/frames/%4d%2d%2d%2d%2d%2d-", &year, &month, &day, &hour, &minute, &second) != 6) {
      output[0] = '\0'; return;
    }
    const time_t epoch = static_cast<time_t>(daysFromCivil(year, month, day)) * 86400L + hour * 3600 + minute * 60 + second;
    clockLocaltime(&epoch, &utc);
    snprintf(output, 6, "%02d:%02d", utc.tm_hour, utc.tm_min);
    return;
  }
  const char *timestamp = strstr(fileName, FILE_PREFIX);
  if (timestamp == nullptr) {
    output[0] = '\0';
    return;
  }
  timestamp += strlen(FILE_PREFIX);
  if (strlen(timestamp) < 13 || timestamp[8] != '.') {
    output[0] = '\0';
    return;
  }
  char number[5] = {};
  memcpy(number, timestamp, 4);
  const int year = atoi(number);
  memcpy(number, timestamp + 4, 2);
  number[2] = '\0';
  const int month = atoi(number);
  memcpy(number, timestamp + 6, 2);
  const int day = atoi(number);
  memcpy(number, timestamp + 9, 2);
  const int hour = atoi(number);
  memcpy(number, timestamp + 11, 2);
  const int minute = atoi(number);
  const time_t epoch =
      static_cast<time_t>(daysFromCivil(year, month, day)) * 86400L +
      hour * 3600L + minute * 60L;
  struct tm localTime = {};
  clockLocaltime(&epoch, &localTime);
  snprintf(output, 6, "%02d:%02d", localTime.tm_hour, localTime.tm_min);
}

bool currentRequest(float &latitude, float &longitude, uint16_t &radiusKm,
                    uint8_t &wantedFrameCount, uint8_t &mapOpacityValue,
                    uint32_t &revision) {
  portENTER_CRITICAL(&stateMux);
  const bool requested = active;
  if (requested && workerSource != requestedSource) {
    // Only the worker invalidates cache metadata, after its previous request
    // has finished. Identical timestamps in the two products are not reusable.
    workerSource = requestedSource;
    cachedPngCount = 0;
    animationFrameCount = 0;
    pendingRefreshCount = 0;
    memset(preparedFrameReady, 0, sizeof(preparedFrameReady));
    if (cachedPngNames) memset(cachedPngNames, 0, MAX_ANIMATION_FRAME_COUNT * FILE_NAME_CAPACITY);
    if (preparedFrameNames) memset(preparedFrameNames, 0, MAX_ANIMATION_FRAME_COUNT * FILE_NAME_CAPACITY);
    ready = false;
    displayedFrame = -1;
    lastSuccessfulRefreshAt = 0;
    rebuildFromCacheRequested = false;
    restartAnimationRequested = false;
    reloadRequested = true;
    showBaseMapRequested = visible;
    nextAttemptAt = 0;
  }
  latitude = centerLatitude;
  longitude = centerLongitude;
  radiusKm = centerRadiusKm;
  wantedFrameCount = requestedFrameCount;
  mapOpacityValue = mapOpacity;
  revision = requestRevision;
  portEXIT_CRITICAL(&stateMux);
  return requested;
}

uint8_t rgb565ToRgb332(uint16_t color) {
  const uint8_t red = static_cast<uint8_t>((color >> 11) & 0x1f);
  const uint8_t green = static_cast<uint8_t>((color >> 5) & 0x3f);
  const uint8_t blue = static_cast<uint8_t>(color & 0x1f);
  return static_cast<uint8_t>(((red >> 2) << 5) | ((green >> 3) << 2) |
                              (blue >> 3));
}

uint16_t rgb332ToRgb565(uint8_t color) {
  const uint8_t red3 = color >> 5;
  const uint8_t green3 = (color >> 2) & 0x07;
  const uint8_t blue2 = color & 0x03;
  const uint16_t red5 = (red3 << 2) | (red3 >> 1);
  const uint16_t green6 = (green3 << 3) | green3;
  const uint16_t blue5 = (blue2 << 3) | (blue2 << 1) | (blue2 >> 1);
  return static_cast<uint16_t>((red5 << 11) | (green6 << 5) | blue5);
}

uint16_t nightRadarColor(uint8_t color) {
  uint8_t level = 0;
  switch (color) {
    case 0x00:
      return 0;
    // Stupně odrazivosti ČHMÚ od nejsilnějších po nejslabší. Po převodu
    // zdrojové palety do RGB332 zachováme jejich pořadí pomocí jasu červené.
    case 0xa0: level = 255; break;
    case 0xe0: level = 248; break;
    case 0xe8: level = 236; break;
    case 0xf0: level = 224; break;
    case 0xf4: level = 210; break;
    case 0xf8: level = 196; break;
    case 0x98: level = 180; break;
    case 0x38: level = 165; break;
    case 0x14: level = 150; break;
    case 0x0f: level = 132; break;
    case 0x03: level = 114; break;
    case 0x22: level = 96; break;
    case 0x21: level = 82; break;
    // Vlastní mapová vrstva a doplňkové barvy zdrojového PNG.
    case 0xff: level = 255; break;  // poloha a nejsilnější odraz
    case 0x1f: level = 210; break;  // města
    case 0xb6: level = 82; break;   // hranice ČR
    case 0x49: level = 48; break;   // okraj displeje
    case 0xdb: level = 64; break;   // pomocná kresba v PNG ČHMÚ
    default: {
      const uint8_t red = static_cast<uint8_t>(((color >> 5) & 0x07) * 255 / 7);
      const uint8_t green =
          static_cast<uint8_t>(((color >> 2) & 0x07) * 255 / 7);
      const uint8_t blue = static_cast<uint8_t>((color & 0x03) * 255 / 3);
      const uint16_t luminance =
          (static_cast<uint16_t>(red) * 54 +
           static_cast<uint16_t>(green) * 183 +
           static_cast<uint16_t>(blue) * 19) >> 8;
      level = constrain(static_cast<int>(luminance), 48, 170);
      break;
    }
  }
  // Maximum odpovídá stejné červené RGB(255,72,72), jakou používá noční UI.
  const uint8_t accent =
      static_cast<uint8_t>((static_cast<uint16_t>(level) * 72) / 255);
  return static_cast<uint16_t>(((level >> 3) << 11) |
                               ((accent >> 2) << 5) | (accent >> 3));
}

void applyNightRadarPalette(uint16_t *buffer) {
  for (size_t pixel = 0; pixel < RADAR_PIXEL_COUNT; ++pixel)
    buffer[pixel] = nightRadarColor(rgb565ToRgb332(buffer[pixel]));
}

bool ensurePendingFrame(size_t slot) {
  if (slot >= MAX_PENDING_REFRESH_FRAMES) return false;
  if (pendingPreparedFrames[slot] == nullptr) {
    pendingPreparedFrames[slot] = static_cast<uint8_t *>(allocateRadarMemory(
        RADAR_PIXEL_COUNT, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  }
  return pendingPreparedFrames[slot] != nullptr;
}

bool cachePendingPng(size_t slot, size_t size) {
  if (slot >= MAX_PENDING_REFRESH_FRAMES || size == 0 || size > PNG_CAPACITY)
    return false;
  if (!reserveSourceCache(0)) return false;
  if (pendingPngCapacities[slot] < size) {
    if (!reserveSourceCache(size)) return false;
    uint8_t *replacement = static_cast<uint8_t *>(allocateRadarMemory(
        size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (replacement == nullptr) return false;
    if (pendingPngFrames[slot] != nullptr)
      heap_caps_free(pendingPngFrames[slot]);
    pendingPngFrames[slot] = replacement;
    pendingPngCapacities[slot] = size;
  }
  memcpy(pendingPngFrames[slot], pngBuffer, size);
  pendingPngSizes[slot] = size;
  return true;
}

bool prepareFrame(size_t index, const uint8_t *pngData, size_t pngSize,
                  const char *fileName, float latitude, float longitude,
                  uint16_t radiusKm, uint8_t mapOpacityValue,
                  uint32_t revision) {
  if (!requestMatches(revision) || !ensurePreparedFrame(index)) return false;
  // Dekódujeme přímo do cache. Rozpracovaný snímek nesmí přehrávač použít,
  // ani když dekódování selže nebo se během něj změní požadovaná revize.
  portENTER_CRITICAL(&stateMux);
  preparedFrameReady[index] = false;
  portEXIT_CRITICAL(&stateMux);
  if (!decodeRadar(pngData, pngSize, latitude, longitude, radiusKm,
                   mapOpacityValue, preparedFrames[index]) ||
      !requestMatches(revision)) {
    return false;
  }
  char decodedTime[6] = "";
  frameTimeFromName(fileName, decodedTime);
  portENTER_CRITICAL(&stateMux);
  preparedFrameReady[index] = true;
  preparedFrameRevisions[index] = revision;
  strlcpy(preparedFrameTimes[index], decodedTime,
          sizeof(preparedFrameTimes[index]));
  strlcpy(preparedFrameNames[index], fileName,
          sizeof(preparedFrameNames[index]));
  portEXIT_CRITICAL(&stateMux);
  return true;
}

void beginProgressivePreparation(size_t count) {
  portENTER_CRITICAL(&stateMux);
  animationFrameCount = count;
  pendingRefreshCount = 0;
  preparationInProgress = true;
  animationPause = false;
  lastProgressiveFrameShownAt = 0;
  memset(preparedFrameReady, 0, sizeof(preparedFrameReady));
  portEXIT_CRITICAL(&stateMux);
}

bool showProgressivelyPreparedFrame(size_t index, uint16_t radiusKm,
                                    uint32_t revision) {
  portENTER_CRITICAL(&stateMux);
  const bool valid = active && revision == requestRevision;
  const bool shouldDisplay = visible && !restartAnimationRequested;
  if (valid) {
    ready = true;
    activeRadiusKm = radiusKm;
  }
  portEXIT_CRITICAL(&stateMux);
  if (!valid) return false;
  if (!shouldDisplay) return true;
  if (index > 0) {
    while (requestMatches(revision)) {
      portENTER_CRITICAL(&stateMux);
      const unsigned long previousShownAt = lastProgressiveFrameShownAt;
      portEXIT_CRITICAL(&stateMux);
      const unsigned long elapsed = millis() - previousShownAt;
      if (previousShownAt == 0 || elapsed >= PREPARATION_FRAME_MIN_MS) break;
      delay(min(10UL, PREPARATION_FRAME_MIN_MS - elapsed));
    }
  }
  const bool shown = showPreparedFrame(index, millis());
  if (shown) {
    portENTER_CRITICAL(&stateMux);
    lastProgressiveFrameShownAt = millis();
    portEXIT_CRITICAL(&stateMux);
  }
  return shown;
}

void finishProgressivePreparation(uint32_t revision) {
  const unsigned long now = millis();
  portENTER_CRITICAL(&stateMux);
  if (revision == requestRevision) {
    preparationInProgress = false;
    fullPreparationInProgress = false;
    animationPause = true;
    animationPauseStartedAt = now;
    lastAnimationStepAt = now;
    ++generation;
  }
  portEXIT_CRITICAL(&stateMux);
}

void releaseUnusedFrames(size_t usedCount) {
  for (size_t index = usedCount; index < MAX_ANIMATION_FRAME_COUNT; ++index) {
    if (preparedFrames[index] != nullptr) {
      heap_caps_free(preparedFrames[index]);
      preparedFrames[index] = nullptr;
    }
    preparedFrameReady[index] = false;
    preparedFrameRevisions[index] = 0;
    preparedFrameTimes[index][0] = '\0';
    preparedFrameNames[index][0] = '\0';
    if (cachedPngFrames[index] != nullptr) {
      heap_caps_free(cachedPngFrames[index]);
      cachedPngFrames[index] = nullptr;
    }
    cachedPngSizes[index] = 0;
    cachedPngCapacities[index] = 0;
    cachedPngNames[index][0] = '\0';
  }
}

bool rebuildAnimationFromCache(float latitude, float longitude,
                               uint16_t radiusKm, uint8_t wantedFrameCount,
                               uint8_t mapOpacityValue, uint32_t revision) {
  if (!ensureBuffers()) {
    setStatus(false, "Nedostatek pameti pro radar");
    return false;
  }
  const size_t availableCount = min(
      cachedPngCount, static_cast<size_t>(wantedFrameCount));
  if (availableCount == 0) return false;
  setStatus(true, "Menim rozsah radaru...");
  beginProgressivePreparation(availableCount);
  for (size_t index = 0; index < availableCount; ++index) {
    if (!prepareFrame(index, cachedPngFrames[index], cachedPngSizes[index],
                      cachedPngNames[index], latitude, longitude, radiusKm,
                      mapOpacityValue, revision) ||
        !showProgressivelyPreparedFrame(index, radiusKm, revision)) {
      setStatus(false, "Snimek radaru se nepodarilo pripravit");
      portENTER_CRITICAL(&stateMux);
      if (revision == requestRevision) preparationInProgress = false;
      portEXIT_CRITICAL(&stateMux);
      return false;
    }
  }
  setStatus(false, "");
  finishProgressivePreparation(revision);
  return requestMatches(revision);
}

void releasePendingWorkspace() {
  pendingRefreshCount = 0;
  for (size_t i = 0; i < MAX_PENDING_REFRESH_FRAMES; ++i) {
    heap_caps_free(pendingPreparedFrames[i]);
    pendingPreparedFrames[i] = nullptr;
    heap_caps_free(pendingPngFrames[i]);
    pendingPngFrames[i] = nullptr;
    pendingPngSizes[i] = pendingPngCapacities[i] = 0;
    pendingFrameNames[i][0] = '\0';
    pendingFrameRevisions[i] = 0;
  }
}

// Salvage successful downloads from an interrupted incremental batch before a
// changed manifest or memory pressure sends us through the general loader.
void adoptPendingFrames(const char names[][FILE_NAME_CAPACITY], size_t count,
                        uint32_t revision) {
  for (size_t slot = 0; slot < MAX_PENDING_REFRESH_FRAMES; ++slot) {
    if (!pendingPreparedFrames[slot] || pendingFrameRevisions[slot] != revision ||
        !pendingFrameNames[slot][0]) continue;
    bool wanted = false, present = false;
    for (size_t i = 0; i < count; ++i)
      if (strcmp(names[i], pendingFrameNames[slot]) == 0) wanted = true;
    for (size_t i = 0; i < MAX_ANIMATION_FRAME_COUNT; ++i)
      if (strcmp(cachedPngNames[i], pendingFrameNames[slot]) == 0) present = true;
    if (!wanted || present) continue;
    for (size_t i = 0; i < MAX_ANIMATION_FRAME_COUNT; ++i) {
      bool needed = false;
      for (size_t j = 0; j < count; ++j)
        if (strcmp(cachedPngNames[i], names[j]) == 0) needed = true;
      if (needed) continue;
      portENTER_CRITICAL(&stateMux);
      std::swap(preparedFrames[i], pendingPreparedFrames[slot]);
      std::swap(cachedPngFrames[i], pendingPngFrames[slot]);
      std::swap(cachedPngSizes[i], pendingPngSizes[slot]);
      std::swap(cachedPngCapacities[i], pendingPngCapacities[slot]);
      preparedFrameReady[i] = true;
      preparedFrameRevisions[i] = revision;
      strlcpy(preparedFrameNames[i], pendingFrameNames[slot], FILE_NAME_CAPACITY);
      strlcpy(cachedPngNames[i], pendingFrameNames[slot], FILE_NAME_CAPACITY);
      strlcpy(preparedFrameTimes[i], pendingFrameTimes[slot], sizeof(preparedFrameTimes[i]));
      portEXIT_CRITICAL(&stateMux);
      break;
    }
  }
}

void reconcileFrameCache(const char names[][FILE_NAME_CAPACITY], size_t count) {
  portENTER_CRITICAL(&stateMux);
  preparationInProgress = true;
  animationPause = false;
  lastProgressiveFrameShownAt = 0;
  displayedFrame = -1;
  for (size_t i = 0; i < count; ++i) {
    size_t match = i;
    while (match < MAX_ANIMATION_FRAME_COUNT &&
           strcmp(cachedPngNames[match], names[i]) != 0) ++match;
    if (match == MAX_ANIMATION_FRAME_COUNT) {
      // Reuse a slot that is not needed later in the new sequence.
      for (match = i; match < MAX_ANIMATION_FRAME_COUNT; ++match) {
        bool needed = false;
        for (size_t j = i + 1; j < count; ++j)
          if (strcmp(cachedPngNames[match], names[j]) == 0) needed = true;
        if (!needed) break;
      }
      preparedFrameReady[match] = false;
      cachedPngSizes[match] = 0;
      cachedPngNames[match][0] = '\0';
    }
    if (match != i) {
      std::swap(preparedFrames[i], preparedFrames[match]);
      std::swap(preparedFrameReady[i], preparedFrameReady[match]);
      std::swap(preparedFrameRevisions[i], preparedFrameRevisions[match]);
      std::swap(preparedFrameTimes[i], preparedFrameTimes[match]);
      std::swap(preparedFrameNames[i], preparedFrameNames[match]);
      std::swap(cachedPngFrames[i], cachedPngFrames[match]);
      std::swap(cachedPngSizes[i], cachedPngSizes[match]);
      std::swap(cachedPngCapacities[i], cachedPngCapacities[match]);
      std::swap(cachedPngNames[i], cachedPngNames[match]);
    }
  }
  animationFrameCount = count;
  cachedPngCount = 0;
  portEXIT_CRITICAL(&stateMux);
}

bool loadAnimation(float latitude, float longitude, uint16_t radiusKm,
                   uint8_t wantedFrameCount, uint8_t mapOpacityValue,
                   uint32_t revision,
                   const char (*knownNames)[FILE_NAME_CAPACITY] = nullptr,
                   size_t knownCount = 0) {
  if (WiFi.status() != WL_CONNECTED) {
    setStatus(false, "Wi-Fi neni pripojena");
    return false;
  }
  if (!ensureBuffers()) {
    setStatus(false, "Nedostatek pameti pro radar");
    return false;
  }
  portENTER_CRITICAL(&stateMux);
  fullPreparationInProgress = true;
  ++generation;
  portEXIT_CRITICAL(&stateMux);
  setStatus(true, "Obnovuji radar...");
  char latestNames[MAX_ANIMATION_FRAME_COUNT][FILE_NAME_CAPACITY] = {};
  size_t latestCount = knownCount;
  if (knownNames) memcpy(latestNames, knownNames, knownCount * FILE_NAME_CAPACITY);
  if (!knownNames && !latestFileNames(latestNames, latestCount, revision)) {
    setStatus(false, "Seznam radaru se nepodarilo nacist");
    portENTER_CRITICAL(&stateMux);
    if (revision == requestRevision) fullPreparationInProgress = false;
    portEXIT_CRITICAL(&stateMux);
    return false;
  }
  const size_t selectedCount =
      min(latestCount, static_cast<size_t>(wantedFrameCount));
  const size_t selectedStart = latestCount - selectedCount;
  if (selectedCount == 0) {
    portENTER_CRITICAL(&stateMux);
    if (revision == requestRevision) fullPreparationInProgress = false;
    portEXIT_CRITICAL(&stateMux);
    return false;
  }

  adoptPendingFrames(latestNames + selectedStart, selectedCount, revision);
  releasePendingWorkspace();
  reconcileFrameCache(latestNames + selectedStart, selectedCount);
  releaseUnusedFrames(selectedCount);
  size_t loadedCount = 0;

  const auto prepareMissingFrame = [&](size_t index) {
    const char *fileName = latestNames[selectedStart + index];
    if (preparedFrameReady[index] &&
        preparedFrameRevisions[index] == revision)
      return true;

    // Rychlé zavření radaru zneplatní rozpracovanou revizi, ale již stažené
    // PNG ponecháváme v cache. Po návratu je musíme znovu promítnout do
    // aktuálního výřezu a označit novou revizí; jinak showPreparedFrame()
    // starý snímek odmítne a na displeji zůstane pouze podkladová mapa.
    if (cachedPngFrames[index] != nullptr &&
        cachedPngSizes[index] > 0 &&
        strcmp(cachedPngNames[index], fileName) == 0) {
      if (!prepareFrame(index, cachedPngFrames[index], cachedPngSizes[index],
                        cachedPngNames[index], latitude, longitude, radiusKm,
                        mapOpacityValue, revision)) {
        setStatus(false, "Snimek radaru se nepodarilo pripravit");
        return false;
      }
      return true;
    }

    size_t pngSize = 0;
    if (!downloadPngWithRetry(fileName, pngSize, revision)) {
      setStatus(false, "Snimek radaru se nepodarilo stahnout");
      return false;
    }
    if (!prepareFrame(index, pngBuffer, pngSize, fileName, latitude,
                      longitude, radiusKm, mapOpacityValue, revision)) {
      setStatus(false, "Snimek radaru se nepodarilo pripravit");
      return false;
    }
    // Retaining the source is optional under pressure, retaining its identity
    // is not: refresh must still recognize the already prepared image.
    if (!cacheDownloadedPng(index, pngSize, fileName)) {
      heap_caps_free(cachedPngFrames[index]);
      cachedPngFrames[index] = nullptr;
      cachedPngSizes[index] = cachedPngCapacities[index] = 0;
      strlcpy(cachedPngNames[index], fileName, FILE_NAME_CAPACITY);
    }
    return true;
  };

  for (size_t index = 0; index < selectedCount; ++index) {
    if (!prepareMissingFrame(index)) {
      portENTER_CRITICAL(&stateMux);
      if (revision == requestRevision) fullPreparationInProgress = false;
      portEXIT_CRITICAL(&stateMux);
      return false;
    }
    ++loadedCount;
    portENTER_CRITICAL(&stateMux);
    cachedPngCount = loadedCount;
    ready = true;
    activeRadiusKm = radiusKm;
    portEXIT_CRITICAL(&stateMux);
    if (!showProgressivelyPreparedFrame(index, radiusKm, revision)) {
      setStatus(false, "Snímek radaru se nepodařilo zobrazit");
      portENTER_CRITICAL(&stateMux);
      if (revision == requestRevision) fullPreparationInProgress = false;
      portEXIT_CRITICAL(&stateMux);
      return false;
    }
  }

  if (!requestMatches(revision)) {
    setStatus(false, "");
    return false;
  }
  finishProgressivePreparation(revision);
  if (loadedCount == selectedCount) {
    releaseUnusedFrames(selectedCount);
    setStatus(false, "");
    return true;
  }
  return false;
}

void commitPendingRefresh() {
  const size_t shift = pendingRefreshCount;
  if (shift == 0 || shift > animationFrameCount ||
      shift > MAX_PENDING_REFRESH_FRAMES)
    return;
  uint8_t *releasedPrepared[MAX_PENDING_REFRESH_FRAMES] = {};
  uint8_t *releasedPng[MAX_PENDING_REFRESH_FRAMES] = {};
  size_t releasedPngCapacities[MAX_PENDING_REFRESH_FRAMES] = {};
  for (size_t index = 0; index < shift; ++index) {
    releasedPrepared[index] = preparedFrames[index];
    releasedPng[index] = cachedPngFrames[index];
    releasedPngCapacities[index] = cachedPngCapacities[index];
  }
  for (size_t index = 0; index + shift < animationFrameCount; ++index) {
    preparedFrames[index] = preparedFrames[index + shift];
    preparedFrameReady[index] = preparedFrameReady[index + shift];
    preparedFrameRevisions[index] = preparedFrameRevisions[index + shift];
    strlcpy(preparedFrameTimes[index], preparedFrameTimes[index + shift],
            sizeof(preparedFrameTimes[index]));
    strlcpy(preparedFrameNames[index], preparedFrameNames[index + shift],
            sizeof(preparedFrameNames[index]));
    cachedPngFrames[index] = cachedPngFrames[index + shift];
    cachedPngSizes[index] = cachedPngSizes[index + shift];
    cachedPngCapacities[index] = cachedPngCapacities[index + shift];
    strlcpy(cachedPngNames[index], cachedPngNames[index + shift],
            sizeof(cachedPngNames[index]));
  }
  const size_t firstPending = animationFrameCount - shift;
  for (size_t slot = 0; slot < shift; ++slot) {
    const size_t target = firstPending + slot;
    preparedFrames[target] = pendingPreparedFrames[slot];
    preparedFrameReady[target] = true;
    preparedFrameRevisions[target] = pendingFrameRevisions[slot];
    strlcpy(preparedFrameTimes[target], pendingFrameTimes[slot],
            sizeof(preparedFrameTimes[target]));
    strlcpy(preparedFrameNames[target], pendingFrameNames[slot],
            sizeof(preparedFrameNames[target]));
    cachedPngFrames[target] = pendingPngFrames[slot];
    cachedPngSizes[target] = pendingPngSizes[slot];
    cachedPngCapacities[target] = pendingPngCapacities[slot];
    strlcpy(cachedPngNames[target], pendingFrameNames[slot],
            sizeof(cachedPngNames[target]));
    pendingPreparedFrames[slot] = releasedPrepared[slot];
    pendingPngFrames[slot] = releasedPng[slot];
    pendingPngSizes[slot] = 0;
    pendingPngCapacities[slot] = releasedPngCapacities[slot];
    pendingFrameNames[slot][0] = '\0';
    pendingFrameTimes[slot][0] = '\0';
    pendingFrameRevisions[slot] = 0;
  }
  pendingRefreshCount = 0;
}

bool refreshLatestFrame(float latitude, float longitude, uint16_t radiusKm,
                        uint8_t wantedFrameCount, uint8_t mapOpacityValue,
                        uint32_t revision) {
  if (WiFi.status() != WL_CONNECTED || !ensureBuffers()) return false;
  setStatus(true, "Obnovuji radar...");
  char latestNames[MAX_ANIMATION_FRAME_COUNT][FILE_NAME_CAPACITY] = {};
  size_t latestCount = 0;
  if (!latestFileNames(latestNames, latestCount, revision)) {
    setStatus(false, "Seznam radaru se nepodarilo nacist");
    return false;
  }
  const size_t selectedCount =
      min(latestCount, static_cast<size_t>(wantedFrameCount));
  const size_t selectedStart = latestCount - selectedCount;
  if (selectedCount != animationFrameCount || selectedCount != cachedPngCount ||
      selectedCount == 0) {
    return loadAnimation(latitude, longitude, radiusKm, wantedFrameCount,
                         mapOpacityValue, revision, latestNames, latestCount);
  }
  bool unchanged = true;
  for (size_t index = 0; index < selectedCount; ++index) {
    if (strcmp(latestNames[selectedStart + index], preparedFrameNames[index]) !=
        0) {
      unchanged = false;
      break;
    }
  }
  if (unchanged) {
    setStatus(false, "");
    return true;
  }
  size_t shift = 0;
  for (size_t candidate = 1;
       candidate < selectedCount && candidate <= MAX_PENDING_REFRESH_FRAMES;
       ++candidate) {
    bool overlapMatches = true;
    for (size_t index = 0; index + candidate < selectedCount; ++index) {
      if (strcmp(latestNames[selectedStart + index],
                 preparedFrameNames[index + candidate]) != 0) {
        overlapMatches = false;
        break;
      }
    }
    if (overlapMatches) {
      shift = candidate;
      break;
    }
  }
  if (shift == 0) {
    return loadAnimation(latitude, longitude, radiusKm, wantedFrameCount,
                         mapOpacityValue, revision, latestNames, latestCount);
  }

  pendingRefreshCount = 0;
  for (size_t slot = 0; slot < shift; ++slot) {
    if (!ensurePendingFrame(slot)) {
      return loadAnimation(latitude, longitude, radiusKm, wantedFrameCount,
                           mapOpacityValue, revision, latestNames, latestCount);
    }
  }
  const size_t firstNew = selectedCount - shift;
  for (size_t slot = 0; slot < shift; ++slot) {
    const char *fileName = latestNames[selectedStart + firstNew + slot];
    if (pendingFrameRevisions[slot] == revision &&
        strcmp(pendingFrameNames[slot], fileName) == 0) continue;
    // A failed retry must never advertise the previous contents of this slot.
    pendingFrameNames[slot][0] = '\0';
    pendingFrameRevisions[slot] = 0;
    size_t pngSize = 0;
    if (!downloadPngWithRetry(fileName, pngSize, revision) ||
        !decodeRadar(pngBuffer, pngSize, latitude, longitude, radiusKm,
                     mapOpacityValue, pendingPreparedFrames[slot]) ||
        !requestMatches(revision)) {
      setStatus(false, "Nove snimky radaru se nepodarilo pripravit");
      pendingRefreshCount = 0;
      return false;
    }
    if (!cachePendingPng(slot, pngSize)) {
      heap_caps_free(pendingPngFrames[slot]);
      pendingPngFrames[slot] = nullptr;
      pendingPngSizes[slot] = pendingPngCapacities[slot] = 0;
    }
    strlcpy(pendingFrameNames[slot], fileName,
            sizeof(pendingFrameNames[slot]));
    frameTimeFromName(fileName, pendingFrameTimes[slot]);
    pendingFrameRevisions[slot] = revision;
  }
  pendingRefreshCount = shift;
  setStatus(false, "");

  portENTER_CRITICAL(&stateMux);
  const bool atBoundary = animationPause &&
                          displayedFrame + 1 ==
                              static_cast<int>(animationFrameCount);
  const bool refreshInBackground = !visible;
  portEXIT_CRITICAL(&stateMux);
  if (animationFrameCount == 1) {
    commitPendingRefresh();
    return showPreparedFrame(0, millis());
  }
  if (atBoundary || refreshInBackground) commitPendingRefresh();
  return true;
}

int firstPreparedFrame() {
  for (size_t index = 0; index < animationFrameCount; ++index)
    if (preparedFrameReady[index] &&
        preparedFrameRevisions[index] == requestRevision)
      return static_cast<int>(index);
  return -1;
}

bool showPreparedFrame(size_t index, unsigned long now) {
  portENTER_CRITICAL(&stateMux);
  const bool valid = active && index < animationFrameCount &&
                     preparedFrameReady[index] &&
                     preparedFrames[index] != nullptr &&
                     preparedFrameRevisions[index] == requestRevision;
  const bool nightVisual = redNightMode;
  portEXIT_CRITICAL(&stateMux);
  if (!valid) return false;
  const uint8_t targetBuffer = 1 - activeDisplayBuffer;
  uint16_t *target = displayBuffers[targetBuffer];
  const uint8_t *source = preparedFrames[index];
  for (size_t pixel = 0; pixel < RADAR_PIXEL_COUNT; ++pixel)
    target[pixel] = nightVisual
        ? nightRadarColor(slovakSource() ? rgb565ToRgb332(decodePreparedColor(source[pixel])) : source[pixel])
        : decodePreparedColor(source[pixel]);
  portENTER_CRITICAL(&stateMux);
  if (!active || index >= animationFrameCount || !preparedFrameReady[index] ||
      preparedFrameRevisions[index] != requestRevision) {
    portEXIT_CRITICAL(&stateMux);
    return false;
  }
  activeDisplayBuffer = targetBuffer;
  displayedFrame = static_cast<int>(index);
  strlcpy(frameTime, preparedFrameTimes[index], sizeof(frameTime));
  ++generation;
  lastAnimationStepAt = now;
  portEXIT_CRITICAL(&stateMux);
  return true;
}

bool playbackHeld = false;

void advanceAnimation(unsigned long now) {
  portENTER_CRITICAL(&stateMux);
  if (playbackHeld) {
    portEXIT_CRITICAL(&stateMux);
    return;
  }
  const bool isVisible = visible;
  const bool canAnimate = ready && animationFrameCount > 1 &&
                          displayedFrame >= 0;
  const bool paused = animationPause;
  const bool preparing = preparationInProgress;
  const unsigned long pauseStarted = animationPauseStartedAt;
  const unsigned long pauseDuration =
      static_cast<unsigned long>(pauseSeconds) * 1000UL;
  const unsigned long lastStep = lastAnimationStepAt;
  const int currentFrame = displayedFrame;
  portEXIT_CRITICAL(&stateMux);
  if (!isVisible || !canAnimate || preparing) return;
  if (paused) {
    if (now - pauseStarted >= pauseDuration) {
      const int first = firstPreparedFrame();
      if (first >= 0 && first != currentFrame &&
          showPreparedFrame(static_cast<size_t>(first), now)) {
        portENTER_CRITICAL(&stateMux);
        animationPause = false;
        ++completedAnimationCycles;
        portEXIT_CRITICAL(&stateMux);
      }
    }
    return;
  }
  if (now - lastStep < ANIMATION_STEP_MS) return;
  const int nextFrame = currentFrame + 1;
  if (nextFrame < static_cast<int>(animationFrameCount) &&
      preparedFrameReady[nextFrame]) {
    if (showPreparedFrame(static_cast<size_t>(nextFrame), now) &&
        nextFrame + 1 >= static_cast<int>(animationFrameCount)) {
      commitPendingRefresh();
      portENTER_CRITICAL(&stateMux);
      animationPause = true;
      animationPauseStartedAt = now;
      portEXIT_CRITICAL(&stateMux);
    }
  } else if (currentFrame + 1 >= static_cast<int>(animationFrameCount)) {
    commitPendingRefresh();
    portENTER_CRITICAL(&stateMux);
    animationPause = true;
    animationPauseStartedAt = now;
    portEXIT_CRITICAL(&stateMux);
  }
}

void radarTask(void *) {
  while (true) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));
    float latitude = 0;
    float longitude = 0;
    uint16_t radiusKm = 50;
    uint8_t wantedFrameCount = 6;
    uint8_t mapOpacityValue = 100;
    uint32_t revision = 0;
    if (!currentRequest(latitude, longitude, radiusKm, wantedFrameCount,
                        mapOpacityValue, revision))
      continue;
    const unsigned long now = millis();
    bool haveFrames = false;
    bool rebuildRequested = false;
    bool reloadNow = false;
    bool showBase = false;
    bool restartAnimation = false;
    bool redrawNightVisual = false;
    int frameToRedraw = -1;
    portENTER_CRITICAL(&stateMux);
    // Při přípravě na pozadí zůstává displayedFrame == -1, dokud se radar
    // poprvé nezobrazí. Hotovou animaci proto určujeme podle připravených
    // snímků, jinak by ji worker do prvního zobrazení načítal stále dokola.
    haveFrames = ready && animationFrameCount > 0;
    rebuildRequested = rebuildFromCacheRequested;
    reloadNow = reloadRequested;
    showBase = showBaseMapRequested;
    restartAnimation = restartAnimationRequested;
    redrawNightVisual = nightVisualRedrawRequested;
    frameToRedraw = displayedFrame;
    portEXIT_CRITICAL(&stateMux);
    if (showBase) {
      showBaseMap(latitude, longitude, radiusKm, mapOpacityValue, revision);
      portENTER_CRITICAL(&stateMux);
      if (revision == requestRevision) {
        showBaseMapRequested = false;
        nightVisualRedrawRequested = false;
      }
      portEXIT_CRITICAL(&stateMux);
      haveFrames = false;
    }
    if (redrawNightVisual && !showBase) {
      bool redrawn = false;
      if (frameToRedraw >= 0)
        redrawn = showPreparedFrame(static_cast<size_t>(frameToRedraw), now);
      else if (frameToRedraw == -2)
        redrawn = showBaseMap(latitude, longitude, radiusKm, mapOpacityValue,
                              revision);
      portENTER_CRITICAL(&stateMux);
      if (revision == requestRevision) nightVisualRedrawRequested = false;
      portEXIT_CRITICAL(&stateMux);
      if (redrawn) continue;
    }
    if (restartAnimation) {
      const int first = firstPreparedFrame();
      if (first >= 0 &&
          showPreparedFrame(static_cast<size_t>(first), millis())) {
        portENTER_CRITICAL(&stateMux);
        if (revision == requestRevision) {
          restartAnimationRequested = false;
          animationPause = false;
        }
        portEXIT_CRITICAL(&stateMux);
      }
      continue;
    }
    if (rebuildRequested) {
      const bool success = rebuildAnimationFromCache(
          latitude, longitude, radiusKm, wantedFrameCount, mapOpacityValue,
          revision);
      const unsigned long scheduledNextAttemptAt =
          success ? millis() + millisecondsUntilNextRefreshSlot() : 0;
      portENTER_CRITICAL(&stateMux);
      if (revision == requestRevision) {
        rebuildFromCacheRequested = false;
        if (success) {
          // Přepočet jiného výřezu nemění stáří zdrojových dat. Další dotaz
          // na ČHMÚ proto naplánujeme podle posledního skutečného stažení.
          nextAttemptAt = scheduledNextAttemptAt;
        } else {
          reloadRequested = true;
          nextAttemptAt = 0;
        }
      }
      portEXIT_CRITICAL(&stateMux);
      continue;
    }
    // Respect retry backoff even when the first frame has not loaded yet.
    if (reloadNow || nextAttemptAt == 0 ||
        static_cast<int32_t>(now - nextAttemptAt) >= 0) {
      const bool fullPreparation = reloadNow || !haveFrames;
      RadarHttpBatch batch;
      httpBatch = &batch;
      const bool success = batch.guard &&
          (fullPreparation
              ? loadAnimation(latitude, longitude, radiusKm, wantedFrameCount,
                              mapOpacityValue, revision)
              : refreshLatestFrame(latitude, longitude, radiusKm,
                                   wantedFrameCount, mapOpacityValue, revision));
      httpBatch = nullptr;
      portENTER_CRITICAL(&stateMux);
      const bool requestChanged = revision != requestRevision;
      if (!requestChanged) reloadRequested = false;
      portEXIT_CRITICAL(&stateMux);
      nextAttemptAt = requestChanged
                          ? 0
                          : millis() + (success ? millisecondsUntilNextRefreshSlot()
                                                : RETRY_INTERVAL_MS);
      if (success && !requestChanged) lastSuccessfulRefreshAt = millis();
      continue;
    }
    advanceAnimation(now);
  }
}
}  // namespace

bool chmiRadarServiceMemoryReclaimRequested() {
  return memoryReclaimRequested.load(std::memory_order_acquire);
}

void chmiRadarServiceMemoryReclaimCompleted() {
  memoryReclaimRequested.store(false, std::memory_order_release);
}

void chmiRadarServiceBegin() {
  if (taskHandle != nullptr) return;
  struct Metadata {
    slovakRadar::Frame frames[slovakRadar::MAX_FRAMES];
    char prepared[MAX_ANIMATION_FRAME_COUNT][FILE_NAME_CAPACITY];
    char cached[MAX_ANIMATION_FRAME_COUNT][FILE_NAME_CAPACITY];
    char pending[MAX_PENDING_REFRESH_FRAMES][FILE_NAME_CAPACITY];
  };
  if (!slovakFrames) {
    void *storage = heap_caps_malloc(sizeof(Metadata), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!storage) { setStatus(false, "Nedostatek pameti pro radar"); return; }
    auto *metadata = new (storage) Metadata{};
    slovakFrames = metadata->frames;
    preparedFrameNames = metadata->prepared;
    cachedPngNames = metadata->cached;
    pendingFrameNames = metadata->pending;
  }
  xTaskCreatePinnedToCoreWithCaps(
      radarTask, "chmi-radar", 12288, nullptr, 1, &taskHandle, 0,
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void chmiRadarServicePrepareForFirmwareUpdate() {
  portENTER_CRITICAL(&stateMux);
  active = false;
  visible = false;
  ++requestRevision;
  portEXIT_CRITICAL(&stateMux);

  // OTA drzi sitovy koordinator, radar tedy v tuto chvili nemuze byt uvnitr
  // HTTP/TLS operace. Ukoncenim tasku zastavime i pripadny dekodovaci cyklus,
  // nez uvolnime jeho pracovni a zobrazovaci buffery.
  if (taskHandle != nullptr) {
    vTaskDeleteWithCaps(taskHandle);
    taskHandle = nullptr;
  }
  memoryReclaimRequested.store(false, std::memory_order_release);
  if (pngDecoder != nullptr) pngDecoder->close();
  for (uint16_t *&buffer : displayBuffers) {
    if (buffer != nullptr) heap_caps_free(buffer);
    buffer = nullptr;
  }
  if (pngBuffer != nullptr) heap_caps_free(pngBuffer);
  pngBuffer = nullptr;
  pngBufferCapacity = 0;
  if (lineBuffer != nullptr) heap_caps_free(lineBuffer);
  lineBuffer = nullptr;
  lineCapacity = 0;
  for (size_t index = 0; index < MAX_ANIMATION_FRAME_COUNT; ++index) {
    if (preparedFrames[index] != nullptr) heap_caps_free(preparedFrames[index]);
    preparedFrames[index] = nullptr;
    if (cachedPngFrames[index] != nullptr) heap_caps_free(cachedPngFrames[index]);
    cachedPngFrames[index] = nullptr;
    cachedPngSizes[index] = 0;
    cachedPngCapacities[index] = 0;
    preparedFrameReady[index] = false;
  }
  for (size_t index = 0; index < MAX_PENDING_REFRESH_FRAMES; ++index) {
    if (pendingPreparedFrames[index] != nullptr)
      heap_caps_free(pendingPreparedFrames[index]);
    pendingPreparedFrames[index] = nullptr;
    if (pendingPngFrames[index] != nullptr) heap_caps_free(pendingPngFrames[index]);
    pendingPngFrames[index] = nullptr;
    pendingPngSizes[index] = 0;
    pendingPngCapacities[index] = 0;
  }
  decodeTarget = nullptr;
  cachedPngCount = 0;
  pendingRefreshCount = 0;
  animationFrameCount = 0;
  displayedFrame = -1;
  ready = false;
  loading = false;
  preparationInProgress = false;
  fullPreparationInProgress = false;
}

void chmiRadarServiceHoldPlayback(bool hold) {
  portENTER_CRITICAL(&stateMux);
  playbackHeld = hold;
  if (!hold) {
    lastAnimationStepAt = millis();
    if (animationPause) animationPauseStartedAt = millis();
  }
  portEXIT_CRITICAL(&stateMux);
}

void chmiRadarServiceSetActive(bool requestedVisible, bool backgroundRefresh,
                               float latitude, float longitude,
                               uint16_t radiusKm, uint8_t frameCount,
                               uint8_t mapOpacityValue,
                               uint8_t pauseSecondsValue, uint8_t source) {
  frameCount = constrain(frameCount, static_cast<uint8_t>(1),
                         static_cast<uint8_t>(source == CLOCK_RADAR_SOURCE_SHMU ? slovakRadar::MAX_FRAMES : MAX_ANIMATION_FRAME_COUNT));
  mapOpacityValue = constrain(mapOpacityValue, static_cast<uint8_t>(0),
                              static_cast<uint8_t>(100));
  pauseSecondsValue = constrain(pauseSecondsValue, static_cast<uint8_t>(0),
                                static_cast<uint8_t>(30));
  portENTER_CRITICAL(&stateMux);
  const bool sourceChanged = requestedSource != source;
  requestedSource = source;
  const bool wasEnabled = active;
  const bool wasVisible = visible;
  const bool requestedEnabled = requestedVisible || backgroundRefresh;
  const bool projectionChanged =
      fabsf(centerLatitude - latitude) > 0.00001f ||
      fabsf(centerLongitude - longitude) > 0.00001f ||
      centerRadiusKm != radiusKm || mapOpacity != mapOpacityValue;
  const bool frameCountChanged = requestedFrameCount != frameCount;
  bool completePngCache = workerSource == requestedSource &&
                          cachedPngCount == requestedFrameCount;
  if (completePngCache) {
    for (size_t index = 0; index < cachedPngCount; ++index)
      if (cachedPngFrames[index] == nullptr || cachedPngSizes[index] == 0 ||
          cachedPngNames[index][0] == '\0') {
        completePngCache = false;
        break;
      }
  }
  bool completePreparedCache =
      ready && workerSource == requestedSource &&
      animationFrameCount == requestedFrameCount;
  if (completePreparedCache) {
    for (size_t index = 0; index < animationFrameCount; ++index)
      if (!preparedFrameReady[index]) {
        completePreparedCache = false;
        break;
      }
  }
  const bool freshCache = lastSuccessfulRefreshAt != 0 &&
                          millis() - lastSuccessfulRefreshAt <
                              REFRESH_INTERVAL_MS;
  active = requestedEnabled;
  visible = requestedVisible;
  centerLatitude = latitude;
  centerLongitude = longitude;
  centerRadiusKm = radiusKm;
  requestedFrameCount = frameCount;
  mapOpacity = mapOpacityValue;
  pauseSeconds = pauseSecondsValue;
  if (!requestedEnabled && (wasEnabled || sourceChanged)) {
    ++requestRevision;
    rebuildFromCacheRequested = false;
    reloadRequested = false;
    showBaseMapRequested = false;
    restartAnimationRequested = false;
    preparationInProgress = false;
    fullPreparationInProgress = false;
  } else if (requestedEnabled && (projectionChanged || frameCountChanged || sourceChanged)) {
    ++requestRevision;
    showBaseMapRequested = requestedVisible;
    restartAnimationRequested = false;
    rebuildFromCacheRequested = !sourceChanged && projectionChanged && !frameCountChanged &&
                                completePngCache;
    reloadRequested = !rebuildFromCacheRequested;
    nextAttemptAt = 0;
  } else if (requestedEnabled && !wasEnabled) {
    if (completePreparedCache) {
      for (size_t index = 0; index < animationFrameCount; ++index)
        if (preparedFrameReady[index])
          preparedFrameRevisions[index] = requestRevision;
      restartAnimationRequested = requestedVisible;
      showBaseMapRequested = false;
      rebuildFromCacheRequested = false;
      reloadRequested = false;
      if (!freshCache) nextAttemptAt = 0;
    } else {
      ++requestRevision;
      showBaseMapRequested = requestedVisible;
      restartAnimationRequested = false;
      rebuildFromCacheRequested = completePngCache;
      reloadRequested = !completePngCache;
      nextAttemptAt = 0;
    }
  } else if (requestedEnabled && requestedVisible && !wasVisible) {
    // Cache mohla být během zobrazení hodin doplněna. Při návratu vždy
    // začínáme od jejího nejstaršího připraveného snímku.
    restartAnimationRequested = completePreparedCache ||
                                preparationInProgress || ready;
    showBaseMapRequested = !ready && !preparationInProgress;
  } else if (requestedEnabled && !requestedVisible && wasVisible) {
    restartAnimationRequested = false;
  }
  portEXIT_CRITICAL(&stateMux);
  if (requestedEnabled && taskHandle != nullptr) xTaskNotifyGive(taskHandle);
}

void chmiRadarServiceSetRedNightMode(bool enabled) {
  portENTER_CRITICAL(&stateMux);
  if (redNightMode == enabled) {
    portEXIT_CRITICAL(&stateMux);
    return;
  }
  redNightMode = enabled;
  nightVisualRedrawRequested = visible && displayedFrame != -1;
  const bool notify = nightVisualRedrawRequested;
  portEXIT_CRITICAL(&stateMux);
  if (notify && taskHandle != nullptr) xTaskNotifyGive(taskHandle);
}

void chmiRadarServiceSnapshot(ChmiRadarSnapshot &snapshot) {
  portENTER_CRITICAL(&stateMux);
  snapshot.pixels = displayedFrame != -1 ? displayBuffers[activeDisplayBuffer]
                                          : nullptr;
  snapshot.generation = generation;
  snapshot.completedAnimationCycles = completedAnimationCycles;
  snapshot.loading = loading;
  snapshot.ready = ready;
  snapshot.fullPreparationInProgress = fullPreparationInProgress;
  snapshot.latestFrame =
      ready && displayedFrame >= 0 &&
      displayedFrame + 1 == static_cast<int>(animationFrameCount);
  snapshot.currentFrameNumber =
      displayedFrame >= 0 ? static_cast<uint8_t>(displayedFrame + 1) : 0;
  snapshot.animationFrameCount = static_cast<uint8_t>(animationFrameCount);
  snapshot.pauseSeconds = pauseSeconds;
  snapshot.radiusKm = activeRadiusKm;
  strlcpy(snapshot.frameTime, frameTime, sizeof(snapshot.frameTime));
  strlcpy(snapshot.message, statusMessage, sizeof(snapshot.message));
  portEXIT_CRITICAL(&stateMux);
}

void chmiRadarServiceDiagnostics(ChmiRadarDiagnostics &diagnostics) {
  const unsigned long now = millis();
  portENTER_CRITICAL(&stateMux);
  diagnostics.active = visible;
  diagnostics.loading = loading;
  diagnostics.ready = ready;
  diagnostics.preparationInProgress = preparationInProgress;
  diagnostics.lastSuccessfulRefreshAvailable = lastSuccessfulRefreshAt != 0;
  diagnostics.requestedFrameCount = requestedFrameCount;
  diagnostics.animationFrameCount = static_cast<uint8_t>(animationFrameCount);
  diagnostics.pendingRefreshCount =
      static_cast<uint8_t>(pendingRefreshCount);
  diagnostics.radiusKm = centerRadiusKm;
  diagnostics.lastSuccessfulRefreshAgeMs =
      lastSuccessfulRefreshAt == 0 ? 0 : now - lastSuccessfulRefreshAt;
  diagnostics.nextRefreshInMs =
      nextAttemptAt != 0 && static_cast<long>(nextAttemptAt - now) > 0
          ? nextAttemptAt - now
          : 0;
  diagnostics.preparedFrameCount = 0;
  diagnostics.oldestFrameTime[0] = '\0';
  diagnostics.newestFrameTime[0] = '\0';
  for (size_t index = 0; index < animationFrameCount; ++index) {
    if (!preparedFrameReady[index]) continue;
    if (diagnostics.oldestFrameTime[0] == '\0')
      strlcpy(diagnostics.oldestFrameTime, preparedFrameTimes[index],
              sizeof(diagnostics.oldestFrameTime));
    strlcpy(diagnostics.newestFrameTime, preparedFrameTimes[index],
            sizeof(diagnostics.newestFrameTime));
    ++diagnostics.preparedFrameCount;
  }
  diagnostics.lastHttpStatus = lastHttpStatus;
  diagnostics.lastBatchMs = lastBatchMs;
  diagnostics.lastBatchConnections = lastBatchConnections;
  diagnostics.lastBatchRequests = lastBatchRequests;
  diagnostics.lastBatchBytes = lastBatchBytes;
  diagnostics.lastDownloadedBytes = lastDownloadedBytes;
  diagnostics.lastDecodeResult = lastDecodeResult;
  diagnostics.lastDecodedLineCount = lastDecodedLineCount;
  diagnostics.acceptedCompleteDecodeError = acceptedCompleteDecodeError;
  strlcpy(diagnostics.latestIndexFile, latestIndexFile,
          sizeof(diagnostics.latestIndexFile));
  strlcpy(diagnostics.currentFile, currentFile,
          sizeof(diagnostics.currentFile));
  strlcpy(diagnostics.message, statusMessage, sizeof(diagnostics.message));
  portEXIT_CRITICAL(&stateMux);
}
