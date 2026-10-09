#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

// NRD2 stores independent compressed rows of firmware palette indices.
// The caller verifies the complete file's SHA-256 and manifest timestamp.
namespace nrd2 {
constexpr size_t MAX_BYTES = 446086;
constexpr unsigned WIDTH = 800;
constexpr unsigned HEIGHT = 550;
constexpr size_t DATA_OFFSET = 2236;

inline uint16_t u16(const uint8_t *p) {
  return uint16_t(p[0]) | uint16_t(p[1]) << 8;
}
inline uint32_t u32(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 |
         uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

struct Frame {
  const uint8_t *data = nullptr;
  size_t size = 0;
  uint32_t measured = 0;

  bool open(const uint8_t *p, size_t n) {
    data = nullptr;
    size = 0;
    measured = 0;
    if (!p || n < DATA_OFFSET || n > MAX_BYTES ||
        memcmp(p, "NRD2", 4) != 0 || u16(p + 4) != 2 ||
        u16(p + 6) != 32 || u16(p + 8) != WIDTH ||
        u16(p + 10) != HEIGHT || u32(p + 16) != 1 ||
        u32(p + 20) != 1 || u32(p + 24) != DATA_OFFSET ||
        u32(p + 28) != n - DATA_OFFSET) return false;
    if (u32(p + 32) != 0 ||
        u32(p + 32 + HEIGHT * 4) != n - DATA_OFFSET) return false;
    for (unsigned y = 0; y < HEIGHT; ++y) {
      const uint32_t begin = u32(p + 32 + 4 * y);
      const uint32_t end = u32(p + 36 + 4 * y);
      if (begin >= end || end > n - DATA_OFFSET) return false;
    }
    measured = u32(p + 12);
    data = p;
    size = n;
    return true;
  }

  // out must hold WIDTH bytes. Reject runs crossing either row boundary.
  bool validateRow(unsigned y) const { return processRow(y, nullptr); }
  bool row(unsigned y, uint8_t *out) const {
    return out && processRow(y, out);
  }

 private:
  bool processRow(unsigned y, uint8_t *out) const {
    if (!data || y >= HEIGHT) return false;
    size_t pos = DATA_OFFSET + u32(data + 32 + 4 * y);
    const size_t end = DATA_OFFSET + u32(data + 36 + 4 * y);
    size_t x = 0;
    while (pos < end) {
      const unsigned token = data[pos++];
      const unsigned count = (token & 127) + 1;
      const unsigned bytes = (token & 128) ? 1 : count;
      if (count > WIDTH - x || bytes > end - pos) return false;
      for (unsigned i = 0; i < bytes; ++i)
        if (data[pos + i] >= 158 && data[pos + i] != 255) return false;
      if (out) {
        if (token & 128) memset(out + x, data[pos], count);
        else memcpy(out + x, data + pos, count);
      }
      pos += bytes;
      x += count;
    }
    return x == WIDTH;
  }
};
}  // namespace nrd2
