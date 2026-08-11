#include "improv.h"

#include <cstring>

namespace improv {

ImprovCommand parse_improv_data(const uint8_t *data, size_t length,
                                bool checkChecksum) {
  ImprovCommand result;
  if (length < 2) {
    result.command = UNKNOWN;
    return result;
  }

  const Command command = static_cast<Command>(data[0]);
  const uint8_t dataLength = data[1];
  if (dataLength != length - 2 - checkChecksum) {
    result.command = UNKNOWN;
    return result;
  }

  if (checkChecksum) {
    uint32_t calculatedChecksum = 0;
    for (size_t index = 0; index < length - 1; ++index) {
      calculatedChecksum += data[index];
    }
    if (static_cast<uint8_t>(calculatedChecksum) != data[length - 1]) {
      result.command = BAD_CHECKSUM;
      return result;
    }
  }

  if (command == WIFI_SETTINGS) {
    if (length < 4) {
      result.command = UNKNOWN;
      return result;
    }
    const uint8_t ssidLength = data[2];
    const size_t ssidStart = 3;
    const size_t ssidEnd = ssidStart + ssidLength;
    if (ssidEnd >= length) {
      result.command = UNKNOWN;
      return result;
    }
    const uint8_t passwordLength = data[ssidEnd];
    const size_t passwordStart = ssidEnd + 1;
    const size_t passwordEnd = passwordStart + passwordLength;
    const size_t payloadEnd = length - (checkChecksum ? 1 : 0);
    if (passwordEnd != payloadEnd) {
      result.command = UNKNOWN;
      return result;
    }
    return {
        .command = command,
        .ssid = std::string(data + ssidStart, data + ssidEnd),
        .password = std::string(data + passwordStart, data + passwordEnd),
    };
  }

  result.command = command;
  return result;
}

bool parse_improv_serial_byte(
    size_t position, uint8_t byte, const uint8_t *buffer,
    std::function<bool(ImprovCommand)> &&callback,
    std::function<void(Error)> &&onError) {
  static constexpr char HEADER[] = "IMPROV";
  if (position < 6) return byte == static_cast<uint8_t>(HEADER[position]);
  if (position == 6) return byte == IMPROV_SERIAL_VERSION;
  if (position <= 8) return true;

  const uint8_t type = buffer[7];
  const uint8_t dataLength = buffer[8];
  if (position <= 8 + dataLength) return true;

  if (position == 9 + dataLength) {
    uint8_t checksum = 0;
    for (size_t index = 0; index < position; ++index) checksum += buffer[index];
    if (checksum != byte) {
      onError(ERROR_INVALID_RPC);
      return false;
    }
    if (type == TYPE_RPC) {
      return callback(parse_improv_data(&buffer[9], dataLength, false));
    }
  }
  return false;
}

std::vector<uint8_t> build_rpc_response(
    Command command, const std::vector<std::string> &data, bool addChecksum) {
  size_t frameLength = 3 + data.size();
  for (const auto &item : data) frameLength += item.length();

  std::vector<uint8_t> output(frameLength, 0);
  output[0] = command;
  size_t position = 2;
  for (const auto &item : data) {
    output[position++] = static_cast<uint8_t>(item.length());
    std::memcpy(output.data() + position, item.c_str(), item.length());
    position += item.length();
  }
  output[1] = static_cast<uint8_t>(position - 2);

  if (addChecksum) {
    uint32_t checksum = 0;
    for (uint8_t byte : output) checksum += byte;
    output[frameLength - 1] = static_cast<uint8_t>(checksum);
  }
  return output;
}

}  // namespace improv
