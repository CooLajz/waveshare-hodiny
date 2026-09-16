#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

constexpr uint32_t NOTIFICATION_MAX_SECONDS = 86400;
constexpr uint32_t NOTIFICATION_MAX_BEEP_MS = 5000;
constexpr size_t NOTIFICATION_TITLE_BYTES = 96;
constexpr size_t NOTIFICATION_MESSAGE_BYTES = 768;

inline bool notificationParseColor(const char *text, uint32_t &color) {
  if (!text || strlen(text) != 7 || text[0] != '#') return false;
  uint32_t result = 0;
  for (size_t i = 1; i < 7; ++i) {
    const char c = text[i];
    const int digit = c >= '0' && c <= '9' ? c - '0' :
        c >= 'a' && c <= 'f' ? c - 'a' + 10 :
        c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
    if (digit < 0) return false;
    result = (result << 4) | digit;
  }
  color = result;
  return true;
}

inline bool notificationParseSeconds(const char *text, uint32_t &seconds) {
  if (!text || !*text) return false;
  uint32_t result = 0;
  for (; *text; ++text) {
    if (*text < '0' || *text > '9') return false;
    const uint32_t digit = *text - '0';
    if (result > (NOTIFICATION_MAX_SECONDS - digit) / 10) return false;
    result = result * 10 + digit;
  }
  seconds = result;
  return true;
}

inline bool notificationParseBeep(const char *text, uint32_t &milliseconds) {
  uint32_t parsed = 0;
  if (!notificationParseSeconds(text, parsed) || parsed > NOTIFICATION_MAX_BEEP_MS)
    return false;
  milliseconds = parsed;
  return true;
}

// Reject malformed UTF-8 and control characters; messages may contain newlines.
inline bool notificationValidText(const char *text, size_t maxBytes,
                                  bool multiline) {
  if (!text) return false;
  const size_t length = strlen(text);
  if (!length || length > maxBytes) return false;
  bool visible = false;
  for (size_t i = 0; i < length;) {
    const uint8_t first = static_cast<uint8_t>(text[i++]);
    if (first < 0x80) {
      if ((first < 0x20 && !(multiline && first == '\n')) || first == 0x7f)
        return false;
      if (first > 0x20) visible = true;
      continue;
    }
    const int extra = first >= 0xc2 && first <= 0xdf ? 1 :
        first >= 0xe0 && first <= 0xef ? 2 :
        first >= 0xf0 && first <= 0xf4 ? 3 : -1;
    if (extra < 0 || i + extra > length) return false;
    uint32_t codepoint = first & ((1 << (6 - extra)) - 1);
    for (int j = 0; j < extra; ++j) {
      const uint8_t next = static_cast<uint8_t>(text[i++]);
      if ((next & 0xc0) != 0x80) return false;
      codepoint = (codepoint << 6) | (next & 0x3f);
    }
    if ((extra == 2 && codepoint < 0x800) ||
        (extra == 3 && codepoint < 0x10000) || codepoint > 0x10ffff ||
        (codepoint >= 0xd800 && codepoint <= 0xdfff) ||
        (codepoint >= 0x80 && codepoint <= 0x9f)) return false;
    visible = true;
  }
  return visible;
}

class NotificationLifetime {
 public:
  void show(uint32_t now, uint32_t seconds) {
    active = true;
    started = now;
    duration = seconds * 1000U;
  }
  bool expired(uint32_t now) const {
    return active && duration && uint32_t(now - started) >= duration;
  }
  bool active = false;
 private:
  uint32_t started = 0;
  uint32_t duration = 0;
};
