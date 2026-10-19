#include "Touch_GT911.h"
#include "I2C_Driver.h"
#include <initializer_list>

namespace {
uint8_t address = 0;
constexpr uint16_t PRODUCT_ID = 0x8140;
constexpr uint16_t TOUCH_STATUS = 0x814e;

bool readRegister(uint8_t device, uint16_t reg, uint8_t *data, uint8_t size) {
  I2CBusGuard guard;
  Wire.beginTransmission(device);
  Wire.write(uint8_t(reg >> 8));
  Wire.write(uint8_t(reg));
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(device, size) != size) {
    while (Wire.available()) Wire.read();
    return false;
  }
  for (uint8_t i = 0; i < size; ++i) data[i] = Wire.read();
  return true;
}

bool acknowledge() {
  I2CBusGuard guard;
  Wire.beginTransmission(address);
  Wire.write(uint8_t(TOUCH_STATUS >> 8));
  Wire.write(uint8_t(TOUCH_STATUS));
  Wire.write(uint8_t(0));
  return Wire.endTransmission(true) == 0;
}
}

bool GT911_Detect() {
  address = 0;
  // INT low at reset selects 0x5d; also accept the alternate GT911 address.
  for (uint8_t candidate : {uint8_t(0x5d), uint8_t(0x14)}) {
    uint8_t product[4] = {};
    if (readRegister(candidate, PRODUCT_ID, product, sizeof(product)) &&
        product[0] == '9' && product[1] == '1' && product[2] == '1' &&
        product[3] == 0) {
      address = candidate;
      return true;
    }
  }
  return false;
}

bool GT911_ReadSample(uint8_t &points, uint16_t &x, uint16_t &y, bool &fresh) {
  fresh = false;
  uint8_t status = 0;
  if (!address || !readRegister(address, TOUCH_STATUS, &status, 1)) return false;
  // Not-ready means no new packet, never a released finger. Do not ACK it.
  if (!(status & 0x80)) return true;
  const uint8_t count = status & 0x0f;
  uint8_t packet[8] = {};
  const bool ok = count <= 5 &&
      (count != 1 || readRegister(address, TOUCH_STATUS + 1, packet, sizeof(packet)));
  if (!acknowledge() || !ok) return false;
  if (count == 1) {
    const uint16_t nextX = uint16_t(packet[1]) | (uint16_t(packet[2]) << 8);
    const uint16_t nextY = uint16_t(packet[3]) | (uint16_t(packet[4]) << 8);
    if (nextX >= 480 || nextY >= 480) return false;
    x = nextX;
    y = nextY;
  }
  points = count;
  fresh = true;
  return true;
}
