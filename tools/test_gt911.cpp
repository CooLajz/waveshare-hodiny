// clang++ -std=c++17 -fsanitize=address,undefined -Itools/touch_test_support tools/test_gt911.cpp -o /tmp/test-gt911
#include "../WaveshareHodiny/Touch_GT911.cpp"
#include <cassert>
#include <cstdio>

TwoWire Wire;
I2CBusGuard::I2CBusGuard() {}
I2CBusGuard::~I2CBusGuard() {}

int main() {
  uint8_t points = 0;
  uint16_t x = 25, y = 50;
  bool fresh = false;
  assert(!GT911_Detect());
  assert(!GT911_ReadSample(points, x, y, fresh));
  Wire.registers[{0x5d, 0x8140}] = {'9', '1', '1', 0};
  assert(GT911_Detect());
  Wire.registers[{0x5d, 0x814e}] = {0};
  assert(GT911_ReadSample(points, x, y, fresh) && !fresh);
  assert(Wire.acknowledgements == 0 && points == 0 && x == 25 && y == 50);
  Wire.registers[{0x5d, 0x814e}] = {0x81};
  Wire.registers[{0x5d, 0x814f}] = {0, 0x2c, 1, 0x90, 1, 20, 0, 0};
  assert(GT911_ReadSample(points, x, y, fresh) && fresh);
  assert(points == 1 && x == 300 && y == 400 && Wire.acknowledgements == 1);
  // Not-ready while down preserves contact, never creates a false release.
  assert(GT911_ReadSample(points, x, y, fresh) && !fresh && points == 1);
  assert(Wire.acknowledgements == 1);
  Wire.registers[{0x5d, 0x814e}] = {0x80};
  assert(GT911_ReadSample(points, x, y, fresh) && fresh);
  assert(points == 0 && x == 300 && y == 400);
  Wire.registers[{0x5d, 0x814e}] = {0x83};
  assert(GT911_ReadSample(points, x, y, fresh) && fresh && points == 3);
  // Malformed counts are cleared so the controller can produce another frame.
  Wire.registers[{0x5d, 0x814e}] = {0x86};
  assert(!GT911_ReadSample(points, x, y, fresh) && !fresh);
  assert(Wire.registers[TwoWire::Key(0x5d, 0x814e)][0] == 0);
  Wire.registers[{0x5d, 0x814e}] = {0x81};
  Wire.registers[{0x5d, 0x814f}] = {0, 1}; // Short I2C read.
  assert(!GT911_ReadSample(points, x, y, fresh) && !fresh);
  assert(Wire.registers[TwoWire::Key(0x5d, 0x814e)][0] == 0);
  Wire.registers[{0x5d, 0x814e}] = {0x81};
  Wire.registers[{0x5d, 0x814f}] = {0, 0xe0, 1, 0, 0, 0, 0, 0}; // x=480.
  assert(!GT911_ReadSample(points, x, y, fresh) && !fresh);
  assert(x == 300 && y == 400);
  Wire.registers[{0x5d, 0x814e}] = {0x80};
  Wire.failAck = true;
  assert(!GT911_ReadSample(points, x, y, fresh) && !fresh);
  Wire.failAck = false;
  Wire.registers.clear();
  Wire.registers[{0x14, 0x8140}] = {'9', '1', '1', 0};
  assert(GT911_Detect());
  Wire.registers[{0x14, 0x814e}] = {0x80};
  assert(GT911_ReadSample(points, x, y, fresh) && fresh && points == 0);
  Wire.registers[{0x14, 0x8140}] = {'?', '?', '?', 0};
  assert(!GT911_Detect());
  puts("GT911 addresses, contact/release, frame ACK, short reads and invalid packets: OK");
}
