#include "ChmiRadarService.h"

#include <HTTPClient.h>
#include <PNGdec.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <freertos/idf_additions.h>

#include <cctype>
#include <cmath>
#include <cstring>
#include <time.h>

#include "ChmiCa.h"
#include "CzechMapData.h"
#include "NetworkCoordinator.h"

namespace {
constexpr char INDEX_URL[] =
    "https://opendata.chmi.cz/meteorology/weather/radar/composite/maxz/png/";
constexpr char FILE_PREFIX[] = "pacz2gmaps3.z_max3d.";
constexpr size_t FILE_NAME_CAPACITY = 56;
constexpr size_t PNG_CAPACITY = 131072;
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
constexpr size_t DOWNLOAD_CHUNK_SIZE = 512;
constexpr unsigned long DOWNLOAD_CHUNK_PAUSE_MS = 4;

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
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;
bool active = false;
float centerLatitude = 49.1951f;
float centerLongitude = 16.6068f;
uint16_t centerRadiusKm = 50;
uint8_t requestedFrameCount = 6;
uint8_t mapOpacity = 100;
uint8_t pauseSeconds = 5;
uint16_t *displayBuffers[DISPLAY_BUFFER_COUNT] = {};
uint16_t *decodeBuffer = nullptr;
uint8_t *preparedFrames[MAX_ANIMATION_FRAME_COUNT] = {};
bool preparedFrameReady[MAX_ANIMATION_FRAME_COUNT] = {};
uint32_t preparedFrameRevisions[MAX_ANIMATION_FRAME_COUNT] = {};
char preparedFrameTimes[MAX_ANIMATION_FRAME_COUNT][6] = {};
char preparedFrameNames[MAX_ANIMATION_FRAME_COUNT][FILE_NAME_CAPACITY] = {};
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

PNG png;
uint8_t *pngBuffer = nullptr;
uint8_t *cachedPngFrames[MAX_ANIMATION_FRAME_COUNT] = {};
size_t cachedPngSizes[MAX_ANIMATION_FRAME_COUNT] = {};
size_t cachedPngCapacities[MAX_ANIMATION_FRAME_COUNT] = {};
char cachedPngNames[MAX_ANIMATION_FRAME_COUNT][FILE_NAME_CAPACITY] = {};
size_t cachedPngCount = 0;
uint8_t *pendingPreparedFrames[MAX_PENDING_REFRESH_FRAMES] = {};
uint8_t *pendingPngFrames[MAX_PENDING_REFRESH_FRAMES] = {};
size_t pendingPngSizes[MAX_PENDING_REFRESH_FRAMES] = {};
size_t pendingPngCapacities[MAX_PENDING_REFRESH_FRAMES] = {};
char pendingFrameNames[MAX_PENDING_REFRESH_FRAMES][FILE_NAME_CAPACITY] = {};
char pendingFrameTimes[MAX_PENDING_REFRESH_FRAMES][6] = {};
uint32_t pendingFrameRevisions[MAX_PENDING_REFRESH_FRAMES] = {};
size_t pendingRefreshCount = 0;
uint16_t *lineBuffer = nullptr;
size_t lineCapacity = 0;
uint16_t *decodeTarget = nullptr;
int imageWidth = 0;
int imageHeight = 0;
int sourceX[CHMI_RADAR_WIDTH] = {};
int sourceY[CHMI_RADAR_HEIGHT] = {};
int dataX1 = 0;
int dataY0 = 0;
uint16_t decodedLineCount = 0;
bool decodedLinesSequential = true;

void advanceAnimation(unsigned long now);
bool showPreparedFrame(size_t index, unsigned long now);
int firstPreparedFrame();
uint8_t rgb565ToRgb332(uint16_t color);
uint16_t nightRadarColor(uint8_t color);
void applyNightRadarPalette(uint16_t *buffer);

unsigned long millisecondsUntilNextRefreshSlot() {
  const time_t now = time(nullptr);
  if (now < VALID_TIME_THRESHOLD) return REFRESH_INTERVAL_MS;
  struct tm localTime = {};
  if (localtime_r(&now, &localTime) == nullptr) return REFRESH_INTERVAL_MS;

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
  return lroundf((longitude - LON_LEFT) * (imageWidth - 1) /
                 (LON_RIGHT - LON_LEFT));
}

int latitudeToY(float latitude) {
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
      buffer = static_cast<uint16_t *>(heap_caps_malloc(
          RADAR_PIXEL_COUNT * sizeof(uint16_t),
          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    }
    if (buffer == nullptr) return false;
  }
  if (decodeBuffer == nullptr) {
    decodeBuffer = static_cast<uint16_t *>(heap_caps_malloc(
        RADAR_PIXEL_COUNT * sizeof(uint16_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  }
  if (pngBuffer == nullptr) {
    pngBuffer = static_cast<uint8_t *>(heap_caps_malloc(
        PNG_CAPACITY, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  }
  return pngBuffer != nullptr && decodeBuffer != nullptr;
}

bool ensurePreparedFrame(size_t index) {
  if (index >= MAX_ANIMATION_FRAME_COUNT) return false;
  if (preparedFrames[index] == nullptr) {
    preparedFrames[index] = static_cast<uint8_t *>(heap_caps_malloc(
        RADAR_PIXEL_COUNT, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  }
  return preparedFrames[index] != nullptr;
}

bool cacheDownloadedPng(size_t index, size_t size, const char *fileName) {
  if (index >= MAX_ANIMATION_FRAME_COUNT || size == 0 || size > PNG_CAPACITY)
    return false;
  if (cachedPngCapacities[index] < size) {
    uint8_t *replacement = static_cast<uint8_t *>(heap_caps_malloc(
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

bool latestFileNames(char output[][FILE_NAME_CAPACITY], size_t &count,
                     uint32_t revision) {
  count = 0;
  memset(output, 0,
         MAX_ANIMATION_FRAME_COUNT * FILE_NAME_CAPACITY * sizeof(char));
  NetworkOperationGuard networkGuard(15000);
  if (!networkGuard) return false;
  WiFiClientSecure client;
  client.setCACert(CHMI_ROOT_CA);
  HTTPClient http;
  http.useHTTP10(true);
  http.setConnectTimeout(6000);
  http.setTimeout(15000);
  const String indexUrl = String(INDEX_URL) + F("?clock=") + millis();
  if (!http.begin(client, indexUrl)) return false;
  http.addHeader(F("Cache-Control"), F("no-cache"));
  const int status = http.GET();
  portENTER_CRITICAL(&stateMux);
  lastHttpStatus = status;
  lastDownloadedBytes = 0;
  currentFile[0] = '\0';
  portEXIT_CRITICAL(&stateMux);
  if (status != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  uint8_t chunk[768];
  size_t prefixMatch = 0;
  char candidate[FILE_NAME_CAPACITY] = "";
  size_t candidateLength = 0;
  size_t indexBytesRead = 0;
  int remaining = http.getSize();
  unsigned long lastDataAt = millis();
  while (remaining > 0 || remaining == -1) {
    if (!requestMatches(revision)) {
      http.end();
      return false;
    }
    const size_t available = stream->available();
    if (available == 0) {
      const unsigned long idleFor = millis() - lastDataAt;
      if (idleFor > 15000 || (!http.connected() && idleFor > 750)) break;
      delay(2);
      continue;
    }
    const size_t wanted = min(available, sizeof(chunk));
    const int bytesRead = stream->readBytes(chunk, wanted);
    if (bytesRead <= 0) break;
    indexBytesRead += static_cast<size_t>(bytesRead);
    lastDataAt = millis();
    parseIndexChunk(chunk, static_cast<size_t>(bytesRead), prefixMatch, candidate,
                    candidateLength, output, count);
    if (remaining > 0) remaining -= bytesRead;
    advanceAnimation(millis());
    delay(2);
  }
  http.end();
  portENTER_CRITICAL(&stateMux);
  lastDownloadedBytes = indexBytesRead;
  if (count > 0)
    strlcpy(latestIndexFile, output[count - 1], sizeof(latestIndexFile));
  portEXIT_CRITICAL(&stateMux);
  return count > 0;
}

bool downloadPng(const char *fileName, size_t &outputSize,
                 uint32_t revision) {
  outputSize = 0;
  NetworkOperationGuard networkGuard(15000);
  if (!networkGuard) return false;
  WiFiClientSecure client;
  client.setCACert(CHMI_ROOT_CA);
  HTTPClient http;
  http.useHTTP10(true);
  http.setConnectTimeout(6000);
  http.setTimeout(15000);
  const String url = String(INDEX_URL) + fileName;
  portENTER_CRITICAL(&stateMux);
  strlcpy(currentFile, fileName, sizeof(currentFile));
  lastDownloadedBytes = 0;
  portEXIT_CRITICAL(&stateMux);
  if (!http.begin(client, url)) return false;
  const int status = http.GET();
  portENTER_CRITICAL(&stateMux);
  lastHttpStatus = status;
  portEXIT_CRITICAL(&stateMux);
  if (status != HTTP_CODE_OK) {
    http.end();
    return false;
  }
  const int declaredSize = http.getSize();
  if (declaredSize > static_cast<int>(PNG_CAPACITY)) {
    http.end();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  int remaining = declaredSize;
  unsigned long lastDataAt = millis();
  while (outputSize < PNG_CAPACITY &&
         (remaining > 0 || remaining == -1)) {
    if (!requestMatches(revision)) {
      http.end();
      return false;
    }
    const size_t available = stream->available();
    if (available == 0) {
      const unsigned long idleFor = millis() - lastDataAt;
      if (idleFor > 15000 || (!http.connected() && idleFor > 750)) break;
      delay(2);
      continue;
    }
    const size_t capacity = PNG_CAPACITY - outputSize;
    const size_t wanted = min(min(available, capacity), DOWNLOAD_CHUNK_SIZE);
    const int count = stream->readBytes(pngBuffer + outputSize, wanted);
    if (count <= 0) break;
    outputSize += static_cast<size_t>(count);
    lastDataAt = millis();
    if (remaining > 0) remaining -= count;
    portENTER_CRITICAL(&stateMux);
    lastDownloadedBytes = outputSize;
    portEXIT_CRITICAL(&stateMux);
    advanceAnimation(millis());
    delay(DOWNLOAD_CHUNK_PAUSE_MS);
  }
  http.end();
  if (declaredSize >= 0 && outputSize != static_cast<size_t>(declaredSize))
    return false;
  static const uint8_t signature[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a,
                                      '\n'};
  return outputSize >= sizeof(signature) &&
         memcmp(pngBuffer, signature, sizeof(signature)) == 0;
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
  png.getLineAsRGB565(draw, lineBuffer, PNG_RGB565_LITTLE_ENDIAN, 0x00000000);
  for (int targetY = 0; targetY < CHMI_RADAR_HEIGHT; ++targetY) {
    if (sourceY[targetY] != draw->y) continue;
    uint16_t *row = decodeTarget + targetY * CHMI_RADAR_WIDTH;
    const long dy = targetY - CHMI_RADAR_HEIGHT / 2;
    for (int targetX = 0; targetX < CHMI_RADAR_WIDTH; ++targetX) {
      const long dx = targetX - CHMI_RADAR_WIDTH / 2;
      if (dx * dx + dy * dy > 238L * 238L) continue;
      const int source = sourceX[targetX];
      if (source >= 0 && source < imageWidth && source <= dataX1 &&
          draw->y >= dataY0) {
        row[targetX] = lineBuffer[source];
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

void setMapPixel(uint16_t *buffer, int x, int y, uint16_t color,
                 uint8_t opacity) {
  if (x >= 0 && x < CHMI_RADAR_WIDTH && y >= 0 && y < CHMI_RADAR_HEIGHT) {
    uint16_t &pixel = buffer[y * CHMI_RADAR_WIDTH + x];
    pixel = blendRgb565(pixel, color, opacity);
  }
}

uint8_t lineOutCode(int x, int y) {
  return (x < 0 ? 1 : 0) | (x >= CHMI_RADAR_WIDTH ? 2 : 0) |
         (y < 0 ? 4 : 0) | (y >= CHMI_RADAR_HEIGHT ? 8 : 0);
}

void drawMapLine(uint16_t *buffer, int x0, int y0, int x1, int y1,
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

void fillMapRect(uint16_t *buffer, int x, int y, int width, int height,
                 uint16_t color, uint8_t opacity) {
  for (int row = y; row < y + height; ++row)
    for (int column = x; column < x + width; ++column)
      setMapPixel(buffer, column, row, color, opacity);
}

void drawMapText(uint16_t *buffer, int x, int y, const char *text,
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

void drawMapOverlay(uint16_t *buffer, float markerLatitude,
                    float markerLongitude, uint16_t radiusKm, int cropX1,
                    int cropX2, int cropY1, int cropY2, uint8_t opacity) {
  if (opacity == 0) return;
  constexpr uint16_t borderColor = 0xbdf7;
  constexpr uint16_t cityColor = 0x07ff;
  int previousX = 0;
  int previousY = 0;
  for (size_t index = 0;
       index < sizeof(CZECH_MAP_BORDER) / sizeof(CZECH_MAP_BORDER[0]);
       ++index) {
    const float longitude = CZECH_MAP_LON_ORIGIN +
                            CZECH_MAP_BORDER[index].longitude *
                                CZECH_MAP_COORD_SCALE;
    const float latitude = CZECH_MAP_LAT_ORIGIN +
                           CZECH_MAP_BORDER[index].latitude *
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

  MapLabelBox occupied[sizeof(CZECH_MAP_CITIES) /
                       sizeof(CZECH_MAP_CITIES[0])] = {};
  size_t occupiedCount = 0;
  const bool showFullNames = radiusKm > 0 && radiusKm <= 50;
  for (uint8_t tier = 1; tier <= 2; ++tier) {
    for (const CzechMapCity &city : CZECH_MAP_CITIES) {
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

void drawDisplayRing(uint16_t *buffer) {
  const int centerX = CHMI_RADAR_WIDTH / 2;
  const int centerY = CHMI_RADAR_HEIGHT / 2;
  constexpr uint16_t gray = 0x4208;
  for (int degree = 0; degree < 360; ++degree) {
    const float angle = degree * 0.017453292519943295f;
    const int x = centerX + lroundf(cosf(angle) * 238.0f);
    const int y = centerY + lroundf(sinf(angle) * 238.0f);
    if (x >= 0 && x < CHMI_RADAR_WIDTH && y >= 0 && y < CHMI_RADAR_HEIGHT)
      buffer[y * CHMI_RADAR_WIDTH + x] = gray;
  }
}

void radarProjectionBounds(float latitude, float longitude, uint16_t radiusKm,
                           int &cropX1, int &cropX2, int &cropY1,
                           int &cropY2) {
  const uint16_t projectionRadiusKm =
      radiusKm == 0 ? WHOLE_COUNTRY_RADIUS_KM : radiusKm;
  if (radiusKm == 0) {
    latitude = WHOLE_COUNTRY_LATITUDE;
    longitude = WHOLE_COUNTRY_LONGITUDE;
  }
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
  imageWidth = RADAR_SOURCE_WIDTH;
  imageHeight = RADAR_SOURCE_HEIGHT;
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

bool decodeRadar(const uint8_t *pngData, size_t pngSize, float latitude,
                 float longitude, uint16_t radiusKm, uint8_t mapOpacityValue,
                 uint16_t *target) {
  if (pngData == nullptr ||
      png.openRAM(const_cast<uint8_t *>(pngData), static_cast<int>(pngSize),
                  drawDecodedLine) !=
      PNG_SUCCESS)
    return false;
  imageWidth = png.getWidth();
  imageHeight = png.getHeight();
  if (imageWidth <= 0 || imageHeight <= 0 || imageWidth > 2048) {
    png.close();
    return false;
  }
  if (lineCapacity < static_cast<size_t>(imageWidth)) {
    if (lineBuffer != nullptr) heap_caps_free(lineBuffer);
    lineBuffer = static_cast<uint16_t *>(heap_caps_malloc(
        imageWidth * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    lineCapacity = lineBuffer == nullptr ? 0 : imageWidth;
  }
  if (lineBuffer == nullptr) {
    png.close();
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
    sourceX[x] = cropX1 + static_cast<int64_t>(x) * (cropX2 - cropX1 + 1) /
                              CHMI_RADAR_WIDTH;
  }
  for (int y = 0; y < CHMI_RADAR_HEIGHT; ++y) {
    sourceY[y] = cropY1 + static_cast<int64_t>(y) * (cropY2 - cropY1 + 1) /
                              CHMI_RADAR_HEIGHT;
  }
  dataX1 = longitudeToX(LON_DATA_RIGHT);
  dataY0 = latitudeToY(LAT_DATA_TOP);
  memset(target, 0,
         CHMI_RADAR_WIDTH * CHMI_RADAR_HEIGHT * sizeof(uint16_t));
  decodedLineCount = 0;
  decodedLinesSequential = true;
  decodeTarget = target;
  const int result = png.decode(nullptr, 0);
  decodeTarget = nullptr;
  png.close();
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
  localtime_r(&epoch, &localTime);
  snprintf(output, 6, "%02d:%02d", localTime.tm_hour, localTime.tm_min);
}

bool currentRequest(float &latitude, float &longitude, uint16_t &radiusKm,
                    uint8_t &wantedFrameCount, uint8_t &mapOpacityValue,
                    uint32_t &revision) {
  portENTER_CRITICAL(&stateMux);
  const bool requested = active;
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

bool packDecodedFrame(size_t index) {
  if (!ensurePreparedFrame(index)) return false;
  uint8_t *target = preparedFrames[index];
  for (size_t pixel = 0; pixel < RADAR_PIXEL_COUNT; ++pixel)
    target[pixel] = rgb565ToRgb332(decodeBuffer[pixel]);
  return true;
}

bool packPendingFrame(size_t slot) {
  if (slot >= MAX_PENDING_REFRESH_FRAMES) return false;
  if (pendingPreparedFrames[slot] == nullptr) {
    pendingPreparedFrames[slot] = static_cast<uint8_t *>(heap_caps_malloc(
        RADAR_PIXEL_COUNT, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  }
  if (pendingPreparedFrames[slot] == nullptr) return false;
  for (size_t pixel = 0; pixel < RADAR_PIXEL_COUNT; ++pixel)
    pendingPreparedFrames[slot][pixel] = rgb565ToRgb332(decodeBuffer[pixel]);
  return true;
}

bool cachePendingPng(size_t slot, size_t size) {
  if (slot >= MAX_PENDING_REFRESH_FRAMES || size == 0 || size > PNG_CAPACITY)
    return false;
  if (pendingPngCapacities[slot] < size) {
    uint8_t *replacement = static_cast<uint8_t *>(heap_caps_malloc(
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
  if (!decodeRadar(pngData, pngSize, latitude, longitude, radiusKm,
                   mapOpacityValue, decodeBuffer) ||
      !packDecodedFrame(index) || !requestMatches(revision)) {
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
  portENTER_CRITICAL(&stateMux);
  const bool valid = active && revision == requestRevision;
  if (valid) {
    ready = true;
    activeRadiusKm = radiusKm;
  }
  portEXIT_CRITICAL(&stateMux);
  const bool shown = valid && showPreparedFrame(index, millis());
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
    animationPause = true;
    animationPauseStartedAt = now;
    lastAnimationStepAt = now;
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
      setStatus(false, "Snimek CHMU se nepodarilo pripravit");
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

bool loadAnimation(float latitude, float longitude, uint16_t radiusKm,
                   uint8_t wantedFrameCount, uint8_t mapOpacityValue,
                   uint32_t revision) {
  if (WiFi.status() != WL_CONNECTED) {
    setStatus(false, "Wi-Fi neni pripojena");
    return false;
  }
  if (!ensureBuffers()) {
    setStatus(false, "Nedostatek pameti pro radar");
    return false;
  }
  setStatus(true, "Obnovuji radar CHMU...");
  char latestNames[MAX_ANIMATION_FRAME_COUNT][FILE_NAME_CAPACITY] = {};
  size_t latestCount = 0;
  if (!latestFileNames(latestNames, latestCount, revision)) {
    setStatus(false, "Seznam CHMU se nepodarilo nacist");
    return false;
  }
  const size_t selectedCount =
      min(latestCount, static_cast<size_t>(wantedFrameCount));
  const size_t selectedStart = latestCount - selectedCount;
  if (selectedCount == 0) return false;

  size_t loadedCount = 0;
  bool canResume = animationFrameCount == selectedCount &&
                   cachedPngCount > 0 && cachedPngCount < selectedCount;
  if (canResume) {
    for (size_t index = 0; index < selectedCount; ++index) {
      if (!preparedFrameReady[index]) continue;
      if (strcmp(latestNames[selectedStart + index], cachedPngNames[index]) !=
              0 ||
          strcmp(cachedPngNames[index], preparedFrameNames[index]) != 0) {
        canResume = false;
        break;
      }
      ++loadedCount;
    }
    if (loadedCount != cachedPngCount) canResume = false;
  }
  if (canResume) {
    portENTER_CRITICAL(&stateMux);
    preparationInProgress = true;
    animationPause = false;
    lastProgressiveFrameShownAt = 0;
    portEXIT_CRITICAL(&stateMux);
  } else {
    beginProgressivePreparation(selectedCount);
    portENTER_CRITICAL(&stateMux);
    cachedPngCount = 0;
    portEXIT_CRITICAL(&stateMux);
    loadedCount = 0;
  }

  const auto prepareMissingFrame = [&](size_t index) {
    const char *fileName = latestNames[selectedStart + index];
    if (preparedFrameReady[index] &&
        preparedFrameRevisions[index] == revision)
      return true;

    // Rychlé zavření radaru zneplatní rozpracovanou revizi, ale již stažené
    // PNG ponecháváme v cache. Po návratu je musíme znovu promítnout do
    // aktuálního výřezu a označit novou revizí; jinak showPreparedFrame()
    // starý snímek odmítne a na displeji zůstane pouze podkladová mapa.
    if (index < cachedPngCount && cachedPngFrames[index] != nullptr &&
        cachedPngSizes[index] > 0 &&
        strcmp(cachedPngNames[index], fileName) == 0) {
      if (!prepareFrame(index, cachedPngFrames[index], cachedPngSizes[index],
                        cachedPngNames[index], latitude, longitude, radiusKm,
                        mapOpacityValue, revision)) {
        setStatus(false, "Snimek CHMU se nepodarilo pripravit");
        return false;
      }
      return true;
    }

    size_t pngSize = 0;
    if (!downloadPngWithRetry(fileName, pngSize, revision)) {
      setStatus(false, "Snimek CHMU se nepodarilo stahnout");
      return false;
    }
    if (!cacheDownloadedPng(index, pngSize, fileName)) {
      setStatus(false, "Nedostatek pameti pro radar");
      return false;
    }
    if (!prepareFrame(index, cachedPngFrames[index], cachedPngSizes[index],
                      cachedPngNames[index], latitude, longitude, radiusKm,
                      mapOpacityValue, revision)) {
      setStatus(false, "Snimek CHMU se nepodarilo pripravit");
      return false;
    }
    ++loadedCount;
    portENTER_CRITICAL(&stateMux);
    cachedPngCount = loadedCount;
    ready = true;
    activeRadiusKm = radiusKm;
    portEXIT_CRITICAL(&stateMux);
    return true;
  };

  for (size_t index = 0; index < selectedCount; ++index) {
    if (!prepareMissingFrame(index)) return false;
    if (!showProgressivelyPreparedFrame(index, radiusKm, revision)) {
      setStatus(false, "Snímek CHMU se nepodařilo zobrazit");
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
  setStatus(true, "Obnovuji radar CHMU...");
  char latestNames[MAX_ANIMATION_FRAME_COUNT][FILE_NAME_CAPACITY] = {};
  size_t latestCount = 0;
  if (!latestFileNames(latestNames, latestCount, revision)) {
    setStatus(false, "Seznam CHMU se nepodarilo nacist");
    return false;
  }
  const size_t selectedCount =
      min(latestCount, static_cast<size_t>(wantedFrameCount));
  const size_t selectedStart = latestCount - selectedCount;
  if (selectedCount != animationFrameCount || selectedCount != cachedPngCount ||
      selectedCount == 0) {
    return loadAnimation(latitude, longitude, radiusKm, wantedFrameCount,
                         mapOpacityValue, revision);
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
                         mapOpacityValue, revision);
  }

  pendingRefreshCount = 0;
  const size_t firstNew = selectedCount - shift;
  for (size_t slot = 0; slot < shift; ++slot) {
    const char *fileName = latestNames[selectedStart + firstNew + slot];
    size_t pngSize = 0;
    if (!downloadPngWithRetry(fileName, pngSize, revision) ||
        !cachePendingPng(slot, pngSize) ||
        !decodeRadar(pngBuffer, pngSize, latitude, longitude, radiusKm,
                     mapOpacityValue, decodeBuffer) ||
        !packPendingFrame(slot) || !requestMatches(revision)) {
      setStatus(false, "Nove snimky CHMU se nepodarilo pripravit");
      pendingRefreshCount = 0;
      return false;
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
  portEXIT_CRITICAL(&stateMux);
  if (animationFrameCount == 1) {
    commitPendingRefresh();
    return showPreparedFrame(0, millis());
  }
  if (atBoundary) commitPendingRefresh();
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
    target[pixel] = nightVisual ? nightRadarColor(source[pixel])
                                : rgb332ToRgb565(source[pixel]);
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

void advanceAnimation(unsigned long now) {
  portENTER_CRITICAL(&stateMux);
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
  if (!canAnimate || preparing) return;
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
    haveFrames = ready && displayedFrame >= 0;
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
    if (reloadNow || !haveFrames ||
        static_cast<long>(now - nextAttemptAt) >= 0) {
      const bool fullPreparation = reloadNow || !haveFrames;
      const bool success =
          fullPreparation
              ? loadAnimation(latitude, longitude, radiusKm, wantedFrameCount,
                              mapOpacityValue, revision)
              : refreshLatestFrame(latitude, longitude, radiusKm,
                                   wantedFrameCount, mapOpacityValue, revision);
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

void chmiRadarServiceBegin() {
  if (taskHandle != nullptr) return;
  xTaskCreatePinnedToCoreWithCaps(
      radarTask, "chmi-radar", 12288, nullptr, 1, &taskHandle, 0,
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void chmiRadarServiceSetActive(bool requested, float latitude, float longitude,
                               uint16_t radiusKm, uint8_t frameCount,
                               uint8_t mapOpacityValue,
                               uint8_t pauseSecondsValue) {
  frameCount = constrain(frameCount, static_cast<uint8_t>(1),
                         static_cast<uint8_t>(MAX_ANIMATION_FRAME_COUNT));
  mapOpacityValue = constrain(mapOpacityValue, static_cast<uint8_t>(0),
                              static_cast<uint8_t>(100));
  pauseSecondsValue = constrain(pauseSecondsValue, static_cast<uint8_t>(0),
                                static_cast<uint8_t>(30));
  portENTER_CRITICAL(&stateMux);
  const bool wasActive = active;
  const bool projectionChanged =
      fabsf(centerLatitude - latitude) > 0.00001f ||
      fabsf(centerLongitude - longitude) > 0.00001f ||
      centerRadiusKm != radiusKm || mapOpacity != mapOpacityValue;
  const bool frameCountChanged = requestedFrameCount != frameCount;
  bool completePngCache = cachedPngCount == requestedFrameCount;
  if (completePngCache) {
    for (size_t index = 0; index < cachedPngCount; ++index)
      if (cachedPngFrames[index] == nullptr || cachedPngSizes[index] == 0 ||
          cachedPngNames[index][0] == '\0') {
        completePngCache = false;
        break;
      }
  }
  bool completePreparedCache =
      ready && completePngCache &&
      animationFrameCount == requestedFrameCount;
  if (completePreparedCache) {
    for (size_t index = 0; index < animationFrameCount; ++index)
      if (!preparedFrameReady[index]) {
        completePreparedCache = false;
        break;
      }
  }
  const bool freshPngCache =
      completePngCache && !frameCountChanged &&
      lastSuccessfulRefreshAt != 0 &&
      millis() - lastSuccessfulRefreshAt < REFRESH_INTERVAL_MS;
  const bool freshPreparedCache =
      completePreparedCache && !projectionChanged && !frameCountChanged &&
      freshPngCache;
  active = requested;
  centerLatitude = latitude;
  centerLongitude = longitude;
  centerRadiusKm = radiusKm;
  requestedFrameCount = frameCount;
  mapOpacity = mapOpacityValue;
  pauseSeconds = pauseSecondsValue;
  if (!requested && wasActive) {
    ++requestRevision;
    rebuildFromCacheRequested = false;
    reloadRequested = false;
    showBaseMapRequested = false;
    restartAnimationRequested = false;
    preparationInProgress = false;
  } else if (requested && !wasActive) {
    if (freshPreparedCache) {
      for (size_t index = 0; index < animationFrameCount; ++index)
        if (preparedFrameReady[index])
          preparedFrameRevisions[index] = requestRevision;
      restartAnimationRequested = true;
      showBaseMapRequested = false;
      rebuildFromCacheRequested = false;
      reloadRequested = false;
    } else {
      ++requestRevision;
      showBaseMapRequested = true;
      restartAnimationRequested = false;
      rebuildFromCacheRequested = freshPngCache;
      reloadRequested = !freshPngCache;
      nextAttemptAt = 0;
    }
  } else if (requested && (projectionChanged || frameCountChanged)) {
    ++requestRevision;
    showBaseMapRequested = true;
    restartAnimationRequested = false;
    rebuildFromCacheRequested = projectionChanged && freshPngCache;
    reloadRequested = !rebuildFromCacheRequested;
    nextAttemptAt = 0;
  }
  portEXIT_CRITICAL(&stateMux);
  if (requested && taskHandle != nullptr) xTaskNotifyGive(taskHandle);
}

void chmiRadarServiceSetRedNightMode(bool enabled) {
  portENTER_CRITICAL(&stateMux);
  if (redNightMode == enabled) {
    portEXIT_CRITICAL(&stateMux);
    return;
  }
  redNightMode = enabled;
  nightVisualRedrawRequested = active && displayedFrame != -1;
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
  diagnostics.active = active;
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
