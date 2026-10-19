#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

class TwoWire {
 public:
  using Key = std::pair<uint8_t, uint16_t>;
  std::map<Key, std::vector<uint8_t>> registers;
  bool failAck = false;
  unsigned acknowledgements = 0;
  void beginTransmission(uint8_t device) { address = device; tx.clear(); }
  size_t write(uint8_t value) { tx.push_back(value); return 1; }
  uint8_t endTransmission(bool = true) {
    if (tx.size() < 2) return 4;
    reg = (uint16_t(tx[0]) << 8) | tx[1];
    if (!registers.count({address, reg})) return 2;
    if (tx.size() == 3) {
      if (failAck) return 4;
      ++acknowledgements;
      registers[{address, reg}] = {tx[2]};
    }
    return 0;
  }
  size_t requestFrom(uint8_t device, uint8_t size) {
    rx = registers[{device, reg}];
    if (rx.size() > size) rx.resize(size);
    position = 0;
    return rx.size();
  }
  int available() { return rx.size() - position; }
  int read() { return position < rx.size() ? rx[position++] : -1; }
 private:
  uint8_t address = 0;
  uint16_t reg = 0;
  size_t position = 0;
  std::vector<uint8_t> tx, rx;
};
extern TwoWire Wire;
