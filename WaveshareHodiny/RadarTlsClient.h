#pragma once
#include <WiFiClientSecure.h>
#include <atomic>
#include <lwip/dns.h>
#include <lwip/tcpip.h>

// DNS callbacks outlive a timed-out request, so their state has static lifetime.
// Only the single radar worker submits requests; DNS runs on the TCP/IP thread.
namespace radarDns {
struct Lookup {
  std::atomic<unsigned> state{0}; // idle, pending, completed
  ip_addr_t address{};
  bool valid = false;
};
inline Lookup &lookup() { static Lookup value; return value; }
inline void found(const char *, const ip_addr_t *address, void *) {
  auto &q = lookup();
  q.valid = address && IP_IS_V4(address);
  if (q.valid) q.address = *address;
  q.state.store(2, std::memory_order_release);
}
inline void start(void *) {
  ip_addr_t address{};
  const err_t result = dns_gethostbyname("radar.nebovidy.cz", &address, found, nullptr);
  if (result != ERR_INPROGRESS) found(nullptr, result == ERR_OK ? &address : nullptr, nullptr);
}
inline bool resolve(IPAddress &address, uint32_t budget) {
  auto &q = lookup();
  // Discard a previous late answer; ask lwIP again to respect its DNS cache TTL.
  if (q.state.load(std::memory_order_acquire) == 2) q.state.store(0);
  if (q.state.load() != 0) return false;
  q.state.store(1);
  if (tcpip_try_callback(start, nullptr) != ERR_OK) { q.state.store(0); return false; }
  const uint32_t began = millis();
  while (q.state.load(std::memory_order_acquire) != 2 && millis() - began < budget) delay(1);
  if (q.state.load(std::memory_order_acquire) != 2) return false;
  const bool valid = q.valid;
  if (valid) address = IPAddress(ip_2_ip4(&q.address)->addr);
  q.state.store(0);
  return valid;
}
}
class RadarTlsClient : public WiFiClientSecure {
 public:
  uint32_t connections = 0;
  bool bounded = false;
  uint32_t began = 0;
  static constexpr uint32_t BUDGET_MS = 3000;
  uint32_t remaining() const {
    const uint32_t elapsed = millis() - began;
    return elapsed >= BUDGET_MS ? 0 : BUDGET_MS - elapsed;
  }
  bool expired() {
    if (bounded && !remaining()) { Stream::setTimeout(0); WiFiClientSecure::stop(); return true; }
    return false;
  }
  void requestDeadline(bool enabled) { bounded = enabled; began = millis(); }
  int connect(const char *host, uint16_t port, int32_t timeout) override {
    ++connections;
    if (!bounded) { _stillinPlainStart = false; return WiFiClientSecure::connect(host, port, timeout); }
    IPAddress address;
    if (strcmp(host, "radar.nebovidy.cz") || !radarDns::resolve(address, min(remaining(), uint32_t(750))) || expired()) return 0;
    _timeout = min(remaining(), uint32_t(1000));
    // Keep hostname verification and SNI even though DNS was resolved separately.
    _stillinPlainStart = true;
    if (!WiFiClientSecure::connect(address, port, host, _CA_cert, _cert, _private_key) || expired()) return 0;
    sslclient->handshake_timeout = remaining();
    if (!startTLS() || expired()) return 0;
    return 1;
  }
  int available() override { return expired() ? 0 : WiFiClientSecure::available(); }
  uint8_t connected() override { return expired() ? 0 : WiFiClientSecure::connected(); }
  int read() override { return expired() ? -1 : WiFiClientSecure::read(); }
  int read(uint8_t *buffer, size_t size) override { return expired() ? -1 : WiFiClientSecure::read(buffer, size); }
  int peek() override { return expired() ? -1 : WiFiClientSecure::peek(); }
  size_t write(const uint8_t *buffer, size_t size) override {
    if (expired()) return 0;
    if (bounded) { _timeout = remaining(); sslclient->socket_timeout = _timeout; }
    return WiFiClientSecure::write(buffer, size);
  }
  size_t write(uint8_t value) override { return write(&value, 1); }
};
