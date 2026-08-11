#include "ImprovSerialService.h"

#include "FirmwareBuild.h"

#if FIRMWARE_RELEASE

#include "improv.h"

#include <array>
#include <string>
#include <vector>

#if !ARDUINO_USB_MODE || !ARDUINO_USB_CDC_ON_BOOT
#error "Release Improv requires Hardware CDC with USB CDC On Boot enabled"
#endif

namespace {
constexpr size_t RX_BUFFER_SIZE = 260;
constexpr uint32_t RX_TIMEOUT_MS = 100;

struct ImprovTransport {
  Stream *stream;
  std::array<uint8_t, RX_BUFFER_SIZE> receiveBuffer{};
  size_t receivePosition = 0;
  unsigned long lastReceiveAt = 0;
};

std::array<ImprovTransport, 2> transports{{{&Serial0}, {&HWCDCSerial}}};
improv::State currentState = improv::STATE_AUTHORIZED;
ImprovCredentialsCallback onCredentials = nullptr;
Stream *provisioningStream = nullptr;

void resetReceiveBuffer(ImprovTransport &transport) {
  transport.receivePosition = 0;
  transport.lastReceiveAt = 0;
}

void writeFrame(Stream &stream, improv::ImprovSerialType type,
                const uint8_t *data, size_t length) {
  if (length > 255) return;
  std::vector<uint8_t> frame = {
      'I', 'M', 'P', 'R', 'O', 'V', improv::IMPROV_SERIAL_VERSION,
      static_cast<uint8_t>(type), static_cast<uint8_t>(length)};
  frame.insert(frame.end(), data, data + length);
  uint8_t checksum = 0;
  for (uint8_t byte : frame) checksum += byte;
  frame.push_back(checksum);
  frame.push_back('\n');
  stream.write(frame.data(), frame.size());
}

void broadcastFrame(improv::ImprovSerialType type, const uint8_t *data,
                    size_t length) {
  for (ImprovTransport &transport : transports) {
    writeFrame(*transport.stream, type, data, length);
  }
}

void sendState(improv::State state, Stream *stream = nullptr) {
  currentState = state;
  const uint8_t value = static_cast<uint8_t>(state);
  if (stream != nullptr) {
    writeFrame(*stream, improv::TYPE_CURRENT_STATE, &value, 1);
  } else {
    broadcastFrame(improv::TYPE_CURRENT_STATE, &value, 1);
  }
}

void sendError(improv::Error error, Stream *stream = nullptr) {
  const uint8_t value = static_cast<uint8_t>(error);
  if (stream != nullptr) {
    writeFrame(*stream, improv::TYPE_ERROR_STATE, &value, 1);
  } else {
    broadcastFrame(improv::TYPE_ERROR_STATE, &value, 1);
  }
}

void sendRpcResponse(Stream &stream, improv::Command command,
                     const std::vector<std::string> &values = {}) {
  std::vector<uint8_t> response =
      improv::build_rpc_response(command, values, false);
  response.resize(2 + response[1]);
  writeFrame(stream, improv::TYPE_RPC_RESPONSE, response.data(),
             response.size());
}

bool handleCommand(improv::ImprovCommand command, Stream &stream) {
  switch (command.command) {
    case improv::WIFI_SETTINGS:
      if (command.ssid.empty() || command.ssid.length() > 32 ||
          command.password.length() > 64 || onCredentials == nullptr) {
        sendError(improv::ERROR_INVALID_RPC, &stream);
        return true;
      }
      provisioningStream = &stream;
      sendError(improv::ERROR_NONE, &stream);
      sendState(improv::STATE_PROVISIONING, &stream);
      onCredentials(String(command.ssid.c_str()),
                    String(command.password.c_str()));
      return true;

    case improv::GET_CURRENT_STATE:
      sendState(currentState, &stream);
      if (currentState == improv::STATE_PROVISIONED) {
        sendRpcResponse(stream, improv::GET_CURRENT_STATE,
                        {"http://waveshare-hodiny.local/"});
      }
      return true;

    case improv::GET_DEVICE_INFO:
      sendRpcResponse(stream, improv::GET_DEVICE_INFO,
                      {FIRMWARE_NAME, FIRMWARE_VERSION, FIRMWARE_CHIP_VARIANT,
                       FIRMWARE_DEVICE_NAME});
      return true;

    default:
      sendError(improv::ERROR_UNKNOWN_RPC, &stream);
      return true;
  }
}

void handleByte(ImprovTransport &transport, uint8_t byte) {
  if (transport.receivePosition >= transport.receiveBuffer.size()) {
    resetReceiveBuffer(transport);
  }
  transport.receiveBuffer[transport.receivePosition] = byte;
  const bool frameComplete =
      transport.receivePosition >= 9 &&
      transport.receivePosition == 9 + transport.receiveBuffer[8];
  const bool keepReading = improv::parse_improv_serial_byte(
      transport.receivePosition, byte, transport.receiveBuffer.data(),
      [&transport](improv::ImprovCommand command) {
        return handleCommand(command, *transport.stream);
      },
      [&transport](improv::Error error) {
        sendError(error, transport.stream);
      });
  if (frameComplete || !keepReading) {
    resetReceiveBuffer(transport);
  } else {
    ++transport.receivePosition;
    transport.lastReceiveAt = millis();
  }
}
}  // namespace

void improvSerialServiceInit(ImprovCredentialsCallback credentialsCallback) {
  onCredentials = credentialsCallback;
  provisioningStream = nullptr;
  Serial0.setTxBufferSize(4096);
  Serial0.begin(115200);
  for (ImprovTransport &transport : transports) {
    resetReceiveBuffer(transport);
  }
  sendState(improv::STATE_AUTHORIZED);
}

void improvSerialServiceLoop() {
  for (ImprovTransport &transport : transports) {
    if (transport.lastReceiveAt != 0 &&
        millis() - transport.lastReceiveAt > RX_TIMEOUT_MS) {
      resetReceiveBuffer(transport);
    }
    while (transport.stream->available() > 0) {
      handleByte(transport,
                 static_cast<uint8_t>(transport.stream->read()));
    }
  }
}

void improvSerialServiceSetProvisioned() {
  if (currentState != improv::STATE_PROVISIONED) {
    sendState(improv::STATE_PROVISIONED);
  }
}

void improvSerialServiceProvisioningSucceeded() {
  Stream *stream = provisioningStream;
  sendError(improv::ERROR_NONE, stream);
  sendState(improv::STATE_PROVISIONED, stream);
  if (stream != nullptr) {
    sendRpcResponse(*stream, improv::WIFI_SETTINGS,
                    {"http://waveshare-hodiny.local/"});
  }
  provisioningStream = nullptr;
}

void improvSerialServiceProvisioningFailed() {
  Stream *stream = provisioningStream;
  sendError(improv::ERROR_UNABLE_TO_CONNECT, stream);
  sendState(improv::STATE_AUTHORIZED, stream);
  provisioningStream = nullptr;
}

#else

void improvSerialServiceInit(ImprovCredentialsCallback) {}
void improvSerialServiceLoop() {}
void improvSerialServiceSetProvisioned() {}
void improvSerialServiceProvisioningSucceeded() {}
void improvSerialServiceProvisioningFailed() {}

#endif
