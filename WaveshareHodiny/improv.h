#pragma once

// Převzato z oficiálního Improv Wi-Fi C++ SDK:
// https://github.com/improv-wifi/sdk-cpp
// Commit: 17898613a1c17062ca5af295ceb639b16b4930bf
// Licence: Apache-2.0

#include <Arduino.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace improv {

enum Error : uint8_t {
  ERROR_NONE = 0x00,
  ERROR_INVALID_RPC = 0x01,
  ERROR_UNKNOWN_RPC = 0x02,
  ERROR_UNABLE_TO_CONNECT = 0x03,
  ERROR_NOT_AUTHORIZED = 0x04,
  ERROR_BAD_HOSTNAME = 0x05,
  ERROR_UNKNOWN = 0xFF,
};

enum State : uint8_t {
  STATE_STOPPED = 0x00,
  STATE_AWAITING_AUTHORIZATION = 0x01,
  STATE_AUTHORIZED = 0x02,
  STATE_PROVISIONING = 0x03,
  STATE_PROVISIONED = 0x04,
};

enum Command : uint8_t {
  UNKNOWN = 0x00,
  WIFI_SETTINGS = 0x01,
  IDENTIFY = 0x02,
  GET_CURRENT_STATE = 0x02,
  GET_DEVICE_INFO = 0x03,
  GET_WIFI_NETWORKS = 0x04,
  HOSTNAME = 0x05,
  DEVICE_NAME = 0x06,
  GET_NETWORK_STATE = 0x07,
  BAD_CHECKSUM = 0xFF,
};

constexpr uint8_t IMPROV_SERIAL_VERSION = 1;

enum ImprovSerialType : uint8_t {
  TYPE_CURRENT_STATE = 0x01,
  TYPE_ERROR_STATE = 0x02,
  TYPE_RPC = 0x03,
  TYPE_RPC_RESPONSE = 0x04,
};

struct ImprovCommand {
  Command command;
  std::string ssid;
  std::string password;
};

ImprovCommand parse_improv_data(const uint8_t *data, size_t length,
                                bool checkChecksum = true);
bool parse_improv_serial_byte(
    size_t position, uint8_t byte, const uint8_t *buffer,
    std::function<bool(ImprovCommand)> &&callback,
    std::function<void(Error)> &&onError);
std::vector<uint8_t> build_rpc_response(
    Command command, const std::vector<std::string> &data,
    bool addChecksum = true);

}  // namespace improv
