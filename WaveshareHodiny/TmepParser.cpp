#include "TmepParser.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {
void copyText(char *destination, size_t destinationSize, const char *source) {
  if (destinationSize == 0) return;
  size_t length = source == nullptr ? 0 : strlen(source);
  if (length >= destinationSize) length = destinationSize - 1;
  if (length > 0) memcpy(destination, source, length);
  destination[length] = '\0';
}

void setError(char *error, size_t errorSize, const char *message) {
  copyText(error, errorSize, message);
}

class JsonCursor {
 public:
  JsonCursor(const char *payload, size_t length)
      : payload_(payload), length_(length) {}

  bool atEnd() {
    skipWhitespace();
    return position_ == length_;
  }

  bool consume(char expected) {
    skipWhitespace();
    if (position_ >= length_ || payload_[position_] != expected) return false;
    ++position_;
    return true;
  }

  bool consumeLiteral(const char *literal) {
    skipWhitespace();
    const size_t literalLength = strlen(literal);
    if (literalLength > length_ - position_ ||
        memcmp(payload_ + position_, literal, literalLength) != 0)
      return false;
    position_ += literalLength;
    return true;
  }

  bool parseString(char *destination, size_t destinationSize) {
    skipWhitespace();
    if (position_ >= length_ || payload_[position_] != '"') return false;
    ++position_;
    size_t outputLength = 0;
    while (position_ < length_) {
      unsigned char character =
          static_cast<unsigned char>(payload_[position_++]);
      if (character == '"') {
        if (destinationSize > 0)
          destination[outputLength < destinationSize ? outputLength
                                                     : destinationSize - 1] =
              '\0';
        return true;
      }
      if (character == '\\') {
        if (position_ >= length_) return false;
        const char escaped = payload_[position_++];
        if (escaped == 'u') {
          uint32_t codePoint = 0;
          if (!parseHexCodePoint(codePoint)) return false;
          if (codePoint >= 0xD800 && codePoint <= 0xDBFF &&
              position_ + 2 <= length_ && payload_[position_] == '\\' &&
              payload_[position_ + 1] == 'u') {
            position_ += 2;
            uint32_t low = 0;
            if (!parseHexCodePoint(low) || low < 0xDC00 || low > 0xDFFF)
              return false;
            codePoint = 0x10000 + ((codePoint - 0xD800) << 10) +
                        (low - 0xDC00);
          }
          appendUtf8(codePoint, destination, destinationSize, outputLength);
          continue;
        }
        switch (escaped) {
          case '"':
          case '\\':
          case '/':
            character = static_cast<unsigned char>(escaped);
            break;
          case 'b': character = '\b'; break;
          case 'f': character = '\f'; break;
          case 'n': character = '\n'; break;
          case 'r': character = '\r'; break;
          case 't': character = '\t'; break;
          default: return false;
        }
      } else if (character < 0x20) {
        return false;
      }
      appendByte(character, destination, destinationSize, outputLength);
    }
    return false;
  }

  bool parseNumber(double &value, uint8_t &decimals) {
    skipWhitespace();
    if (position_ >= length_) return false;
    char *end = nullptr;
    value = strtod(payload_ + position_, &end);
    if (end == payload_ + position_ || !std::isfinite(value) ||
        end > payload_ + length_)
      return false;
    const size_t endPosition = static_cast<size_t>(end - payload_);
    decimals = 0;
    size_t decimalPosition = position_;
    while (decimalPosition < endPosition &&
           payload_[decimalPosition] != '.' &&
           payload_[decimalPosition] != 'e' &&
           payload_[decimalPosition] != 'E')
      ++decimalPosition;
    if (decimalPosition < endPosition && payload_[decimalPosition] == '.') {
      size_t digit = decimalPosition + 1;
      while (digit < endPosition && payload_[digit] >= '0' &&
             payload_[digit] <= '9') {
        if (decimals < 2) ++decimals;
        ++digit;
      }
    }
    position_ = endPosition;
    return true;
  }

  bool parseNullableNumber(TmepValue &destination) {
    skipWhitespace();
    if (consumeLiteral("null")) {
      destination.available = false;
      return true;
    }
    double value = 0;
    uint8_t decimals = 0;
    if (!parseNumber(value, decimals)) return false;
    destination.available = true;
    destination.value = static_cast<float>(value);
    destination.decimals = decimals;
    return true;
  }

