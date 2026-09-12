#include "HourlyForecastService.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "FirmwareHubCa.h"
#include "NetworkCoordinator.h"

namespace {
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

portMUX_TYPE forecastMux = portMUX_INITIALIZER_UNLOCKED;
HourlyForecast cached;
float lastLatitude = 1000, lastLongitude = 1000;
unsigned long nextFetch = 0;
}
HourlyForecast hourlyForecastSnapshot() {
  portENTER_CRITICAL(&forecastMux);
  const HourlyForecast result = cached;
  portEXIT_CRITICAL(&forecastMux);
  return result;
}
void hourlyForecastRefresh(const ClockConfig &config) {
  if (lastLatitude != config.openMeteoLatitude || lastLongitude != config.openMeteoLongitude) {
    lastLatitude = config.openMeteoLatitude;
    lastLongitude = config.openMeteoLongitude;
    portENTER_CRITICAL(&forecastMux);
    cached = HourlyForecast{};
    portEXIT_CRITICAL(&forecastMux);
    nextFetch = 0;
  }
  if (WiFi.status() != WL_CONNECTED ||
      (nextFetch && static_cast<long>(millis() - nextFetch) < 0)) return;
  nextFetch = millis() + 60000;
  NetworkOperationGuard guard(5000);
  if (!guard) return;
  String url = F("https://api.open-meteo.com/v1/forecast?latitude=");
  url += String(config.openMeteoLatitude, 5);
  url += F("&longitude=");
  url += String(config.openMeteoLongitude, 5);
  // UTC epochs avoid ambiguous local timestamps when daylight saving time changes.
  url += F("&hourly=temperature_2m,weather_code,is_day&forecast_hours=24&timeformat=unixtime&timezone=auto");
  WiFiClientSecure client;
  client.setCACert(FIRMWARE_RELEASE_ROOT_CA);
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  if (!http.begin(client, url)) return;
  const int status = http.GET();
  HourlyForecast parsed;
  bool ok = false;
  if (status == HTTP_CODE_OK && http.getSize() <= 12000) {
    String body;
    BoundedStringStream stream(body, 12000);
    const int bytes = http.writeToStream(&stream);
    ok = bytes >= 0 && !stream.overflowed() && !stream.allocationFailed() &&
         parseHourlyForecast(body.c_str(), parsed);
  }
  http.end();
  if (!ok) return;
  parsed.fetchedAt = time(nullptr);
  // A successful HTTPS response can arrive before the first NTP synchronization.
  // Its first forecast hour is a conservative server-time freshness fallback.
  if (parsed.fetchedAt < 1577836800) parsed.fetchedAt = parsed.hours[0].timestamp;
  portENTER_CRITICAL(&forecastMux);
  cached = parsed;
  portEXIT_CRITICAL(&forecastMux);
  nextFetch = millis() + 600000;
}
