#pragma once
#include <Wire.h>

#define I2C_SCL_PIN       7
#define I2C_SDA_PIN       15


void I2C_Init(void);

bool I2C_Read(uint8_t Driver_addr, uint8_t Reg_addr, uint8_t *Reg_data, uint32_t Length);
bool I2C_Write(uint8_t Driver_addr, uint8_t Reg_addr, const uint8_t *Reg_data, uint32_t Length);

// Serialize complete transactions across UI, touch and buzzer tasks.
class I2CBusGuard {
 public:
  I2CBusGuard();
  ~I2CBusGuard();
  I2CBusGuard(const I2CBusGuard &) = delete;
  I2CBusGuard &operator=(const I2CBusGuard &) = delete;
};