  bool skipValue(uint8_t depth = 0) {
    if (depth > 8) return false;
    skipWhitespace();
    if (position_ >= length_) return false;
    if (payload_[position_] == '"') {
      char ignored[1];
      return parseString(ignored, sizeof(ignored));
    }
    if (payload_[position_] == '{') {
      ++position_;
      skipWhitespace();
      if (consume('}')) return true;
      while (true) {
        char key[1];
        if (!parseString(key, sizeof(key)) || !consume(':') ||
            !skipValue(depth + 1))
          return false;
        if (consume('}')) return true;
        if (!consume(',')) return false;
      }
    }
    if (payload_[position_] == '[') {
      ++position_;
      skipWhitespace();
      if (consume(']')) return true;
      while (true) {
        if (!skipValue(depth + 1)) return false;
        if (consume(']')) return true;
        if (!consume(',')) return false;
      }
    }
    if (consumeLiteral("true") || consumeLiteral("false") ||
        consumeLiteral("null"))
      return true;
    double ignored = 0;
    uint8_t decimals = 0;
    return parseNumber(ignored, decimals);
  }

 private:
  void skipWhitespace() {
    while (position_ < length_ &&
           (payload_[position_] == ' ' || payload_[position_] == '\t' ||
            payload_[position_] == '\r' || payload_[position_] == '\n'))
      ++position_;
  }

  bool parseHexCodePoint(uint32_t &value) {
    if (position_ + 4 > length_) return false;
    value = 0;
    for (size_t index = 0; index < 4; ++index) {
      const char digit = payload_[position_++];
      value <<= 4;
      if (digit >= '0' && digit <= '9')
        value |= static_cast<uint32_t>(digit - '0');
      else if (digit >= 'a' && digit <= 'f')
        value |= static_cast<uint32_t>(digit - 'a' + 10);
      else if (digit >= 'A' && digit <= 'F')
        value |= static_cast<uint32_t>(digit - 'A' + 10);
      else
        return false;
    }
    return true;
  }

  void appendByte(uint8_t byte, char *destination, size_t destinationSize,
                  size_t &outputLength) {
    if (destinationSize > 0 && outputLength + 1 < destinationSize)
      destination[outputLength] = static_cast<char>(byte);
    ++outputLength;
  }

  void appendUtf8(uint32_t codePoint, char *destination,
                  size_t destinationSize, size_t &outputLength) {
    if (codePoint <= 0x7F) {
      appendByte(static_cast<uint8_t>(codePoint), destination, destinationSize,
                 outputLength);
    } else if (codePoint <= 0x7FF) {
      appendByte(0xC0 | (codePoint >> 6), destination, destinationSize,
                 outputLength);
      appendByte(0x80 | (codePoint & 0x3F), destination, destinationSize,
                 outputLength);
    } else if (codePoint <= 0xFFFF) {
      appendByte(0xE0 | (codePoint >> 12), destination, destinationSize,
                 outputLength);
      appendByte(0x80 | ((codePoint >> 6) & 0x3F), destination,
                 destinationSize, outputLength);
      appendByte(0x80 | (codePoint & 0x3F), destination, destinationSize,
                 outputLength);
    } else if (codePoint <= 0x10FFFF) {
      appendByte(0xF0 | (codePoint >> 18), destination, destinationSize,
                 outputLength);
      appendByte(0x80 | ((codePoint >> 12) & 0x3F), destination,
                 destinationSize, outputLength);
      appendByte(0x80 | ((codePoint >> 6) & 0x3F), destination,
                 destinationSize, outputLength);
      appendByte(0x80 | (codePoint & 0x3F), destination, destinationSize,
                 outputLength);
    }
  }

  const char *payload_;
  size_t length_;
  size_t position_ = 0;
};

bool parseUnit(JsonCursor &cursor, TmepValue &value) {
  char unit[TMEP_VALUE_UNIT_LENGTH];
  if (cursor.consumeLiteral("null")) {
    value.unit[0] = '\0';
    return true;
  }
  if (!cursor.parseString(unit, sizeof(unit))) return false;
  copyText(value.unit, sizeof(value.unit), unit);
  return true;
}

bool parseNullableText(JsonCursor &cursor, char *destination,
                       size_t destinationSize) {
  if (cursor.consumeLiteral("null")) {
    if (destinationSize > 0) destination[0] = '\0';
    return true;
  }
  return cursor.parseString(destination, destinationSize);
}

