#pragma once
#include <cstddef>
#include <cstdint>

namespace radarHttp {
// readSome returns available bytes, 0 at EOF, or -1 on timeout/cancellation.
// It owns waiting/yielding. This parser never treats a short read as EOF.
template <typename Read, typename Sink>
bool readBody(Read readSome, Sink &sink, int declared, bool chunked) {
  uint8_t buffer[1024];
  auto exact = [&](uint8_t *out, size_t size) {
    while (size) {
      int n = readSome(out, size);
      if (n <= 0 || static_cast<size_t>(n) > size) return false;
      out += n;
      size -= n;
    }
    return true;
  };
  auto line = [&](char *out, size_t capacity) {
    size_t used = 0;
    while (used + 1 < capacity) {
      uint8_t c;
      if (!exact(&c, 1)) return false;
      if (c == '\r') {
        if (!exact(&c, 1) || c != '\n') return false;
        out[used] = '\0';
        return true;
      }
      if (c < 32 || c == 127) return false;
      out[used++] = c;
    }
    return false;
  };
  auto payload = [&](size_t size) {
    if (size > sink.limit - sink.received) return false;
    while (size) {
      const size_t wanted = size < sizeof(buffer) ? size : sizeof(buffer);
      int n = readSome(buffer, wanted);
      if (n <= 0 || static_cast<size_t>(n) > wanted ||
          sink.write(buffer, n) != static_cast<size_t>(n)) return false;
      size -= n;
    }
    return true;
  };
  if (chunked) {
    // Conflicting framing must not be reused for another request.
    if (declared >= 0) return false;
    char header[128];
    while (true) {
      if (!line(header, sizeof(header))) return false;
      size_t size = 0, i = 0;
      for (; header[i] && header[i] != ';'; ++i) {
        const char c = header[i];
        const int digit = c >= '0' && c <= '9' ? c - '0' :
            c >= 'a' && c <= 'f' ? c - 'a' + 10 :
            c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (digit < 0 || size > (SIZE_MAX - digit) / 16) return false;
        size = size * 16 + digit;
      }
      if (i == 0) return false;
      if (!size) {
        // Consume the entire terminating chunk, including optional trailers.
        for (size_t lines = 0; lines < 16; ++lines) {
          if (!line(header, sizeof(header))) return false;
          if (!header[0]) return true;
        }
        return false;
      }
      if (!payload(size)) return false;
      uint8_t crlf[2];
      if (!exact(crlf, 2) || crlf[0] != '\r' || crlf[1] != '\n') return false;
    }
  }
  if (declared >= 0) return payload(static_cast<size_t>(declared));
  // A response without framing is only complete when the peer closes it.
  while (true) {
    int n = readSome(buffer, sizeof(buffer));
    if (n == 0) return true;
    if (n < 0 || static_cast<size_t>(n) > sizeof(buffer) ||
        sink.write(buffer, n) != static_cast<size_t>(n)) return false;
  }
}
}  // namespace radarHttp
