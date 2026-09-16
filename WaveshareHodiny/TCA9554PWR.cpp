#include "TCA9554PWR.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace {
SemaphoreHandle_t outputMutex() {
  static StaticSemaphore_t storage;
  static SemaphoreHandle_t mutex = xSemaphoreCreateMutexStatic(&storage);
  return mutex;
}

bool readOutput(uint8_t &value) {
  I2CBusGuard transaction;
  Wire.beginTransmission(TCA9554_ADDRESS);
  Wire.write(TCA9554_OUTPUT_REG);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(static_cast<uint8_t>(TCA9554_ADDRESS),
                       static_cast<uint8_t>(1)) != 1 || !Wire.available()) return false;
  value = Wire.read();
  return true;
}
}

bool Set_EXIO_Checked(uint8_t pin, uint8_t state) {
  if (pin < 1 || pin > 8 || state > 1) return false;
  if (xSemaphoreTake(outputMutex(), pdMS_TO_TICKS(100)) != pdTRUE) return false;
  uint8_t current = 0;
  bool ok = readOutput(current);
  if (ok) {
    const uint8_t mask = 1U << (pin - 1);
    const uint8_t next = state ? current | mask : current & ~mask;
    if (next != current) ok = I2C_Write_EXIO(TCA9554_OUTPUT_REG, next) == 0;
    uint8_t confirmed = 0;
    ok = ok && readOutput(confirmed) && confirmed == next;
  }
  xSemaphoreGive(outputMutex());
  return ok;
}

/*****************************************************  Operation register REG   ****************************************************/
uint8_t I2C_Read_EXIO(uint8_t REG)                             // Read the value of the TCA9554PWR register REG
{
  I2CBusGuard transaction;
  Wire.beginTransmission(TCA9554_ADDRESS);
  Wire.write(REG);
  uint8_t result = Wire.endTransmission();
  if (result != 0) {
    printf("The I2C transmission fails. - I2C Read EXIO\r\n");
  }
  Wire.requestFrom(TCA9554_ADDRESS, 1);
  uint8_t bitsStatus;
  if (Wire.available()) {
    bitsStatus = Wire.read();
  }
  return bitsStatus;
}
uint8_t I2C_Write_EXIO(uint8_t REG,uint8_t Data)              // Write Data to the REG register of the TCA9554PWR
{
  I2CBusGuard transaction;
  Wire.beginTransmission(TCA9554_ADDRESS);
  Wire.write(REG);
  Wire.write(Data);
  uint8_t result = Wire.endTransmission();
  if (result != 0) {
    printf("The I2C transmission fails. - I2C Write EXIO\r\n");
    return -1;
  }
  return 0;
}
/********************************************************** Set EXIO mode **********************************************************/
void Mode_EXIO(uint8_t Pin,uint8_t State)                 // Set the mode of the TCA9554PWR Pin. The default is Output mode (output mode or input mode). State: 0= Output mode 1= input mode
{
  uint8_t bitsStatus = I2C_Read_EXIO(TCA9554_CONFIG_REG);
  uint8_t Data = (0x01 << (Pin-1)) | bitsStatus;
  uint8_t result = I2C_Write_EXIO(TCA9554_CONFIG_REG,Data);
  if (result != 0) {
    printf("I/O Configuration Failure !!!\r\n");
  }
}
void Mode_EXIOS(uint8_t PinState)                         // Set the mode of the 7 pins from the TCA9554PWR with PinState
{
  uint8_t result = I2C_Write_EXIO(TCA9554_CONFIG_REG,PinState);
  if (result != 0) {
    printf("I/O Configuration Failure !!!\r\n");
  }
}
/********************************************************** Read EXIO status **********************************************************/
uint8_t Read_EXIO(uint8_t Pin)                            // Read the level of the TCA9554PWR Pin
{
  uint8_t inputBits = I2C_Read_EXIO(TCA9554_INPUT_REG);
  uint8_t bitStatus = (inputBits >> (Pin-1)) & 0x01;
  return bitStatus;
}
uint8_t Read_EXIOS(uint8_t REG = TCA9554_INPUT_REG)       // Read the level of all pins of TCA9554PWR, the default read input level state, want to get the current IO output state, pass the parameter TCA9554_OUTPUT_REG, such as Read_EXIOS(TCA9554_OUTPUT_REG);
{
  uint8_t inputBits = I2C_Read_EXIO(REG);
  return inputBits;
}

/********************************************************** Set the EXIO output status **********************************************************/
void Set_EXIO(uint8_t Pin,uint8_t State)                  // Sets the level state of the Pin without affecting the other pins
{
  if (!Set_EXIO_Checked(Pin, State)) printf("Failed to set GPIO!!!\r\n");
}
void Set_EXIOS(uint8_t PinState)                          // Set 7 pins to the PinState state such as :PinState=0x23, 0010 0011 state (the highest bit is not used)
{
  if (xSemaphoreTake(outputMutex(), pdMS_TO_TICKS(100)) != pdTRUE) return;
  uint8_t result = I2C_Write_EXIO(TCA9554_OUTPUT_REG,PinState);
  xSemaphoreGive(outputMutex());
  if (result != 0) {
    printf("Failed to set GPIO!!!\r\n");
  }
}
/********************************************************** Flip EXIO state **********************************************************/
void Set_Toggle(uint8_t Pin)                              // Flip the level of the TCA9554PWR Pin
{
    uint8_t bitsStatus = Read_EXIO(Pin);
    Set_EXIO(Pin,(bool)!bitsStatus);
}
/********************************************************* TCA9554PWR Initializes the device ***********************************************************/
void TCA9554PWR_Init(uint8_t PinState)                  // Set the seven pins to PinState state, for example :PinState=0x23, 0010 0011 State  (Output mode or input mode) 0= Output mode 1= Input mode. The default value is output mode
{
  Mode_EXIOS(PinState);
}