bool parseSensor(JsonCursor &cursor, TmepSensor &sensor) {
  if (!cursor.consume('{')) return false;
  if (cursor.consume('}')) return true;
  while (true) {
    char key[40];
    if (!cursor.parseString(key, sizeof(key)) || !cursor.consume(':'))
      return false;
    if (strcmp(key, "nadpis") == 0) {
      if (!parseNullableText(cursor, sensor.title, sizeof(sensor.title)))
        return false;
    } else if (strcmp(key, "domena") == 0) {
      if (!parseNullableText(cursor, sensor.domain, sizeof(sensor.domain)))
        return false;
    } else if (strcmp(key, "umisteni") == 0) {
      if (!parseNullableText(cursor, sensor.location, sizeof(sensor.location)))
        return false;
    } else if (strcmp(key, "cas") == 0) {
      if (!parseNullableText(cursor, sensor.measuredAt,
                             sizeof(sensor.measuredAt)))
        return false;
    } else if (strcmp(key, "teplota") == 0) {
      if (!cursor.parseNullableNumber(sensor.temperature)) return false;
    } else if (strcmp(key, "vlhkost") == 0) {
      if (!cursor.parseNullableNumber(sensor.humidity)) return false;
    } else if (strcmp(key, "tlak") == 0) {
      if (!cursor.parseNullableNumber(sensor.pressure)) return false;
    } else if (strcmp(key, "rssi") == 0) {
      if (!cursor.parseNullableNumber(sensor.rssi)) return false;
      copyText(sensor.rssi.unit, sizeof(sensor.rssi.unit), "dBm");
    } else if (strcmp(key, "napeti") == 0) {
      if (!cursor.parseNullableNumber(sensor.voltage)) return false;
      copyText(sensor.voltage.unit, sizeof(sensor.voltage.unit), "V");
    } else if (strcmp(key, "teplota_jednotka") == 0) {
      if (!parseUnit(cursor, sensor.temperature)) return false;
    } else if (strcmp(key, "vlhkost_jednotka") == 0) {
      if (!parseUnit(cursor, sensor.humidity)) return false;
    } else if (strcmp(key, "tlak_jednotka") == 0) {
      if (!parseUnit(cursor, sensor.pressure)) return false;
    } else if (!cursor.skipValue()) {
      return false;
    }
    if (cursor.consume('}')) return true;
    if (!cursor.consume(',')) return false;
  }
}
}  // namespace

bool tmepParseExport(const char *payload, size_t length, TmepCatalog &catalog,
                     char *error, size_t errorSize) {
  catalog = TmepCatalog{};
  if (payload == nullptr || length == 0) {
    setError(error, errorSize, "TMEP vrátil prázdnou odpověď.");
    return false;
  }
  JsonCursor cursor(payload, length);
  if (!cursor.consume('{')) {
    setError(error, errorSize,
             "TMEP odmítl exportní klíč nebo nevrátil JSON.");
    return false;
  }
  if (cursor.consume('}')) {
    setError(error, errorSize, "TMEP export neobsahuje žádná čidla.");
    return false;
  }
  while (true) {
    char sensorId[16];
    if (!cursor.parseString(sensorId, sizeof(sensorId)) ||
        !cursor.consume(':')) {
      setError(error, errorSize, "TMEP vrátil neplatný JSON export.");
      return false;
    }
    if (catalog.count < TMEP_MAX_SENSORS) {
      TmepSensor &sensor = catalog.sensors[catalog.count];
      copyText(sensor.id, sizeof(sensor.id), sensorId);
      if (!parseSensor(cursor, sensor)) {
        setError(error, errorSize, "TMEP vrátil neplatný záznam čidla.");
        return false;
      }
      ++catalog.count;
    } else {
      catalog.truncated = true;
      if (!cursor.skipValue()) {
        setError(error, errorSize, "TMEP vrátil neplatný záznam čidla.");
        return false;
      }
    }
    if (cursor.consume('}')) break;
    if (!cursor.consume(',')) {
      setError(error, errorSize, "TMEP vrátil neplatný JSON export.");
      return false;
    }
  }
  if (!cursor.atEnd()) {
    setError(error, errorSize, "Za TMEP JSONem jsou neočekávaná data.");
    return false;
  }
  if (catalog.count == 0) {
    setError(error, errorSize, "TMEP export neobsahuje žádná čidla.");
    return false;
  }
  if (errorSize > 0) error[0] = '\0';
  return true;
}

const TmepSensor *tmepFindSensor(const TmepCatalog &catalog,
                                const char *sensorId) {
  for (size_t index = 0; index < catalog.count; ++index) {
    if (strcmp(catalog.sensors[index].id, sensorId) == 0)
      return &catalog.sensors[index];
  }
  return nullptr;
}

const TmepValue *tmepFindValue(const TmepSensor &sensor, const char *field) {
  if (strcmp(field, "teplota") == 0) return &sensor.temperature;
  if (strcmp(field, "vlhkost") == 0) return &sensor.humidity;
  if (strcmp(field, "tlak") == 0) return &sensor.pressure;
  if (strcmp(field, "rssi") == 0) return &sensor.rssi;
  if (strcmp(field, "napeti") == 0) return &sensor.voltage;
  return nullptr;
}

bool tmepFieldSupported(const char *field) {
  return field != nullptr &&
         (strcmp(field, "teplota") == 0 || strcmp(field, "vlhkost") == 0 ||
          strcmp(field, "tlak") == 0 || strcmp(field, "rssi") == 0 ||
          strcmp(field, "napeti") == 0);
}
