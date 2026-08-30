#include "ConfigurationWeb.h"

#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>

#include <cmath>
#include <cctype>

#include "ConfigurationPage.h"
#include "ConfigurationLocalization.h"
#include "ChmiRadarService.h"
#include "DiagnosticPage.h"
#include "Display_ST7701.h"
#include "FirmwareBuild.h"
#include "FirmwareHubCa.h"
#include "FirmwareUpdateService.h"
#include "HomeAssistantConnectionPolicy.h"
#include "LoginPage.h"
#include "NetworkCoordinator.h"
#include "NetworkDiagnostics.h"

namespace {
constexpr size_t MAX_POST_BODY_BYTES = 16 * 1024;
constexpr size_t MAX_POST_KEY_BYTES = 64;
constexpr size_t MAX_POST_VALUE_BYTES = 1024;

class BoundedWebServer : public WebServer {
 public:
  using WebServer::WebServer;
  using WebServer::arg;
  using WebServer::hasArg;

  void beginBoundedPostSupport() {
    if (postBody_ == nullptr) {
      postBody_ = static_cast<char *>(heap_caps_malloc(
          MAX_POST_BODY_BYTES + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    }
  }

  void captureRawPost(HTTPRaw &raw) {
    if (raw.status == RAW_START) {
      // WebServer čte každý raw blok do celé velikosti HTTP_RAW_BUFLEN a u
      // posledního kratšího bloku by jinak čekal výchozích pět sekund.
      // Lokální prohlížeč už v této fázi tělo odesílá, krátký timeout proto
      // odstraní prodlevu a současně omezuje pomalé POST požadavky.
      client().setTimeout(100);
      rawPostActive_ = true;
      postBodyLength_ = 0;
      postBodyReady_ = false;
      postBodyTooLarge_ = clientContentLength() > MAX_POST_BODY_BYTES;
      postBodyMalformed_ = false;
      if (postBody_ != nullptr) postBody_[0] = '\0';
      return;
    }
    if (raw.status == RAW_WRITE) {
      if (postBodyTooLarge_ || postBody_ == nullptr) return;
      if (raw.currentSize > MAX_POST_BODY_BYTES - postBodyLength_) {
        postBodyTooLarge_ = true;
        return;
      }
      memcpy(postBody_ + postBodyLength_, raw.buf, raw.currentSize);
      postBodyLength_ += raw.currentSize;
      return;
    }
    if (raw.status == RAW_END) {
      if (!postBodyTooLarge_ && postBody_ != nullptr) {
        postBody_[postBodyLength_] = '\0';
        postBodyMalformed_ = !validRawBodyShape();
        postBodyReady_ = !postBodyMalformed_;
      }
      return;
    }
    postBodyReady_ = false;
  }

  bool postBodyAccepted() const {
    return rawPostActive_ && postBodyReady_ && !postBodyTooLarge_;
  }

  bool postBodyTooLarge() const { return postBodyTooLarge_; }
  bool postBodyMalformed() const { return postBodyMalformed_; }

  String arg(const String &name) {
    if (!rawPostActive_) return WebServer::arg(name);
    size_t valueStart = 0;
    size_t valueLength = 0;
    if (!findRawArgument(name, valueStart, valueLength)) return String();
    return WebServer::urlDecode(String(postBody_ + valueStart, valueLength));
  }

  bool hasArg(const String &name) {
    if (!rawPostActive_) return WebServer::hasArg(name);
    size_t valueStart = 0;
    size_t valueLength = 0;
    return findRawArgument(name, valueStart, valueLength);
  }

 private:
  bool validRawBodyShape() const {
    size_t fieldStart = 0;
    while (fieldStart < postBodyLength_) {
      size_t fieldEnd = fieldStart;
      while (fieldEnd < postBodyLength_ && postBody_[fieldEnd] != '&')
        ++fieldEnd;
      size_t equalsAt = fieldStart;
      while (equalsAt < fieldEnd && postBody_[equalsAt] != '=') ++equalsAt;
      if (equalsAt == fieldEnd || equalsAt - fieldStart > MAX_POST_KEY_BYTES ||
          fieldEnd - equalsAt - 1 > MAX_POST_VALUE_BYTES) {
        return false;
      }
      fieldStart = fieldEnd + 1;
    }
    return true;
  }

  bool findRawArgument(const String &name, size_t &valueStart,
                       size_t &valueLength) const {
    if (!postBodyAccepted()) return false;
    size_t fieldStart = 0;
    while (fieldStart <= postBodyLength_) {
      size_t fieldEnd = fieldStart;
      while (fieldEnd < postBodyLength_ && postBody_[fieldEnd] != '&')
        ++fieldEnd;
      size_t equalsAt = fieldStart;
      while (equalsAt < fieldEnd && postBody_[equalsAt] != '=') ++equalsAt;
      if (equalsAt < fieldEnd) {
        const String encodedName(postBody_ + fieldStart, equalsAt - fieldStart);
        if (WebServer::urlDecode(encodedName) == name) {
          valueStart = equalsAt + 1;
          valueLength = fieldEnd - valueStart;
          return true;
        }
      }
      if (fieldEnd == postBodyLength_) break;
      fieldStart = fieldEnd + 1;
    }
    return false;
  }

  char *postBody_ = nullptr;
  size_t postBodyLength_ = 0;
  bool rawPostActive_ = false;
  bool postBodyReady_ = false;
  bool postBodyTooLarge_ = false;
  bool postBodyMalformed_ = false;
};

BoundedWebServer server(80);
ClockConfigLoadCallback configLoadCallback = nullptr;
ClockConfigSaveCallback configSaveCallback = nullptr;
ConfigurationWebStatusCallback webStatusCallback = nullptr;
SunTransitionTimesCallback sunTransitionTimesCallback = nullptr;
HomeAssistantRefreshCallback homeAssistantRefreshCallback = nullptr;
DayNightStatusCallback currentDayNightStatusCallback = nullptr;
DisplayPowerCallback currentDisplayPowerCallback = nullptr;
DisplayPowerStatusCallback currentDisplayPowerStatusCallback = nullptr;
RadarRangeStateCallback currentRadarRangeStateCallback = nullptr;
RadarRangePreviewCallback currentRadarRangePreviewCallback = nullptr;
ClockAppearanceStateCallback currentAppearanceStateCallback = nullptr;
ClockAppearanceChangeCallback currentAppearancePreviewCallback = nullptr;
ClockAppearanceChangeCallback currentAppearanceSaveCallback = nullptr;
ClockConfig configBuffer;
constexpr unsigned long WEB_AVAILABILITY_MS = 10UL * 60UL * 1000UL;
bool webActive = false;
unsigned long webAvailableUntil = 0;
ConfigurationWebMode selectedWebMode = CONFIGURATION_WEB_ALWAYS;
constexpr char WEB_PREFS_NAMESPACE[] = "web-mode";
constexpr char WEB_PREFS_KEY[] = "mode";
constexpr char CONTROL_PREFS_NAMESPACE[] = "control-api";
constexpr char CONTROL_PREFS_KEY[] = "secret";
constexpr size_t CONTROL_SECRET_LENGTH = 32;
String controlSecret;
constexpr char WEB_AUTH_PREFS_NAMESPACE[] = "web-auth";
constexpr char WEB_AUTH_PREFS_KEY[] = "credential";
constexpr uint32_t WEB_PASSWORD_MAGIC = 0x57485058;
constexpr size_t WEB_PASSWORD_SALT_SIZE = 16;
constexpr size_t WEB_PASSWORD_HASH_SIZE = 32;
constexpr unsigned int WEB_PASSWORD_ITERATIONS = 5000;
constexpr size_t WEB_SESSION_COUNT = 4;
constexpr unsigned long WEB_SESSION_LIFETIME_MS = 8UL * 60UL * 60UL * 1000UL;
constexpr char WEB_SESSION_COOKIE[] = "wh_session";

struct WebPasswordRecord {
  uint32_t magic = 0;
  uint8_t salt[WEB_PASSWORD_SALT_SIZE] = {};
  uint8_t hash[WEB_PASSWORD_HASH_SIZE] = {};
  uint32_t checksum = 0;
};

struct WebSession {
  String token;
  unsigned long expiresAt = 0;
};

WebPasswordRecord webPasswordRecord;
bool webPasswordEnabled = false;
WebSession webSessions[WEB_SESSION_COUNT];
size_t nextWebSessionSlot = 0;
uint8_t failedLoginAttempts = 0;
unsigned long loginBlockedUntil = 0;

uint32_t bytesChecksum(const uint8_t *bytes, size_t size) {
  uint32_t hash = 2166136261u;
  for (size_t index = 0; index < size; ++index) {
    hash ^= bytes[index];
    hash *= 16777619u;
  }
  return hash;
}

uint32_t webPasswordChecksum(const WebPasswordRecord &record) {
  return bytesChecksum(reinterpret_cast<const uint8_t *>(&record),
                       offsetof(WebPasswordRecord, checksum));
}

bool constantTimeEqual(const uint8_t *left, const uint8_t *right,
                       size_t length) {
  uint8_t difference = 0;
  for (size_t index = 0; index < length; ++index)
    difference |= left[index] ^ right[index];
  return difference == 0;
}

bool validWebPasswordLength(const String &password) {
  size_t characterCount = 0;
  for (size_t index = 0; index < password.length(); ++index) {
    if ((static_cast<uint8_t>(password[index]) & 0xC0) != 0x80)
      ++characterCount;
  }
  return characterCount >= 6 && characterCount <= 20;
}

bool deriveWebPassword(const String &password, const uint8_t *salt,
                       uint8_t *output) {
  return mbedtls_pkcs5_pbkdf2_hmac_ext(
             MBEDTLS_MD_SHA256,
             reinterpret_cast<const unsigned char *>(password.c_str()),
             password.length(), salt, WEB_PASSWORD_SALT_SIZE,
             WEB_PASSWORD_ITERATIONS, WEB_PASSWORD_HASH_SIZE, output) == 0;
}

void initializeWebPassword() {
  webPasswordRecord = WebPasswordRecord{};
  webPasswordEnabled = false;
  Preferences preferences;
  if (!preferences.begin(WEB_AUTH_PREFS_NAMESPACE, true, "clockcfg")) return;
  const size_t storedSize = preferences.getBytesLength(WEB_AUTH_PREFS_KEY);
  const bool loaded =
      storedSize == sizeof(webPasswordRecord) &&
      preferences.getBytes(WEB_AUTH_PREFS_KEY, &webPasswordRecord,
                           sizeof(webPasswordRecord)) == sizeof(webPasswordRecord);
  preferences.end();
  webPasswordEnabled =
      loaded && webPasswordRecord.magic == WEB_PASSWORD_MAGIC &&
      webPasswordRecord.checksum == webPasswordChecksum(webPasswordRecord);
  if (!webPasswordEnabled) webPasswordRecord = WebPasswordRecord{};
}

bool persistWebPassword(const String &password) {
  WebPasswordRecord candidate;
  candidate.magic = WEB_PASSWORD_MAGIC;
  esp_fill_random(candidate.salt, sizeof(candidate.salt));
  if (!deriveWebPassword(password, candidate.salt, candidate.hash)) return false;
  candidate.checksum = webPasswordChecksum(candidate);
  Preferences preferences;
  if (!preferences.begin(WEB_AUTH_PREFS_NAMESPACE, false, "clockcfg"))
    return false;
  const bool saved =
      preferences.putBytes(WEB_AUTH_PREFS_KEY, &candidate, sizeof(candidate)) ==
      sizeof(candidate);
  preferences.end();
  if (!saved) return false;
  webPasswordRecord = candidate;
  webPasswordEnabled = true;
  return true;
}

bool eraseWebPassword() {
  Preferences preferences;
  if (!preferences.begin(WEB_AUTH_PREFS_NAMESPACE, false, "clockcfg"))
    return false;
  const bool removed = preferences.remove(WEB_AUTH_PREFS_KEY);
  preferences.end();
  if (!removed) return false;
  webPasswordRecord = WebPasswordRecord{};
  webPasswordEnabled = false;
  return true;
}

bool webPasswordMatches(const String &password) {
  if (!webPasswordEnabled || !validWebPasswordLength(password)) return false;
  uint8_t candidate[WEB_PASSWORD_HASH_SIZE];
  if (!deriveWebPassword(password, webPasswordRecord.salt, candidate))
    return false;
  return constantTimeEqual(candidate, webPasswordRecord.hash,
                           sizeof(candidate));
}

String randomHex(size_t byteCount) {
  static constexpr char HEX_DIGITS[] = "0123456789abcdef";
  String result;
  result.reserve(byteCount * 2);
  for (size_t index = 0; index < byteCount; ++index) {
    const uint8_t value = static_cast<uint8_t>(esp_random());
    result += HEX_DIGITS[value >> 4];
    result += HEX_DIGITS[value & 0x0F];
  }
  return result;
}

bool deadlinePending(unsigned long deadline) {
  return deadline != 0 && static_cast<long>(deadline - millis()) > 0;
}

String requestCookie(const char *name) {
  const String cookies = server.header("Cookie");
  const String prefix = String(name) + '=';
  int start = 0;
  while (start < static_cast<int>(cookies.length())) {
    while (start < static_cast<int>(cookies.length()) &&
           (cookies[start] == ' ' || cookies[start] == ';')) ++start;
    const int end = cookies.indexOf(';', start);
    const int itemEnd = end < 0 ? cookies.length() : end;
    if (cookies.substring(start, start + prefix.length()) == prefix)
      return cookies.substring(start + prefix.length(), itemEnd);
    if (end < 0) break;
    start = end + 1;
  }
  return String();
}

bool webSessionAuthenticated() {
  if (!webPasswordEnabled) return true;
  const String token = requestCookie(WEB_SESSION_COOKIE);
  if (token.length() != 64) return false;
  for (WebSession &session : webSessions) {
    if (!deadlinePending(session.expiresAt)) {
      session.token = "";
      session.expiresAt = 0;
      continue;
    }
    if (session.token.length() == token.length() &&
        constantTimeEqual(
            reinterpret_cast<const uint8_t *>(session.token.c_str()),
            reinterpret_cast<const uint8_t *>(token.c_str()), token.length()))
      return true;
  }
  return false;
}

void clearWebSessions() {
  for (WebSession &session : webSessions) session = WebSession{};
  nextWebSessionSlot = 0;
}

void issueWebSession() {
  WebSession &session = webSessions[nextWebSessionSlot];
  nextWebSessionSlot = (nextWebSessionSlot + 1) % WEB_SESSION_COUNT;
  session.token = randomHex(32);
  session.expiresAt = millis() + WEB_SESSION_LIFETIME_MS;
  String cookie = String(WEB_SESSION_COOKIE) + '=' + session.token +
                  F("; Path=/; HttpOnly; SameSite=Strict; Max-Age=28800");
  server.sendHeader(F("Set-Cookie"), cookie);
}

void expireWebSessionCookie() {
  server.sendHeader(F("Set-Cookie"),
                    String(WEB_SESSION_COOKIE) +
                        F("=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0"));
}

bool validControlSecret(const String &value) {
  if (value.length() != CONTROL_SECRET_LENGTH) return false;
  for (size_t i = 0; i < value.length(); ++i) {
    const char character = value[i];
    if (!((character >= '0' && character <= '9') ||
          (character >= 'a' && character <= 'f'))) return false;
  }
  return true;
}

String generateControlSecret() {
  static constexpr char HEX_DIGITS[] = "0123456789abcdef";
  String result;
  result.reserve(CONTROL_SECRET_LENGTH);
  for (size_t i = 0; i < CONTROL_SECRET_LENGTH / 8; ++i) {
    const uint32_t randomValue = esp_random();
    for (int shift = 28; shift >= 0; shift -= 4) {
      result += HEX_DIGITS[(randomValue >> shift) & 0x0F];
    }
  }
  return result;
}

void initializeControlSecret() {
  Preferences preferences;
  if (!preferences.begin(CONTROL_PREFS_NAMESPACE, false, "clockcfg")) return;
  controlSecret = preferences.getString(CONTROL_PREFS_KEY, "");
  if (!validControlSecret(controlSecret)) {
    controlSecret = generateControlSecret();
    if (preferences.putString(CONTROL_PREFS_KEY, controlSecret) !=
        controlSecret.length()) controlSecret = "";
  }
  preferences.end();
}

bool controlSecretMatches(const String &candidate) {
  if (controlSecret.isEmpty() || candidate.length() != controlSecret.length())
    return false;
  uint8_t difference = 0;
  for (size_t i = 0; i < controlSecret.length(); ++i) {
    difference |= static_cast<uint8_t>(candidate[i] ^ controlSecret[i]);
  }
  return difference == 0;
}

void notifyWebStatus() {
  if (webStatusCallback != nullptr) webStatusCallback(webActive);
}

void extendWebAvailability() {
  if (!webActive) return;
  if (selectedWebMode == CONFIGURATION_WEB_ALWAYS) {
    webAvailableUntil = ULONG_MAX;
    return;
  }
  webAvailableUntil = millis() + WEB_AVAILABILITY_MS;
}

void unlockConfiguration(bool resetDeadline) {
  if (!webActive) {
    webActive = true;
    notifyWebStatus();
  }
  if (resetDeadline) extendWebAvailability();
}

void lockConfiguration() {
  if (!webActive) return;
  webActive = false;
  webAvailableUntil = 0;
  notifyWebStatus();
}

bool persistWebMode(ConfigurationWebMode mode) {
  Preferences preferences;
  if (!preferences.begin(WEB_PREFS_NAMESPACE, false, "clockcfg")) return false;
  const bool saved = preferences.putUChar(WEB_PREFS_KEY, mode) == 1;
  preferences.end();
  return saved;
}

void applyWebMode(ConfigurationWebMode mode) {
  selectedWebMode = mode;
  if (mode == CONFIGURATION_WEB_DISABLED) lockConfiguration();
  else unlockConfiguration(true);
}

String jsonEscape(const char *value) {
  String result;
  if (value == nullptr) return result;
  result.reserve(strlen(value) + 8);
  for (const char *cursor = value; *cursor != '\0'; ++cursor) {
    switch (*cursor) {
      case '\\': result += F("\\\\"); break;
      case '"': result += F("\\\""); break;
      case '\n': result += F("\\n"); break;
      case '\r': result += F("\\r"); break;
      case '\t': result += F("\\t"); break;
      default:
        if (static_cast<uint8_t>(*cursor) >= 0x20) result += *cursor;
    }
  }
  return result;
}

String metricJson(const ClockMetricConfig &metric) {
  String result = F("{\"custom\":");
  result += metric.custom ? F("true") : F("false");
  result += F(",\"preset\":\"");
  result += jsonEscape(metric.preset);
  result += F("\",\"name\":\"");
  result += jsonEscape(metric.name);
  result += F("\",\"entityId\":\"");
  result += jsonEscape(metric.entityId);
  result += F("\",\"suffix\":\"");
  result += jsonEscape(metric.suffix);
  result += F("\",\"decimals\":");
  result += metric.decimals;
  result += '}';
  return result;
}

String htmlColor(uint32_t value) {
  char color[8];
  snprintf(color, sizeof(color), "#%06lX",
           static_cast<unsigned long>(value & 0xFFFFFF));
  return String(color);
}

String sideJson(const ClockSideConfig &side) {
  String result = F("{\"name\":\"");
  result += jsonEscape(side.name);
  result += F("\",\"temperatureEntityId\":\"");
  result += jsonEscape(side.temperatureEntityId);
  result += F("\",\"icon\":\"");
  result += jsonEscape(side.icon);
  result += F("\",\"color\":\"");
  result += htmlColor(side.color);
  result += F("\"}");
  return result;
}

String openMeteoSlotJson(const ClockOpenMeteoSlotConfig &slot) {
  String result = F("{\"value\":\"");
  result += jsonEscape(slot.value);
  result += F("\",\"name\":\"");
  result += jsonEscape(slot.name);
  result += F("\",\"color\":\"");
  result += htmlColor(slot.color);
  result += F("\"}");
  return result;
}

bool validOpenMeteoValue(const String &value) {
  static const char *values[] = {
      "temperature_2m",       "apparent_temperature",
      "relative_humidity_2m", "pressure_msl",
      "surface_pressure",     "wind_speed_10m",
      "wind_gusts_10m",       "wind_direction_10m",
      "precipitation",        "rain",
      "showers",              "snowfall",
      "cloud_cover",          "uv_index",
  };
  for (const char *candidate : values) {
    if (value == candidate) return true;
  }
  return false;
}

String urlEncode(const String &value) {
  static constexpr char HEX_DIGITS[] = "0123456789ABCDEF";
  String result;
  result.reserve(value.length() * 2);
  for (size_t index = 0; index < value.length(); ++index) {
    const uint8_t character = static_cast<uint8_t>(value[index]);
    if ((character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9') || character == '-' ||
        character == '_' || character == '.') {
      result += static_cast<char>(character);
    } else {
      result += '%';
      result += HEX_DIGITS[character >> 4];
      result += HEX_DIGITS[character & 0x0F];
    }
  }
  return result;
}

String colorScaleJson(const ClockMetricColorScale &scale) {
  String result = F("[");
  for (uint8_t index = 0; index < scale.count; ++index) {
    if (index > 0) result += ',';
    result += F("{\"value\":");
    result += String(scale.points[index].value, 3);
    result += F(",\"color\":\"");
    result += htmlColor(scale.points[index].color);
    result += F("\"}");
  }
  result += ']';
  return result;
}

void addSecurityHeaders() {
  server.sendHeader(F("Connection"), F("close"));
  server.sendHeader(F("Cache-Control"), F("no-store"));
  server.sendHeader(F("X-Content-Type-Options"), F("nosniff"));
  server.sendHeader(F("X-Frame-Options"), F("DENY"));
  server.sendHeader(
      F("Content-Security-Policy"),
      F("default-src 'self'; img-src 'self' data:; style-src 'unsafe-inline'; script-src "
        "'self' 'unsafe-inline'; connect-src 'self'; form-action 'self'; "
        "frame-ancestors 'none'"));
}

void sendJson(int status, const String &payload) {
  addSecurityHeaders();
  server.send(status, F("application/json; charset=utf-8"), payload);
}

void sendError(int status, const __FlashStringHelper *message) {
  String payload = F("{\"ok\":false,\"message\":\"");
  payload += message;
  payload += F("\"}");
  sendJson(status, payload);
}

ClockConfig &currentConfig() {
  if (configLoadCallback != nullptr) configLoadCallback(configBuffer);
  return configBuffer;
}

uint8_t browserLanguage() {
  String language = server.header("Accept-Language");
  language.trim();
  const int comma = language.indexOf(',');
  if (comma >= 0) language.remove(comma);
  const int quality = language.indexOf(';');
  if (quality >= 0) language.remove(quality);
  language.trim();
  language.toLowerCase();
  const bool czechOrSlovak =
      language == "cs" || language.startsWith("cs-") ||
      language.startsWith("cs_") || language == "sk" ||
      language.startsWith("sk-") || language.startsWith("sk_");
  return czechOrSlovak ? CLOCK_LANGUAGE_CZECH : CLOCK_LANGUAGE_ENGLISH;
}

void persistBrowserLanguageIfUnset() {
  ClockConfig &config = currentConfig();
  if (config.language != CLOCK_LANGUAGE_UNSET) return;
  config.language = browserLanguage();
  config.schemaVersion = CLOCK_CONFIG_SCHEMA_VERSION;
  if (configSaveCallback == nullptr || !configSaveCallback(config, false)) {
    config.language = CLOCK_LANGUAGE_UNSET;
  }
}

bool validRadarRadius(int radiusKm) {
  return radiusKm == 0 || radiusKm == 25 || radiusKm == 50 ||
         radiusKm == 100 || radiusKm == 200;
}

bool parseHtmlColor(const String &value, uint32_t &color);

void radarRangeState(uint16_t &savedRadiusKm, uint16_t &activeRadiusKm) {
  savedRadiusKm = currentConfig().radarRadiusKm;
  activeRadiusKm = savedRadiusKm;
  if (currentRadarRangeStateCallback != nullptr)
    currentRadarRangeStateCallback(savedRadiusKm, activeRadiusKm);
}

bool readAppearanceFromRequest(ClockAppearanceConfig &appearance) {
  const String style = server.arg("clockStyle");
  if (style == "digital")
    appearance.style = CLOCK_STYLE_DIGITAL;
  else if (style == "analog")
    appearance.style = CLOCK_STYLE_ANALOG;
  else
    return false;
  if (!parseHtmlColor(server.arg("analogToneColor"),
                      appearance.analogToneColor))
    return false;
  const String accents = server.arg("analogCardinalAccentsEnabled");
  if (accents != "0" && accents != "1") return false;
  appearance.analogCardinalAccentsEnabled = accents == "1";
  if (server.hasArg("monochromeWeatherIconColor")) {
    if (!parseHtmlColor(server.arg("monochromeWeatherIconColor"),
                        appearance.monochromeWeatherIconColor))
      return false;
  } else if (currentAppearanceStateCallback != nullptr) {
    ClockAppearanceConfig saved;
    ClockAppearanceConfig active;
    currentAppearanceStateCallback(saved, active);
    appearance.monochromeWeatherIconColor =
        active.monochromeWeatherIconColor;
  }
  return true;
}

void appearanceState(ClockAppearanceConfig &saved,
                     ClockAppearanceConfig &active) {
  saved = ClockAppearanceConfig{};
  active = saved;
  if (currentAppearanceStateCallback != nullptr)
    currentAppearanceStateCallback(saved, active);
}

String normalizedUrl(String url) {
  url.trim();
  while (url.endsWith("/")) url.remove(url.length() - 1);
  return url;
}

const char *normalizedRoomIcon(const String &icon) {
  static const char *icons[] = {"weather", "home", "living-room", "bedroom",
                                "kitchen", "none"};
  for (const char *candidate : icons) {
    if (icon == candidate) return candidate;
  }
  return "home";
}

bool validHomeAssistantUrl(const String &url) {
  return url.length() == 0 || url.startsWith("http://") ||
         url.startsWith("https://");
}

bool parseHtmlColor(const String &value, uint32_t &color) {
  if (value.length() != 7 || value[0] != '#') return false;
  char *end = nullptr;
  const unsigned long parsed = strtoul(value.c_str() + 1, &end, 16);
  if (end == value.c_str() + 1 || *end != '\0' || parsed > 0xFFFFFF) {
    return false;
  }
  color = static_cast<uint32_t>(parsed);
  return true;
}

bool parseFiniteFloat(const String &text, float &value) {
  char *end = nullptr;
  value = strtof(text.c_str(), &end);
  return end != text.c_str() && *end == '\0' && std::isfinite(value);
}

bool readColorScaleFromForm(const char *prefix, ClockMetricColorScale &scale) {
  const String fieldPrefix(prefix);
  const int count = server.arg(fieldPrefix + "Count").toInt();
  if (count < 1 || count > static_cast<int>(CLOCK_METRIC_COLOR_POINT_COUNT)) {
    return false;
  }
  scale = ClockMetricColorScale{};
  scale.count = static_cast<uint8_t>(count);
  for (uint8_t index = 0; index < scale.count; ++index) {
    const String suffix = String(index);
    if (!parseFiniteFloat(server.arg(fieldPrefix + "Value" + suffix),
                          scale.points[index].value) ||
        !parseHtmlColor(server.arg(fieldPrefix + "Color" + suffix),
                        scale.points[index].color)) {
      return false;
    }
  }
  for (uint8_t index = 1; index < scale.count; ++index) {
    const ClockMetricColorPoint point = scale.points[index];
    uint8_t position = index;
    while (position > 0 && scale.points[position - 1].value > point.value) {
      scale.points[position] = scale.points[position - 1];
      --position;
    }
    scale.points[position] = point;
  }
  for (uint8_t index = 1; index < scale.count; ++index) {
    if (scale.points[index - 1].value == scale.points[index].value) {
      return false;
    }
  }
  return true;
}

void applyPreset(ClockMetricConfig &metric, const String &preset) {
  struct Preset {
    const char *id;
    const char *name;
    const char *suffix;
    uint8_t decimals;
  };
  static const Preset presets[] = {
      {"co2", "CO₂", "ppm", 0},       {"voc", "VOC", "ppb", 0},
      {"pm25", "PM2.5", "µg/m³", 0}, {"pm10", "PM10", "µg/m³", 0},
      {"humidity", "VLHKOST", "%", 0}, {"pressure", "TLAK", "hPa", 0},
      {"aqi", "AQI", "", 0},          {"illuminance", "SVĚTLO", "lx", 0},
      {"noise", "HLUK", "dB", 0},     {"battery", "BATERIE", "%", 0},
  };
  const Preset *selected = &presets[0];
  for (const Preset &candidate : presets) {
    if (preset == candidate.id) {
      selected = &candidate;
      break;
    }
  }
  clockConfigCopy(metric.preset, sizeof(metric.preset), selected->id);
  clockConfigCopy(metric.name, sizeof(metric.name), selected->name);
  clockConfigCopy(metric.suffix, sizeof(metric.suffix), selected->suffix);
  metric.decimals = selected->decimals;
}

void readMetricFromForm(const char *prefix, ClockMetricConfig &metric) {
  const String fieldPrefix(prefix);
  metric.custom = server.arg(fieldPrefix + "Mode") == "custom";
  clockConfigCopy(metric.entityId, sizeof(metric.entityId),
                  server.arg(fieldPrefix + "Entity"));
  if (metric.custom) {
    clockConfigCopy(metric.preset, sizeof(metric.preset), "custom");
    clockConfigCopy(metric.name, sizeof(metric.name),
                    server.arg(fieldPrefix + "Name"));
    clockConfigCopy(metric.suffix, sizeof(metric.suffix),
                    server.arg(fieldPrefix + "Suffix"));
  } else {
    applyPreset(metric, server.arg(fieldPrefix + "Preset"));
  }
  metric.decimals =
      constrain(server.arg(fieldPrefix + "Decimals").toInt(), 0, 2);
}

bool readSideFromForm(const char *prefix, ClockSideConfig &side) {
  const String fieldPrefix(prefix);
  clockConfigCopy(side.name, sizeof(side.name), server.arg(fieldPrefix + "Name"));
  if (side.name[0] == '\0') {
    clockConfigCopy(side.name, sizeof(side.name), "MÍSTNOST");
  }
  clockConfigCopy(side.temperatureEntityId, sizeof(side.temperatureEntityId),
                  server.arg(fieldPrefix + "TemperatureEntity"));
  clockConfigCopy(side.icon, sizeof(side.icon),
                  normalizedRoomIcon(server.arg(fieldPrefix + "Icon")));
  return parseHtmlColor(server.arg(fieldPrefix + "Color"), side.color);
}

template <typename Client>
bool beginHomeAssistantRequest(HTTPClient &http, Client &client,
                               const String &url, const String &path,
                               const String &token) {
  http.setConnectTimeout(4000);
  http.setTimeout(8000);
  if (!http.begin(client, normalizedUrl(url) + path)) return false;
  http.addHeader(F("Authorization"), String(F("Bearer ")) + token);
  http.addHeader(F("Accept"), F("application/json"));
  return true;
}

template <typename Client>
int testHomeAssistant(Client &client, const String &url, const String &token,
                      const String &entityId) {
  HTTPClient http;
  const String path = entityId.isEmpty()
                          ? String(F("/api/"))
                          : String(F("/api/states/")) + entityId;
  if (!beginHomeAssistantRequest(http, client, url, path, token)) {
    return HTTPC_ERROR_CONNECTION_REFUSED;
  }
  const int status = http.GET();
  http.end();
  return status;
}

void resolveConnectionInput(String &url, String &token) {
  const ClockConfig &config = currentConfig();
  url = normalizedUrl(server.arg("haUrl"));
  token = server.arg("haToken");
  const String storedUrl = normalizedUrl(config.homeAssistantUrl);
  if (token.isEmpty() &&
      homeAssistantMayReuseStoredToken(url.c_str(), storedUrl.c_str())) {
    token = config.homeAssistantToken;
  }
  if (url.isEmpty()) url = storedUrl;
}

void handleRoot() {
  persistBrowserLanguageIfUnset();
  addSecurityHeaders();
  if (webActive) {
    extendWebAvailability();
    server.send_P(200, PSTR("text/html; charset=utf-8"),
                  webPasswordEnabled && !webSessionAuthenticated()
                      ? LOGIN_PAGE
                      : CONFIGURATION_PAGE);
  } else {
    server.send_P(200, PSTR("text/html; charset=utf-8"), DIAGNOSTIC_PAGE);
  }
}

void handleDiagnosticPage() {
  persistBrowserLanguageIfUnset();
  addSecurityHeaders();
  server.send_P(200, PSTR("text/html; charset=utf-8"), DIAGNOSTIC_PAGE);
}

bool requestOriginAllowed() {
  const String origin = server.header("Origin");
  if (origin.isEmpty()) return true;
  return origin == String(F("http://")) + server.hostHeader();
}

bool requireConfigurationAccess() {
  if (!webActive) {
    sendError(423, F("Konfigurace je zamčená. Aktivuj ji na displeji hodin."));
    return false;
  }
  if (server.method() != HTTP_GET && !requestOriginAllowed()) {
    sendError(403, F("Požadavek z cizí webové stránky byl odmítnut."));
    return false;
  }
  if (webPasswordEnabled && !webSessionAuthenticated()) {
    sendError(401, F("Nastavení je chráněné heslem. Přihlas se znovu."));
    return false;
  }
  extendWebAvailability();
  return true;
}

void handleWebLogin() {
  if (!webActive) {
    sendError(423, F("Konfigurace je zamčená. Aktivuj ji na displeji hodin."));
    return;
  }
  if (!requestOriginAllowed()) {
    sendError(403, F("Požadavek z cizí webové stránky byl odmítnut."));
    return;
  }
  extendWebAvailability();
  if (!webPasswordEnabled) {
    sendError(409, F("Ochrana webového nastavení není zapnutá."));
    return;
  }
  if (deadlinePending(loginBlockedUntil)) {
    sendError(429, F("Příliš mnoho pokusů. Zkus to za chvíli znovu."));
    return;
  }
  if (!webPasswordMatches(server.arg("password"))) {
    if (failedLoginAttempts < 8) ++failedLoginAttempts;
    const uint8_t exponent = failedLoginAttempts > 5
                                 ? 4
                                 : failedLoginAttempts - 1;
    loginBlockedUntil = millis() + (1000UL << exponent);
    sendError(401, F("Heslo není správné."));
    return;
  }
  failedLoginAttempts = 0;
  loginBlockedUntil = 0;
  issueWebSession();
  sendJson(200, F("{\"ok\":true}"));
}

void handleWebPassword() {
  const String action = server.arg("action");
  if (action == "clear") {
    if (!webPasswordEnabled) {
      sendError(409, F("Ochrana heslem už je vypnutá."));
      return;
    }
    if (!eraseWebPassword()) {
      sendError(500, F("Heslo se nepodařilo vymazat z paměti."));
      return;
    }
    clearWebSessions();
    expireWebSessionCookie();
    sendJson(200, F("{\"ok\":true,\"configured\":false}"));
    return;
  }

  const bool expectedAction =
      (!webPasswordEnabled && action == "set") ||
      (webPasswordEnabled && action == "change");
  if (!expectedAction) {
    sendError(409, webPasswordEnabled
                       ? F("Heslo už je nastavené. Použij Změnit.")
                       : F("Heslo zatím není nastavené. Použij Nastavit."));
    return;
  }
  const String password = server.arg("password");
  if (!validWebPasswordLength(password)) {
    sendError(400, F("Heslo musí mít 6 až 20 znaků."));
    return;
  }
  if (!persistWebPassword(password)) {
    sendError(500, F("Heslo se nepodařilo uložit do paměti."));
    return;
  }
  clearWebSessions();
  issueWebSession();
  sendJson(200, F("{\"ok\":true,\"configured\":true}"));
}

void handleGetConfig() {
  extendWebAvailability();
  const ClockConfig &config = currentConfig();
  uint16_t savedRadarRadiusKm = config.radarRadiusKm;
  uint16_t activeRadarRadiusKm = config.radarRadiusKm;
  radarRangeState(savedRadarRadiusKm, activeRadarRadiusKm);
  ClockAppearanceConfig savedAppearance;
  ClockAppearanceConfig activeAppearance;
  appearanceState(savedAppearance, activeAppearance);
  String result;
  result.reserve(3000);
  result = F("{\"ok\":true,\"homeAssistantUrl\":\"");
  result += jsonEscape(config.homeAssistantUrl);
  result += F("\",\"tokenConfigured\":");
  result += config.homeAssistantToken[0] == '\0' ? F("false") : F("true");
  result += F(",\"webPasswordConfigured\":");
  result += webPasswordEnabled ? F("true") : F("false");
  result += F(",\"dataSource\":\"");
  result += config.dataSource == CLOCK_DATA_SOURCE_HOME_ASSISTANT
                ? F("home-assistant")
                : F("open-meteo");
  result += F("\",\"language\":\"");
  result += config.language == CLOCK_LANGUAGE_ENGLISH ? F("en") : F("cs");
  result += F("\",\"openMeteoCity\":\"");
  result += jsonEscape(config.openMeteoCity);
  result += F("\",\"openMeteoLatitude\":");
  result += String(config.openMeteoLatitude, 5);
  result += F(",\"openMeteoLongitude\":");
  result += String(config.openMeteoLongitude, 5);
  result += F(",\"openMeteoCountry\":\"");
  result += clockConfigRadarAvailable(config) ? F("CZ") : F("OTHER");
  result += F("\",\"radarAvailable\":");
  result += clockConfigRadarAvailable(config) ? F("true") : F("false");
  result += F(",\"radarRadiusKm\":");
  result += savedRadarRadiusKm;
  result += F(",\"radarActiveRadiusKm\":");
  result += activeRadarRadiusKm;
  result += F(",\"radarFrameCount\":");
  result += config.radarFrameCount;
  result += F(",\"radarMapOpacity\":");
  result += config.radarMapOpacity;
  result += F(",\"radarPauseSeconds\":");
  result += config.radarPauseSeconds;
  result += F(",\"automaticRadarRotation\":");
  result += config.automaticRadarRotation ? F("true") : F("false");
  result += F(",\"clockDisplaySeconds\":");
  result += config.clockDisplaySeconds;
  result += F(",\"radarDisplaySeconds\":");
  result += config.radarDisplaySeconds;
  result += F(",\"clockStyle\":\"");
  result += savedAppearance.style == CLOCK_STYLE_ANALOG ? F("analog")
                                                        : F("digital");
  result += F("\",\"activeClockStyle\":\"");
  result += activeAppearance.style == CLOCK_STYLE_ANALOG ? F("analog")
                                                         : F("digital");
  result += F("\",\"analogToneColor\":\"");
  result += htmlColor(savedAppearance.analogToneColor);
  result += F("\",\"activeAnalogToneColor\":\"");
  result += htmlColor(activeAppearance.analogToneColor);
  result += F("\",\"analogCardinalAccentsEnabled\":");
  result += savedAppearance.analogCardinalAccentsEnabled ? F("true")
                                                         : F("false");
  result += F(",\"activeAnalogCardinalAccentsEnabled\":");
  result += activeAppearance.analogCardinalAccentsEnabled ? F("true")
                                                          : F("false");
  result += F(",\"monochromeWeatherIconColor\":\"");
  result += htmlColor(savedAppearance.monochromeWeatherIconColor);
  result += F("\",\"activeMonochromeWeatherIconColor\":\"");
  result += htmlColor(activeAppearance.monochromeWeatherIconColor);
  result += '"';
  result += F(",\"openMeteoSlots\":[");
  for (size_t index = 0; index < 4; ++index) {
    if (index > 0) result += ',';
    result += openMeteoSlotJson(config.openMeteoSlots[index]);
  }
  result += ']';
  result += F(",\"controlSecret\":\"");
  result += jsonEscape(controlSecret.c_str());
  result += F("\"");
  result += F(",\"weatherEntityId\":\"");
  result += jsonEscape(config.weatherEntityId);
  result += F("\",\"sunEntityId\":\"");
  result += jsonEscape(config.sunEntityId);
  result += F("\",\"dayNightLightEntityId\":\"");
  result += jsonEscape(config.dayNightLightEntityId);
  result += F("\",\"sunriseOffsetMinutes\":");
  result += static_cast<int>(config.sunriseOffsetMinutes);
  result += F(",\"sunsetOffsetMinutes\":");
  result += static_cast<int>(config.sunsetOffsetMinutes);
  uint64_t nextSunriseTimestamp = 0;
  uint64_t nextSunsetTimestamp = 0;
  if (sunTransitionTimesCallback != nullptr) {
    sunTransitionTimesCallback(nextSunriseTimestamp, nextSunsetTimestamp);
  }
  result += F(",\"nextSunriseTimestamp\":");
  result += static_cast<unsigned long>(nextSunriseTimestamp);
  result += F(",\"nextSunsetTimestamp\":");
  result += static_cast<unsigned long>(nextSunsetTimestamp);
  result += F(",\"animatedWeatherIcons\":");
  result += config.animatedWeatherIcons ? F("true") : F("false");
  result += F(",\"weatherIconStyle\":\"");
  if (config.weatherIconStyle == CLOCK_WEATHER_ICON_STYLE_FLAT) {
    result += F("flat");
  } else if (config.weatherIconStyle == CLOCK_WEATHER_ICON_STYLE_LINE) {
    result += F("line");
  } else {
    result += F("monochrome");
  }
  result += '"';
  result += F(",\"leftSide\":");
  result += sideJson(config.leftSide);
  result += F(",\"rightSide\":");
  result += sideJson(config.rightSide);
  result += F(",\"metricA\":");
  result += metricJson(config.metricA);
  result += F(",\"metricB\":");
  result += metricJson(config.metricB);
  result += F(",\"metricAColorScale\":");
  result += colorScaleJson(config.metricAColorScale);
  result += F(",\"metricBColorScale\":");
  result += colorScaleJson(config.metricBColorScale);
  result += F(",\"dayBrightness\":");
  result += config.dayBrightness;
  result += F(",\"nightBrightness\":");
  result += config.nightBrightness;
  result += F(",\"automaticDayNight\":");
  result += config.automaticDayNight ? F("true") : F("false");
  result += F(",\"nightVisualMode\":\"");
  result += config.nightVisualMode == CLOCK_NIGHT_VISUAL_BRIGHTNESS_ONLY
                ? F("brightness")
                : F("red");
  result += '"';
  result += F(",\"automaticFirmwareUpdate\":");
  result += config.automaticFirmwareUpdate ? F("true") : F("false");
  result += F(",\"webMode\":\"");
  if (selectedWebMode == CONFIGURATION_WEB_ALWAYS)
    result += F("always");
  else if (selectedWebMode == CONFIGURATION_WEB_DISABLED)
    result += F("disabled");
  else
    result += F("timed");
  result += '"';
  result += F(",\"timeColor\":\"");
  result += htmlColor(config.timeColor);
  result += F("\",\"timeColonEffect\":\"");
  if (config.timeColonEffect == CLOCK_TIME_COLON_FADE)
    result += F("fade");
  else if (config.timeColonEffect == CLOCK_TIME_COLON_BLINK)
    result += F("blink");
  else
    result += F("steady");
  result += '"';
  result += F(",\"showLeadingHourZero\":");
  result += config.showLeadingHourZero ? F("true") : F("false");
  result += F(",\"timeFont\":\"");
  if (config.timeFont == CLOCK_TIME_FONT_LIBERATION_SANS)
    result += F("liberation");
  else if (config.timeFont == CLOCK_TIME_FONT_LCD)
    result += F("lcd");
  else if (config.timeFont == CLOCK_TIME_FONT_DOTO)
    result += F("doto");
  else
    result += F("barlow");
  result += F("\",\"dateFormat\":\"");
  if (config.dateFormat == CLOCK_DATE_FORMAT_NUMERIC)
    result += F("numeric");
  else if (config.dateFormat == CLOCK_DATE_FORMAT_DAY_MONTH_YEAR)
    result += F("day-month-year");
  else if (config.dateFormat == CLOCK_DATE_FORMAT_WEEKDAY_DAY_MONTH_YEAR)
    result += F("weekday-day-month-year");
  else if (config.dateFormat == CLOCK_DATE_FORMAT_HIDDEN)
    result += F("hidden");
  else
    result += F("weekday-day-month");
  result += F("\",\"dateColor\":\"");
  result += htmlColor(config.dateColor);
  result += '"';
  result += F(",\"leftWeatherIconColor\":\"");
  result += htmlColor(config.leftWeatherIconColor);
  result += F("\",\"rightWeatherIconColor\":\"");
  result += htmlColor(config.rightWeatherIconColor);
  result += '"';
  result += F(",\"secondRingEnabled\":");
  result += config.secondRingEnabled ? F("true") : F("false");
  result += F(",\"secondEffect\":\"");
  if (config.secondEffect == CLOCK_SECOND_EFFECT_COMET)
    result += F("comet");
  else if (config.secondEffect == CLOCK_SECOND_EFFECT_LINE)
    result += F("line");
  else
    result += F("dots");
  result += '"';
  result += F(",\"secondRingBackgroundColor\":\"");
  result += htmlColor(config.secondRingBackgroundColor);
  result += '"';
  result += F(",\"secondRingBackgroundBrightness\":");
  result += config.secondRingBackgroundBrightness;
  result += F(",\"secondRingBackgroundDotSize\":");
  result += config.secondRingBackgroundDotSize;
  result += F(",\"secondDotSize\":");
  result += config.secondDotSize;
  result += F(",\"secondDotColor\":\"");
  result += htmlColor(config.secondDotColor);
  result += '"';
  result += F(",\"secondDotBrightness\":");
  result += config.secondDotBrightness;
  result += '}';
  sendJson(200, result);
}

void handleSaveConfig() {
  ClockConfig &config = currentConfig();
  const String language = server.arg("language");
  if (language == "cs")
    config.language = CLOCK_LANGUAGE_CZECH;
  else if (language == "en")
    config.language = CLOCK_LANGUAGE_ENGLISH;
  else {
    sendError(400, F("Jazyk není platný."));
    return;
  }
  const String dataSource = server.arg("dataSource");
  if (dataSource == "open-meteo")
    config.dataSource = CLOCK_DATA_SOURCE_OPEN_METEO;
  else if (dataSource == "home-assistant")
    config.dataSource = CLOCK_DATA_SOURCE_HOME_ASSISTANT;
  else {
    sendError(400, F("Zdroj dat není platný."));
    return;
  }
  String openMeteoCity = server.arg("openMeteoCity");
  openMeteoCity.trim();
  float openMeteoLatitude = 0;
  float openMeteoLongitude = 0;
  if (openMeteoCity.isEmpty() ||
      !parseFiniteFloat(server.arg("openMeteoLatitude"), openMeteoLatitude) ||
      !parseFiniteFloat(server.arg("openMeteoLongitude"), openMeteoLongitude) ||
      openMeteoLatitude < -90 || openMeteoLatitude > 90 ||
      openMeteoLongitude < -180 || openMeteoLongitude > 180) {
    sendError(400, F("Nejprve vyhledej platnou polohu zařízení."));
    return;
  }
  clockConfigCopy(config.openMeteoCity, sizeof(config.openMeteoCity),
                  openMeteoCity);
  config.openMeteoLatitude = openMeteoLatitude;
  config.openMeteoLongitude = openMeteoLongitude;
  String openMeteoCountry = server.arg("openMeteoCountry");
  openMeteoCountry.trim();
  openMeteoCountry.toUpperCase();
  if (openMeteoCountry == "CZ")
    config.openMeteoCountry = CLOCK_LOCATION_COUNTRY_CZECHIA;
  else if (openMeteoCountry == "OTHER" || openMeteoCountry.length() == 2)
    config.openMeteoCountry = CLOCK_LOCATION_COUNTRY_OTHER;
  else {
    sendError(400, F("Nejprve vyhledej platnou polohu zařízení."));
    return;
  }
  const int radarRadiusKm = server.arg("radarRadiusKm").toInt();
  if (!validRadarRadius(radarRadiusKm)) {
    sendError(400, F("Rozsah meteoradaru není platný."));
    return;
  }
  config.radarRadiusKm = static_cast<uint16_t>(radarRadiusKm);
  const int radarFrameCount = server.arg("radarFrameCount").toInt();
  if (radarFrameCount < 1 || radarFrameCount > 15) {
    sendError(400, F("Počet snímků meteoradaru musí být od 1 do 15."));
    return;
  }
  config.radarFrameCount = static_cast<uint8_t>(radarFrameCount);
  const int radarMapOpacity = server.arg("radarMapOpacity").toInt();
  if (radarMapOpacity < 0 || radarMapOpacity > 100) {
    sendError(400, F("Viditelnost mapy meteoradaru musí být od 0 do 100 %."));
    return;
  }
  config.radarMapOpacity = static_cast<uint8_t>(radarMapOpacity);
  const int radarPauseSeconds = server.arg("radarPauseSeconds").toInt();
  if (radarPauseSeconds < 0 || radarPauseSeconds > 30) {
    sendError(400, F("Pauza animace meteoradaru musí být od 0 do 30 sekund."));
    return;
  }
  config.radarPauseSeconds = static_cast<uint8_t>(radarPauseSeconds);
  const int clockDisplaySeconds = server.arg("clockDisplaySeconds").toInt();
  const int radarDisplaySeconds = server.arg("radarDisplaySeconds").toInt();
  if (clockDisplaySeconds < 10 || clockDisplaySeconds > 3600 ||
      radarDisplaySeconds < 10 || radarDisplaySeconds > 3600) {
    sendError(400, F("Časy automatického střídání musí být od 10 do 3600 sekund."));
    return;
  }
  config.automaticRadarRotation =
      clockConfigRadarAvailable(config) &&
      server.arg("automaticRadarRotation") == "1";
  config.clockDisplaySeconds =
      static_cast<uint16_t>(clockDisplaySeconds);
  config.radarDisplaySeconds =
      static_cast<uint16_t>(radarDisplaySeconds);
  for (size_t index = 0; index < 4; ++index) {
    const String prefix = String(F("openMeteoSlot")) + index;
    const String value = server.arg(prefix + F("Value"));
    if (!validOpenMeteoValue(value) ||
        !parseHtmlColor(server.arg(prefix + F("Color")),
                        config.openMeteoSlots[index].color)) {
      sendError(400, F("Nastavení pozice Open-Meteo není platné."));
      return;
    }
    clockConfigCopy(config.openMeteoSlots[index].value,
                    sizeof(config.openMeteoSlots[index].value), value);
    clockConfigCopy(config.openMeteoSlots[index].name,
                    sizeof(config.openMeteoSlots[index].name),
                    server.arg(prefix + F("Name")));
  }
  const String webModeValue = server.arg("webMode");
  ConfigurationWebMode requestedWebMode = CONFIGURATION_WEB_TIMED;
  if (webModeValue == "always")
    requestedWebMode = CONFIGURATION_WEB_ALWAYS;
  else if (webModeValue == "disabled")
    requestedWebMode = CONFIGURATION_WEB_DISABLED;
  else if (webModeValue != "timed") {
    sendError(400, F("Režim webového serveru není platný."));
    return;
  }
  const String url = normalizedUrl(server.arg("haUrl"));
  if (!validHomeAssistantUrl(url)) {
    sendError(400, F("Adresa Home Assistantu musí začínat http:// nebo https://."));
    return;
  }
  const bool automaticDayNight = server.arg("automaticDayNight") == "1";
  String sunEntity = server.arg("sunEntity");
  sunEntity.trim();
  if (config.dataSource == CLOCK_DATA_SOURCE_HOME_ASSISTANT &&
      automaticDayNight && sunEntity.isEmpty()) {
    sendError(400,
              F("Pro automatický režim DEN/NOC musí být vyplněna SUN entita."));
    return;
  }
  clockConfigCopy(config.homeAssistantUrl, sizeof(config.homeAssistantUrl), url);
  const String submittedToken = server.arg("haToken");
  if (!submittedToken.isEmpty()) {
    clockConfigCopy(config.homeAssistantToken,
                    sizeof(config.homeAssistantToken), submittedToken);
  }
  clockConfigCopy(config.weatherEntityId, sizeof(config.weatherEntityId),
                  server.arg("weatherEntity"));
  clockConfigCopy(config.sunEntityId, sizeof(config.sunEntityId),
                  sunEntity);
  String dayNightLightEntity = server.arg("dayNightLightEntity");
  dayNightLightEntity.trim();
  clockConfigCopy(config.dayNightLightEntityId,
                  sizeof(config.dayNightLightEntityId), dayNightLightEntity);
  const int sunriseOffsetMinutes = server.arg("sunriseOffsetMinutes").toInt();
  const int sunsetOffsetMinutes = server.arg("sunsetOffsetMinutes").toInt();
  if (sunriseOffsetMinutes < -60 || sunriseOffsetMinutes > 60 ||
      sunriseOffsetMinutes % 15 != 0 || sunsetOffsetMinutes < -60 ||
      sunsetOffsetMinutes > 60 || sunsetOffsetMinutes % 15 != 0) {
    sendError(400,
              F("Posuny SUN musí být od -60 do +60 minut po 15 minutách."));
    return;
  }
  config.sunriseOffsetMinutes = static_cast<int8_t>(sunriseOffsetMinutes);
  config.sunsetOffsetMinutes = static_cast<int8_t>(sunsetOffsetMinutes);
  config.animatedWeatherIcons = server.arg("animatedWeatherIcons") == "1";
  const String weatherIconStyle = server.arg("weatherIconStyle");
  if (weatherIconStyle.isEmpty() && !config.animatedWeatherIcons) {
    // Disabled HTML controls are omitted from form submissions. Preserve the
    // stored style so older configuration pages can still disable animations.
  } else if (weatherIconStyle == "monochrome") {
    config.weatherIconStyle = CLOCK_WEATHER_ICON_STYLE_MONOCHROME;
  } else if (weatherIconStyle == "flat") {
    config.weatherIconStyle = CLOCK_WEATHER_ICON_STYLE_FLAT;
  } else if (weatherIconStyle == "line") {
    config.weatherIconStyle = CLOCK_WEATHER_ICON_STYLE_LINE;
  } else {
    sendError(400, F("Styl animovaných ikon počasí není platný."));
    return;
  }
  if (!readSideFromForm("left", config.leftSide) ||
      !readSideFromForm("right", config.rightSide)) {
    sendError(400, F("Barva místnosti není platná."));
    return;
  }
  readMetricFromForm("metricA", config.metricA);
  readMetricFromForm("metricB", config.metricB);
  if (!readColorScaleFromForm("metricAColor", config.metricAColorScale) ||
      !readColorScaleFromForm("metricBColor", config.metricBColorScale)) {
    sendError(400, F("Barevná škála musí obsahovat 1 až 10 platných bodů bez duplicitních hodnot."));
    return;
  }
  config.dayBrightness =
      constrain(server.arg("dayBrightness").toInt(), 1, 100);
  config.nightBrightness =
      constrain(server.arg("nightBrightness").toInt(), 1, 100);
  config.automaticDayNight = automaticDayNight;
  const String nightVisualMode = server.arg("nightVisualMode");
  if (nightVisualMode == "red") {
    config.nightVisualMode = CLOCK_NIGHT_VISUAL_RED;
  } else if (nightVisualMode == "brightness") {
    config.nightVisualMode = CLOCK_NIGHT_VISUAL_BRIGHTNESS_ONLY;
  } else {
    sendError(400, F("Vzhled nočního režimu není platný."));
    return;
  }
  config.automaticFirmwareUpdate =
      server.arg("automaticFirmwareUpdate") == "1";
  const String timeColonEffect = server.arg("timeColonEffect");
  if (timeColonEffect == "steady")
    config.timeColonEffect = CLOCK_TIME_COLON_STEADY;
  else if (timeColonEffect == "blink")
    config.timeColonEffect = CLOCK_TIME_COLON_BLINK;
  else if (timeColonEffect == "fade")
    config.timeColonEffect = CLOCK_TIME_COLON_FADE;
  else {
    sendError(400, F("Efekt dvojtečky hodin není platný."));
    return;
  }
  config.showLeadingHourZero = server.arg("showLeadingHourZero") == "1";
  const String timeFont = server.arg("timeFont");
  if (timeFont == "barlow")
    config.timeFont = CLOCK_TIME_FONT_BARLOW;
  else if (timeFont == "liberation")
    config.timeFont = CLOCK_TIME_FONT_LIBERATION_SANS;
  else if (timeFont == "lcd")
    config.timeFont = CLOCK_TIME_FONT_LCD;
  else if (timeFont == "doto")
    config.timeFont = CLOCK_TIME_FONT_DOTO;
  else {
    sendError(400, F("Font hodin není platný."));
    return;
  }
  const String dateFormat = server.arg("dateFormat");
  if (dateFormat == "weekday-day-month")
    config.dateFormat = CLOCK_DATE_FORMAT_WEEKDAY_DAY_MONTH;
  else if (dateFormat == "numeric")
    config.dateFormat = CLOCK_DATE_FORMAT_NUMERIC;
  else if (dateFormat == "day-month-year")
    config.dateFormat = CLOCK_DATE_FORMAT_DAY_MONTH_YEAR;
  else if (dateFormat == "weekday-day-month-year")
    config.dateFormat = CLOCK_DATE_FORMAT_WEEKDAY_DAY_MONTH_YEAR;
  else if (dateFormat == "hidden")
    config.dateFormat = CLOCK_DATE_FORMAT_HIDDEN;
  else {
    sendError(400, F("Formát data není platný."));
    return;
  }
  if (!parseHtmlColor(server.arg("timeColor"), config.timeColor) ||
      !parseHtmlColor(server.arg("dateColor"), config.dateColor) ||
      !parseHtmlColor(server.arg("leftWeatherIconColor"),
                      config.leftWeatherIconColor) ||
      !parseHtmlColor(server.arg("rightWeatherIconColor"),
                      config.rightWeatherIconColor)) {
    sendError(400, F("Barva hodin, data nebo ikon není platná."));
    return;
  }
  const String secondEffect = server.arg("secondEffect");
  if (secondEffect != "off" && secondEffect != "dots" && secondEffect != "line" &&
      secondEffect != "comet") {
    sendError(400, F("Efekt zobrazení vteřin není platný."));
    return;
  }
  config.secondRingEnabled = secondEffect != "off";
  if (secondEffect != "off" && server.hasArg("secondRingEnabled")) {
    // Kompatibilita se starší webovou stránkou se samostatným přepínačem.
    config.secondRingEnabled = server.arg("secondRingEnabled") == "1";
  }
  if (secondEffect == "comet")
    config.secondEffect = CLOCK_SECOND_EFFECT_COMET;
  else if (secondEffect == "line")
    config.secondEffect = CLOCK_SECOND_EFFECT_LINE;
  else
    config.secondEffect = CLOCK_SECOND_EFFECT_DOTS;
  uint32_t secondRingBackgroundColor;
  if (!parseHtmlColor(server.arg("secondRingBackgroundColor"),
                      secondRingBackgroundColor)) {
    sendError(400, F("Barva pozadí vteřin není platná."));
    return;
  }
  config.secondRingBackgroundColor = secondRingBackgroundColor;
  config.secondRingBackgroundBrightness = constrain(
      server.arg("secondRingBackgroundBrightness").toInt(), 0, 255);
  config.secondRingBackgroundDotSize =
      constrain(server.arg("secondRingBackgroundDotSize").toInt(), 1, 10);
  config.secondDotSize =
      constrain(server.arg("secondDotSize").toInt(), 1, 10);
  uint32_t secondDotColor;
  if (!parseHtmlColor(server.arg("secondDotColor"), secondDotColor)) {
    sendError(400, F("Barva aktivních vteřin není platná."));
    return;
  }
  config.secondDotColor = secondDotColor;
  config.secondDotBrightness =
      constrain(server.arg("secondDotBrightness").toInt(), 0, 255);
  config.schemaVersion = CLOCK_CONFIG_SCHEMA_VERSION;

  ClockAppearanceConfig appearance;
  if (!readAppearanceFromRequest(appearance)) {
    sendError(400, F("Typ nebo tón hodin není platný."));
    return;
  }

  if (configSaveCallback == nullptr ||
      !configSaveCallback(config, !submittedToken.isEmpty())) {
    sendError(500, F("Nastavení se nepodařilo uložit do paměti."));
    return;
  }
  if (currentAppearanceSaveCallback == nullptr ||
      !currentAppearanceSaveCallback(appearance)) {
    sendError(500, F("Vzhled hodin se nepodařilo uložit do paměti."));
    return;
  }
  if (!persistWebMode(requestedWebMode)) {
    sendError(500, F("Režim webového serveru se nepodařilo uložit."));
    return;
  }
  extendWebAvailability();
  sendJson(200, F("{\"ok\":true}"));
  applyWebMode(requestedWebMode);
}

void handleSaveLanguage() {
  ClockConfig &config = currentConfig();
  const String language = server.arg("language");
  if (language == "cs")
    config.language = CLOCK_LANGUAGE_CZECH;
  else if (language == "en")
    config.language = CLOCK_LANGUAGE_ENGLISH;
  else {
    sendError(400, F("Jazyk není platný."));
    return;
  }
  config.schemaVersion = CLOCK_CONFIG_SCHEMA_VERSION;
  if (configSaveCallback == nullptr || !configSaveCallback(config, false)) {
    sendError(500, F("Nastavení se nepodařilo uložit do paměti."));
    return;
  }
  extendWebAvailability();
  sendJson(200, F("{\"ok\":true}"));
}

void handleRadarRangeState() {
  extendWebAvailability();
  uint16_t savedRadiusKm = 0;
  uint16_t activeRadiusKm = 0;
  radarRangeState(savedRadiusKm, activeRadiusKm);
  String result = F("{\"ok\":true,\"savedRadiusKm\":");
  result += savedRadiusKm;
  result += F(",\"activeRadiusKm\":");
  result += activeRadiusKm;
  result += F(",\"available\":");
  result += clockConfigRadarAvailable(currentConfig()) ? F("true")
                                                        : F("false");
  result += '}';
  sendJson(200, result);
}

void handleRadarRangePreview() {
  extendWebAvailability();
  if (!clockConfigRadarAvailable(currentConfig())) {
    sendError(409, F("Meteoradar ČHMÚ je dostupný pouze pro lokality v České republice."));
    return;
  }
  const int radiusKm = server.arg("radiusKm").toInt();
  if (!validRadarRadius(radiusKm)) {
    sendError(400, F("Rozsah meteoradaru není platný."));
    return;
  }
  if (currentRadarRangePreviewCallback == nullptr ||
      !currentRadarRangePreviewCallback(static_cast<uint16_t>(radiusKm))) {
    sendError(503, F("Rozsah meteoradaru se nepodařilo změnit."));
    return;
  }
  handleRadarRangeState();
}

void handleClockAppearancePreview() {
  extendWebAvailability();
  ClockAppearanceConfig appearance;
  if (!readAppearanceFromRequest(appearance)) {
    sendError(400, F("Typ nebo tón hodin není platný."));
    return;
  }
  if (currentAppearancePreviewCallback == nullptr ||
      !currentAppearancePreviewCallback(appearance)) {
    sendError(503, F("Náhled vzhledu hodin se nepodařilo změnit."));
    return;
  }
  ClockAppearanceConfig saved;
  ClockAppearanceConfig active;
  appearanceState(saved, active);
  String result = F("{\"ok\":true,\"clockStyle\":\"");
  result += saved.style == CLOCK_STYLE_ANALOG ? F("analog") : F("digital");
  result += F("\",\"activeClockStyle\":\"");
  result += active.style == CLOCK_STYLE_ANALOG ? F("analog") : F("digital");
  result += F("\",\"analogToneColor\":\"");
  result += htmlColor(saved.analogToneColor);
  result += F("\",\"activeAnalogToneColor\":\"");
  result += htmlColor(active.analogToneColor);
  result += F("\",\"analogCardinalAccentsEnabled\":");
  result += saved.analogCardinalAccentsEnabled ? F("true") : F("false");
  result += F(",\"activeAnalogCardinalAccentsEnabled\":");
  result += active.analogCardinalAccentsEnabled ? F("true") : F("false");
  result += F(",\"monochromeWeatherIconColor\":\"");
  result += htmlColor(saved.monochromeWeatherIconColor);
  result += F("\",\"activeMonochromeWeatherIconColor\":\"");
  result += htmlColor(active.monochromeWeatherIconColor);
  result += '"';
  result += F("}");
  sendJson(200, result);
}

void handleOpenMeteoLocation() {
  String city = server.arg("city");
  city.trim();
  if (city.length() < 2) {
    sendError(400, F("Zadej název města."));
    return;
  }
  networkDiagnosticsBegin(NetworkDiagnosticKind::OpenMeteoTest);
  NetworkOperationGuard networkGuard(8000);
  if (!networkGuard) {
    networkDiagnosticsEnd(NetworkDiagnosticKind::OpenMeteoTest, false,
                          HTTPC_ERROR_CONNECTION_REFUSED);
    sendError(503, F("Síť je právě vytížená jinou operací."));
    return;
  }
  WiFiClientSecure client;
  client.setCACert(FIRMWARE_RELEASE_ROOT_CA);
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(8000);
  const bool englishLanguage =
      currentConfig().language == CLOCK_LANGUAGE_ENGLISH;
  const String url =
      String(F("https://geocoding-api.open-meteo.com/v1/search?count=10&language=")) +
      (englishLanguage ? F("en") : F("cs")) + F("&format=json&name=") +
      urlEncode(city);
  int status = HTTPC_ERROR_CONNECTION_REFUSED;
  String payload;
  if (http.begin(client, url)) {
    status = http.GET();
    if (status == HTTP_CODE_OK) payload = http.getString();
    http.end();
  }
  const bool ok = status == HTTP_CODE_OK && payload.indexOf(F("\"results\"")) >= 0;
  networkDiagnosticsEnd(NetworkDiagnosticKind::OpenMeteoTest, ok, status);
  if (!ok) {
    sendError(502, status == HTTP_CODE_OK
                       ? F("Město nebylo nalezeno.")
                       : F("Geokódovací služba nyní není dostupná."));
    return;
  }
  sendJson(200, payload);
}

void handleTestConnection() {
  String url;
  String token;
  resolveConnectionInput(url, token);
  if (!validHomeAssistantUrl(url) || url.isEmpty() || token.isEmpty()) {
    sendError(400, F("Doplň adresu Home Assistantu a token."));
    return;
  }
  int status;
  String entityId = server.arg("haEntity");
  entityId.trim();
  if (entityId.isEmpty()) entityId = currentConfig().weatherEntityId;
  networkDiagnosticsBegin(NetworkDiagnosticKind::HomeAssistantTest);
  NetworkOperationGuard networkGuard(8000);
  if (!networkGuard) {
    networkDiagnosticsEnd(NetworkDiagnosticKind::HomeAssistantTest, false,
                          HTTPC_ERROR_CONNECTION_REFUSED);
    sendError(503, F("Síť je právě vytížená jinou operací."));
    return;
  }
  if (url.startsWith("https://")) {
    WiFiClientSecure client;
    client.setInsecure();
    status = testHomeAssistant(client, url, token, entityId);
  } else {
    WiFiClient client;
    status = testHomeAssistant(client, url, token, entityId);
  }
  networkDiagnosticsEnd(NetworkDiagnosticKind::HomeAssistantTest,
                        status == HTTP_CODE_OK, status);
  if (status == HTTP_CODE_OK) {
    sendJson(200, F("{\"ok\":true}"));
  } else if (status == HTTP_CODE_UNAUTHORIZED) {
    sendError(401, F("Home Assistant odmítl token."));
  } else {
    sendError(502, F("Home Assistant není dostupný na zadané adrese."));
  }
}

void appendMemoryJson(String &result, const NetworkMemorySnapshot &memory) {
  result += F("{\"internalFree\":");
  result += memory.internalFree;
  result += F(",\"internalLargest\":");
  result += memory.internalLargest;
  result += F(",\"psramFree\":");
  result += memory.psramFree;
  result += F(",\"psramLargest\":");
  result += memory.psramLargest;
  result += '}';
}

void appendDiagnosticJson(String &result,
                          const NetworkDiagnosticSnapshot &snapshot) {
  result += F("{\"attempts\":");
  result += snapshot.attempts;
  result += F(",\"successes\":");
  result += snapshot.successes;
  result += F(",\"failures\":");
  result += snapshot.failures;
  result += F(",\"lastResult\":");
  result += snapshot.lastResult;
  result += F(",\"lastStartedAt\":");
  result += snapshot.lastStartedAt;
  result += F(",\"lastFinishedAt\":");
  result += snapshot.lastFinishedAt;
  result += F(",\"before\":");
  appendMemoryJson(result, snapshot.before);
  result += F(",\"after\":");
  appendMemoryJson(result, snapshot.after);
  result += F(",\"detail\":\"");
  result += jsonEscape(snapshot.detail);
  result += '"';
  result += '}';
}

void handleDiagnostics() {
  const FirmwareUpdateSnapshot firmware = firmwareUpdateServiceSnapshot();
  ChmiRadarDiagnostics radar;
  chmiRadarServiceDiagnostics(radar);
  const ClockConfig &config = currentConfig();
  bool sunAvailable = false;
  bool sunIsDay = true;
  bool lightAvailable = false;
  bool lightOn = false;
  bool nightMode = false;
  if (currentDayNightStatusCallback != nullptr) {
    currentDayNightStatusCallback(sunAvailable, sunIsDay, lightAvailable,
                                  lightOn, nightMode);
  }
  String result;
  result.reserve(2400);
  result = F("{\"ok\":true,\"configurationAvailable\":");
  result += webActive ? F("true") : F("false");
  result += F(",\"webMode\":\"");
  if (selectedWebMode == CONFIGURATION_WEB_ALWAYS)
    result += F("always");
  else if (selectedWebMode == CONFIGURATION_WEB_DISABLED)
    result += F("disabled");
  else
    result += F("timed");
  result += F("\",\"language\":\"");
  result += config.language == CLOCK_LANGUAGE_ENGLISH ? F("en") : F("cs");
  result += F("\",\"firmwareVersion\":\"");
  result += jsonEscape(FIRMWARE_VERSION);
  result += F("\",\"chipModel\":\"");
  result += jsonEscape(ESP.getChipModel());
  result += F("\",\"chipRevision\":");
  result += ESP.getChipRevision();
  result += F(",\"cpuFrequencyMHz\":");
  result += ESP.getCpuFreqMHz();
  result += F(",\"displayPixelClockHz\":");
  result += LCD_GetPixelClock();
  result += F(",\"resetReason\":");
  result += static_cast<int>(esp_reset_reason());
  result += F(",\"flashSize\":");
  result += ESP.getFlashChipSize();
  result += F(",\"psramSize\":");
  result += ESP.getPsramSize();
  result += F(",\"wifiConnected\":");
  result += WiFi.status() == WL_CONNECTED ? F("true") : F("false");
  result += F(",\"wifiRssi\":");
  result += WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
  result += F(",\"ipAddress\":\"");
  result += WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : F("");
  result += F("\",\"firmwareState\":\"");
  result += firmwareUpdateStateName(firmware.state);
  result += F("\",\"firmwareMessage\":\"");
  result += jsonEscape(firmware.message);
  result += F("\",\"sunStateAvailable\":");
  result += sunAvailable ? F("true") : F("false");
  result += F(",\"sunIsDay\":");
  result += sunIsDay ? F("true") : F("false");
  result += F(",\"dayNightLightStateAvailable\":");
  result += lightAvailable ? F("true") : F("false");
  result += F(",\"dayNightLightOn\":");
  result += lightOn ? F("true") : F("false");
  result += F(",\"nightMode\":");
  result += nightMode ? F("true") : F("false");
  result += F(",\"displayForcedOff\":");
  result += currentDisplayPowerStatusCallback != nullptr &&
                    currentDisplayPowerStatusCallback()
                ? F("true")
                : F("false");
  result += F(",\"uptimeMs\":");
  result += millis();
  result += F(",\"currentMemory\":");
  appendMemoryJson(result, networkDiagnosticsCurrentMemory());
  result += F(",\"minimumMemory\":{\"internalFree\":");
  result += static_cast<unsigned long>(heap_caps_get_minimum_free_size(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  result += F(",\"psramFree\":");
  result += static_cast<unsigned long>(
      heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  result += '}';
  result += F(",\"chmiRadar\":{\"active\":");
  result += radar.active ? F("true") : F("false");
  result += F(",\"loading\":");
  result += radar.loading ? F("true") : F("false");
  result += F(",\"ready\":");
  result += radar.ready ? F("true") : F("false");
  result += F(",\"preparationInProgress\":");
  result += radar.preparationInProgress ? F("true") : F("false");
  result += F(",\"available\":");
  result += clockConfigRadarAvailable(config) ? F("true") : F("false");
  result += F(",\"location\":\"");
  result += jsonEscape(config.openMeteoCity);
  result += F("\",\"latitude\":");
  result += String(config.openMeteoLatitude, 5);
  result += F(",\"longitude\":");
  result += String(config.openMeteoLongitude, 5);
  result += F(",\"radiusKm\":");
  result += radar.radiusKm;
  result += F(",\"requestedFrameCount\":");
  result += radar.requestedFrameCount;
  result += F(",\"preparedFrameCount\":");
  result += radar.preparedFrameCount;
  result += F(",\"animationFrameCount\":");
  result += radar.animationFrameCount;
  result += F(",\"pendingRefreshCount\":");
  result += radar.pendingRefreshCount;
  result += F(",\"lastSuccessfulRefreshAvailable\":");
  result += radar.lastSuccessfulRefreshAvailable ? F("true") : F("false");
  result += F(",\"lastSuccessfulRefreshAgeMs\":");
  result += radar.lastSuccessfulRefreshAgeMs;
  result += F(",\"nextRefreshInMs\":");
  result += radar.nextRefreshInMs;
  result += F(",\"lastHttpStatus\":");
  result += radar.lastHttpStatus;
  result += F(",\"lastDownloadedBytes\":");
  result += static_cast<unsigned long>(radar.lastDownloadedBytes);
  result += F(",\"lastDecodeResult\":");
  result += radar.lastDecodeResult;
  result += F(",\"lastDecodedLineCount\":");
  result += radar.lastDecodedLineCount;
  result += F(",\"acceptedCompleteDecodeError\":");
  result += radar.acceptedCompleteDecodeError ? F("true") : F("false");
  result += F(",\"latestIndexFile\":\"");
  result += jsonEscape(radar.latestIndexFile);
  result += F("\",\"currentFile\":\"");
  result += jsonEscape(radar.currentFile);
  result += F("\",\"oldestFrameTime\":\"");
  result += jsonEscape(radar.oldestFrameTime);
  result += F("\",\"newestFrameTime\":\"");
  result += jsonEscape(radar.newestFrameTime);
  result += F("\",\"message\":\"");
  result += jsonEscape(radar.message);
  result += F("\"}");
  result += F(",\"homeAssistantRuntime\":");
  appendDiagnosticJson(
      result, networkDiagnosticsSnapshot(
                  NetworkDiagnosticKind::HomeAssistantRuntime));
  result += F(",\"homeAssistantTest\":");
  appendDiagnosticJson(
      result,
      networkDiagnosticsSnapshot(NetworkDiagnosticKind::HomeAssistantTest));
  result += F(",\"weatherAnimation\":");
  appendDiagnosticJson(
      result,
      networkDiagnosticsSnapshot(NetworkDiagnosticKind::WeatherAnimation));
  result += F(",\"openMeteoRuntime\":");
  appendDiagnosticJson(
      result,
      networkDiagnosticsSnapshot(NetworkDiagnosticKind::OpenMeteoRuntime));
  result += F(",\"openMeteoTest\":");
  appendDiagnosticJson(
      result, networkDiagnosticsSnapshot(NetworkDiagnosticKind::OpenMeteoTest));
  result += '}';
  sendJson(200, result);
}

void handleDayNightRefresh() {
  if (homeAssistantRefreshCallback == nullptr ||
      !homeAssistantRefreshCallback()) {
    sendError(503, F("Home Assistant refresh není nyní dostupný."));
    return;
  }
  sendJson(202,
           F("{\"ok\":true,\"message\":\"Okamžitý refresh byl spuštěn.\"}"));
}

void handleControlRequest() {
  const String uri = server.uri();
  constexpr char PREFIX[] = "/api/control/";
  if (!uri.startsWith(PREFIX)) {
    sendError(404, F("Stránka nebyla nalezena."));
    return;
  }
  if (server.method() != HTTP_POST) {
    sendError(405, F("Tento příkaz vyžaduje metodu POST."));
    return;
  }
  const int secretStart = strlen(PREFIX);
  const int secretEnd = uri.indexOf('/', secretStart);
  if (secretEnd < 0 ||
      !controlSecretMatches(uri.substring(secretStart, secretEnd))) {
    sendError(401, F("Neplatný secret ovládacího API."));
    return;
  }
  const String command = uri.substring(secretEnd);
  if (command == "/display/off" || command == "/display/on") {
    if (currentDisplayPowerCallback == nullptr) {
      sendError(503, F("Ovládání displeje není nyní dostupné."));
      return;
    }
    const bool forcedOff = command.endsWith("/off");
    currentDisplayPowerCallback(forcedOff);
    sendJson(200, forcedOff
                      ? F("{\"ok\":true,\"display\":\"off\"}")
                      : F("{\"ok\":true,\"display\":\"on\"}"));
    return;
  }
  if (command == "/day-night/refresh") {
    handleDayNightRefresh();
    return;
  }
  sendError(404, F("Příkaz ovládacího API neexistuje."));
}

void handleRestart() {
  sendJson(200, F("{\"ok\":true}"));
  delay(250);
  ESP.restart();
}

void handleFirmwareStatus() {
  extendWebAvailability();
  const FirmwareUpdateSnapshot snapshot = firmwareUpdateServiceSnapshot();
  String result;
  result.reserve(600);
  result = F("{\"ok\":true,\"state\":\"");
  result += firmwareUpdateStateName(snapshot.state);
  result += F("\",\"currentVersion\":\"");
  result += jsonEscape(snapshot.currentVersion);
  result += F("\",\"serverVersion\":\"");
  result += jsonEscape(snapshot.serverVersion);
  result += F("\",\"message\":\"");
  result += jsonEscape(snapshot.message);
  result += F("\",\"downloadedBytes\":");
  result += snapshot.downloadedBytes;
  result += F(",\"totalBytes\":");
  result += snapshot.totalBytes;
  result += F(",\"updateAvailable\":");
  result += snapshot.updateAvailable ? F("true") : F("false");
  result += F(",\"busy\":");
  result += snapshot.busy ? F("true") : F("false");
  result += F(",\"installationSupported\":");
  result += snapshot.installationSupported ? F("true") : F("false");
  result += '}';
  sendJson(200, result);
}

void handleFirmwareCheck() {
  if (!firmwareUpdateServiceRequestCheck(false)) {
    sendError(409, F("Kontrola nebo aktualizace už probíhá."));
    return;
  }
  sendJson(202, F("{\"ok\":true,\"message\":\"Kontrola byla spuštěna.\"}"));
}

void handleFirmwareInstall() {
  const FirmwareUpdateSnapshot snapshot = firmwareUpdateServiceSnapshot();
  if (!snapshot.installationSupported) {
    sendError(409, F("Development build se aktualizuje pouze přes USB."));
    return;
  }
  if (!firmwareUpdateServiceRequestCheck(true)) {
    sendError(409, F("Kontrola nebo aktualizace už probíhá."));
    return;
  }
  sendJson(202,
           F("{\"ok\":true,\"message\":\"Kontrola a aktualizace byly spuštěny.\"}"));
}

bool requireAcceptedPostBody() {
  if (server.header("Content-Type").startsWith("multipart/")) {
    sendError(415, F("Formát multipart není podporovaný."));
    return false;
  }
  if (server.postBodyAccepted()) return true;
  if (server.postBodyTooLarge()) {
    sendError(413, F("Požadavek je příliš velký."));
  } else if (server.postBodyMalformed()) {
    sendError(400, F("Formulář obsahuje příliš dlouhé nebo neplatné pole."));
  } else {
    sendError(503, F("Pro zpracování požadavku není dost paměti."));
  }
  return false;
}

class BoundedPostRequestHandler final : public RequestHandler {
 public:
  BoundedPostRequestHandler(const char *uri,
                            WebServer::THandlerFunction handler)
      : uri_(uri), handler_(handler) {}

  bool canHandle(WebServer &, HTTPMethod method, const String &uri) override {
    return method == HTTP_POST && uri == uri_;
  }

  bool canRaw(WebServer &, const String &uri) override { return uri == uri_; }

  bool handle(WebServer &, HTTPMethod, const String &) override {
    if (requireAcceptedPostBody()) handler_();
    return true;
  }

  void raw(WebServer &, const String &, HTTPRaw &raw) override {
    server.captureRawPost(raw);
  }

 private:
  String uri_;
  WebServer::THandlerFunction handler_;
};

void registerBoundedPost(const char *uri,
                         WebServer::THandlerFunction handler) {
  server.addHandler(new BoundedPostRequestHandler(uri, handler));
}

class ControlRequestHandler final : public RequestHandler {
 public:
  bool canHandle(WebServer &, HTTPMethod method, const String &uri) override {
    return method == HTTP_POST && uri.startsWith("/api/control/");
  }

  bool canRaw(WebServer &, const String &uri) override {
    return uri.startsWith("/api/control/");
  }

  bool handle(WebServer &, HTTPMethod, const String &) override {
    if (requireAcceptedPostBody()) handleControlRequest();
    return true;
  }

  void raw(WebServer &, const String &, HTTPRaw &raw) override {
    server.captureRawPost(raw);
  }
};
}  // namespace

void configurationWebBegin(ClockConfigLoadCallback loadCallback,
                           ClockConfigSaveCallback saveCallback,
                           ConfigurationWebStatusCallback statusCallback,
                           SunTransitionTimesCallback sunTimesCallback,
                           HomeAssistantRefreshCallback refreshCallback,
                           DayNightStatusCallback dayNightStatusCallback,
                           DisplayPowerCallback displayPowerCallback,
                           DisplayPowerStatusCallback displayPowerStatusCallback,
                           RadarRangeStateCallback radarRangeStateCallback,
                           RadarRangePreviewCallback radarRangePreviewCallback,
                           ClockAppearanceStateCallback appearanceStateCallback,
                           ClockAppearanceChangeCallback appearancePreviewCallback,
                           ClockAppearanceChangeCallback appearanceSaveCallback) {
  configLoadCallback = loadCallback;
  configSaveCallback = saveCallback;
  webStatusCallback = statusCallback;
  sunTransitionTimesCallback = sunTimesCallback;
  homeAssistantRefreshCallback = refreshCallback;
  currentDayNightStatusCallback = dayNightStatusCallback;
  currentDisplayPowerCallback = displayPowerCallback;
  currentDisplayPowerStatusCallback = displayPowerStatusCallback;
  currentRadarRangeStateCallback = radarRangeStateCallback;
  currentRadarRangePreviewCallback = radarRangePreviewCallback;
  currentAppearanceStateCallback = appearanceStateCallback;
  currentAppearancePreviewCallback = appearancePreviewCallback;
  currentAppearanceSaveCallback = appearanceSaveCallback;
  initializeControlSecret();
  initializeWebPassword();
  server.beginBoundedPostSupport();
  Preferences preferences;
  if (preferences.begin(WEB_PREFS_NAMESPACE, true, "clockcfg")) {
    selectedWebMode = static_cast<ConfigurationWebMode>(constrain(
        preferences.getUChar(WEB_PREFS_KEY, CONFIGURATION_WEB_ALWAYS),
        static_cast<uint8_t>(CONFIGURATION_WEB_TIMED),
        static_cast<uint8_t>(CONFIGURATION_WEB_DISABLED)));
    preferences.end();
  }
  const char *collectedHeaders[] = {"Cookie", "Origin", "Content-Type",
                                    "Accept-Language"};
  server.collectHeaders(collectedHeaders, 4);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/ui-language.js", HTTP_GET, []() {
    addSecurityHeaders();
    server.send_P(200, PSTR("text/javascript; charset=utf-8"),
                  CONFIGURATION_LOCALIZATION_JS);
  });
  server.on("/diagnostics", HTTP_GET, handleDiagnosticPage);
  registerBoundedPost("/api/auth/login", handleWebLogin);
  registerBoundedPost("/api/web-password", []() {
    if (requireConfigurationAccess()) handleWebPassword();
  });
  server.on("/api/config", HTTP_GET, []() {
    if (requireConfigurationAccess()) handleGetConfig();
  });
  registerBoundedPost("/api/config", []() {
    if (requireConfigurationAccess()) handleSaveConfig();
  });
  registerBoundedPost("/api/language", []() {
    if (requireConfigurationAccess()) handleSaveLanguage();
  });
  registerBoundedPost("/api/ha/test", []() {
    if (requireConfigurationAccess()) handleTestConnection();
  });
  registerBoundedPost("/api/open-meteo/location", []() {
    if (requireConfigurationAccess()) handleOpenMeteoLocation();
  });
  server.on("/api/radar/state", HTTP_GET, []() {
    if (requireConfigurationAccess()) handleRadarRangeState();
  });
  registerBoundedPost("/api/radar/preview", []() {
    if (requireConfigurationAccess()) handleRadarRangePreview();
  });
  registerBoundedPost("/api/clock-appearance/preview", []() {
    if (requireConfigurationAccess()) handleClockAppearancePreview();
  });
  registerBoundedPost("/api/restart", []() {
    if (requireConfigurationAccess()) handleRestart();
  });
  server.on("/api/firmware", HTTP_GET, []() {
    if (requireConfigurationAccess()) handleFirmwareStatus();
  });
  registerBoundedPost("/api/firmware/check", []() {
    if (requireConfigurationAccess()) handleFirmwareCheck();
  });
  registerBoundedPost("/api/firmware/install", []() {
    if (requireConfigurationAccess()) handleFirmwareInstall();
  });
  server.on("/api/update-status", HTTP_GET, []() {
    if (requireConfigurationAccess()) handleFirmwareStatus();
  });
  registerBoundedPost("/api/check-update", []() {
    if (requireConfigurationAccess()) handleFirmwareCheck();
  });
  registerBoundedPost("/api/install-update", []() {
    if (requireConfigurationAccess()) handleFirmwareInstall();
  });
  server.on("/api/diagnostics", HTTP_GET, handleDiagnostics);
  server.on("/api/status", HTTP_GET, handleDiagnostics);
  server.on("/api/runtime", HTTP_GET, handleDiagnostics);
  server.addHandler(new ControlRequestHandler());
  server.onNotFound(handleControlRequest);
  server.begin();
  if (selectedWebMode != CONFIGURATION_WEB_DISABLED) {
    unlockConfiguration(true);
  } else {
    notifyWebStatus();
  }
}

void configurationWebLoop() {
  server.handleClient();
  if (selectedWebMode == CONFIGURATION_WEB_TIMED &&
      webActive && static_cast<long>(millis() - webAvailableUntil) >= 0) {
    lockConfiguration();
  }
}

void configurationWebEnsureActive() {
  if (selectedWebMode != CONFIGURATION_WEB_DISABLED && !webActive)
    unlockConfiguration(true);
}

void configurationWebExtendAvailability() {
  if (selectedWebMode != CONFIGURATION_WEB_DISABLED)
    unlockConfiguration(true);
}

ConfigurationWebMode configurationWebMode() { return selectedWebMode; }

bool configurationWebSetMode(ConfigurationWebMode mode) {
  if (mode > CONFIGURATION_WEB_DISABLED) return false;
  if (!persistWebMode(mode)) return false;
  applyWebMode(mode);
  return true;
}

void configurationWebLockForTest() { lockConfiguration(); }

void configurationWebUnlockForTest() {
  if (selectedWebMode != CONFIGURATION_WEB_DISABLED)
    unlockConfiguration(true);
}
