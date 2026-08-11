#include "SemVer.h"

#include <cctype>
#include <cstdint>
#include <cstring>

namespace {
struct Identifier {
  const char *start = nullptr;
  size_t length = 0;
  bool numeric = false;
};

struct Version {
  uint32_t major = 0;
  uint32_t minor = 0;
  uint32_t patch = 0;
  const char *prerelease = nullptr;
  const char *prereleaseEnd = nullptr;
};

bool parseNumber(const char *&cursor, uint32_t &value) {
  const char *start = cursor;
  if (!std::isdigit(static_cast<unsigned char>(*cursor))) return false;
  if (*cursor == '0' && std::isdigit(static_cast<unsigned char>(cursor[1]))) {
    return false;
  }
  uint64_t parsed = 0;
  while (std::isdigit(static_cast<unsigned char>(*cursor))) {
    parsed = parsed * 10 + static_cast<unsigned>(*cursor - '0');
    if (parsed > UINT32_MAX) return false;
    ++cursor;
  }
  if (cursor == start) return false;
  value = static_cast<uint32_t>(parsed);
  return true;
}

bool validIdentifiers(const char *start, const char *end,
                      bool rejectNumericLeadingZero) {
  if (start == end) return false;
  const char *identifierStart = start;
  bool numeric = true;
  for (const char *cursor = start; cursor <= end; ++cursor) {
    if (cursor == end || *cursor == '.') {
      if (cursor == identifierStart) return false;
      if (rejectNumericLeadingZero && numeric &&
          cursor - identifierStart > 1 && *identifierStart == '0') {
        return false;
      }
      identifierStart = cursor + 1;
      numeric = true;
      continue;
    }
    const unsigned char character = static_cast<unsigned char>(*cursor);
    if (!std::isalnum(character) && character != '-') return false;
    if (!std::isdigit(character)) numeric = false;
  }
  return true;
}

bool parseVersion(const char *text, Version &version) {
  if (text == nullptr || *text == '\0') return false;
  const char *cursor = text;
  if (!parseNumber(cursor, version.major) || *cursor++ != '.' ||
      !parseNumber(cursor, version.minor) || *cursor++ != '.' ||
      !parseNumber(cursor, version.patch)) {
    return false;
  }

  if (*cursor == '-') {
    version.prerelease = ++cursor;
    while (*cursor != '\0' && *cursor != '+') ++cursor;
    version.prereleaseEnd = cursor;
    if (!validIdentifiers(version.prerelease, version.prereleaseEnd, true)) {
      return false;
    }
  }
  if (*cursor == '+') {
    const char *build = ++cursor;
    while (*cursor != '\0') ++cursor;
    if (!validIdentifiers(build, cursor, false)) return false;
  }
  return *cursor == '\0';
}

Identifier nextIdentifier(const char *&cursor, const char *end) {
  Identifier identifier;
  identifier.start = cursor;
  identifier.numeric = true;
  while (cursor < end && *cursor != '.') {
    if (!std::isdigit(static_cast<unsigned char>(*cursor))) {
      identifier.numeric = false;
    }
    ++cursor;
  }
  identifier.length = static_cast<size_t>(cursor - identifier.start);
  if (cursor < end) ++cursor;
  return identifier;
}

int compareIdentifier(const Identifier &left, const Identifier &right) {
  if (left.numeric != right.numeric) return left.numeric ? -1 : 1;
  if (left.numeric && left.length != right.length) {
    return left.length < right.length ? -1 : 1;
  }
  const size_t commonLength = left.length < right.length ? left.length : right.length;
  const int compared = std::memcmp(left.start, right.start, commonLength);
  if (compared != 0) return compared < 0 ? -1 : 1;
  if (left.length == right.length) return 0;
  return left.length < right.length ? -1 : 1;
}
}  // namespace

bool semVerIsValid(const char *version) {
  Version parsed;
  return parseVersion(version, parsed);
}

int semVerCompare(const char *left, const char *right) {
  Version leftVersion;
  Version rightVersion;
  if (!parseVersion(left, leftVersion) || !parseVersion(right, rightVersion)) {
    return 0;
  }
  if (leftVersion.major != rightVersion.major) {
    return leftVersion.major < rightVersion.major ? -1 : 1;
  }
  if (leftVersion.minor != rightVersion.minor) {
    return leftVersion.minor < rightVersion.minor ? -1 : 1;
  }
  if (leftVersion.patch != rightVersion.patch) {
    return leftVersion.patch < rightVersion.patch ? -1 : 1;
  }
  const bool leftStable = leftVersion.prerelease == nullptr;
  const bool rightStable = rightVersion.prerelease == nullptr;
  if (leftStable != rightStable) return leftStable ? 1 : -1;
  if (leftStable) return 0;

  const char *leftCursor = leftVersion.prerelease;
  const char *rightCursor = rightVersion.prerelease;
  while (leftCursor < leftVersion.prereleaseEnd &&
         rightCursor < rightVersion.prereleaseEnd) {
    const int compared = compareIdentifier(
        nextIdentifier(leftCursor, leftVersion.prereleaseEnd),
        nextIdentifier(rightCursor, rightVersion.prereleaseEnd));
    if (compared != 0) return compared;
  }
  if (leftCursor == leftVersion.prereleaseEnd &&
      rightCursor == rightVersion.prereleaseEnd) {
    return 0;
  }
  return leftCursor == leftVersion.prereleaseEnd ? -1 : 1;
}
