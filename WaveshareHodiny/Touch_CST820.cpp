#include "Touch_CST820.h"
#include "TouchGestureTracker.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace {
SemaphoreHandle_t sampleMutex() {
  static StaticSemaphore_t storage;
  static SemaphoreHandle_t mutex = xSemaphoreCreateMutexStatic(&storage);
  return mutex;
}
TaskHandle_t sampler = nullptr;
CST820_Touch latest = {};
GESTURE pendingGesture = NONE;
uint32_t pendingAt = 0;
bool sampleValid = false;
bool ignoreUntilRelease = false;
TouchGestureTracker tracker;
TouchDiagnostics diagnostics;
void sampleTouch(void *);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// I2C读写
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool I2C_Read_Touch(uint8_t Driver_addr, uint8_t Reg_addr, uint8_t *Reg_data, uint32_t Length)
{
  I2CBusGuard guard;
  Wire.beginTransmission(Driver_addr);
  Wire.write(Reg_addr);
  if ( Wire.endTransmission(true)){
    printf("The I2C transmission fails. - I2C Read\r\n");
    return -1;
  }
  if (Wire.requestFrom(Driver_addr, Length) != Length) {
    while (Wire.available()) Wire.read();
    return -1;
  }
  for (int i = 0; i < Length; i++) {
    if (!Wire.available()) return -1;
    *Reg_data++ = Wire.read();
  }
  return 0;
}
bool I2C_Write_Touch(uint8_t Driver_addr, uint8_t Reg_addr, const uint8_t *Reg_data, uint32_t Length)
{
  I2CBusGuard guard;
  Wire.beginTransmission(Driver_addr);
  Wire.write(Reg_addr);
  for (int i = 0; i < Length; i++) {
    Wire.write(*Reg_data++);
  }
  if ( Wire.endTransmission(true))
  {
    printf("The I2C transmission fails. - I2C Write\r\n");
    return -1;
  }
  return 0;
}
struct CST820_Touch touch_data = {0};
uint8_t Touch_Init(void) {
  pinMode(CST820_INT_PIN, INPUT_PULLUP);
  CST820_Touch_Reset();
  CST820_AutoSleep(false);
  uint16_t Verification = CST820_Read_cfg();

  if (!sampler && xTaskCreate(sampleTouch, "touch-sampler", 3072, nullptr, 2, &sampler) != pdPASS)
    return false;

  return true;
}
/* Reset controller */
uint8_t CST820_Touch_Reset(void)
{
  Set_EXIO(EXIO_PIN2,Low);
  vTaskDelay(pdMS_TO_TICKS(10));
  Set_EXIO(EXIO_PIN2,High);
  vTaskDelay(pdMS_TO_TICKS(50));
  return true;
}
uint16_t CST820_Read_cfg(void) {

  uint8_t buf[3]={0};
  I2C_Read_Touch(CST820_ADDR, CST820_REG_Version,buf, 1);
  printf("TouchPad_Version:0x%02x\r\n", buf[0]);
  I2C_Read_Touch(CST820_ADDR, CST820_REG_ChipID, buf, 3);
  printf("ChipID:0x%02x   ProjID:0x%02x   FwVersion:0x%02x \r\n",buf[0], buf[1], buf[2]);

  return true;
}
/*!
    @brief  Fall asleep automatically
*/
void CST820_AutoSleep(bool Sleep_State) {
  CST820_Touch_Reset();
  uint8_t Sleep_State_Set = 0;
  if(Sleep_State)
    Sleep_State_Set = 0;
  else
    Sleep_State_Set = 0xFF;
  I2C_Write_Touch(CST820_ADDR, CST820_REG_DisAutoSleep, &Sleep_State_Set, 1);
}

// reads sensor and touches
// updates Touch Points
namespace {
void sampleTouch(void *) {
  for (;;) {
    xSemaphoreTake(sampleMutex(), portMAX_DELAY);
    uint8_t buf[6] = {};
    sampleValid = !I2C_Read_Touch(CST820_ADDR, CST820_REG_GestureID, buf, 6);
    diagnostics.ioOk = sampleValid;
    if (sampleValid) {
      ++diagnostics.samples;
      latest.points = buf[1] ? 1 : 0;
      // Release packets may omit coordinates; keep the last contact position.
      if (latest.points) {
        latest.x = ((buf[2] & 0x0f) << 8) | buf[3];
        latest.y = ((buf[4] & 0x0f) << 8) | buf[5];
      }
      const uint32_t now = millis();
      const uint8_t event = tracker.sample(latest.points, latest.x, latest.y, buf[0], now);
      if (event == SINGLE_CLICK) ++diagnostics.taps;
      if (ignoreUntilRelease) {
        if (!latest.points && !buf[0]) ignoreUntilRelease = false;
      } else if (event) {
        pendingGesture = static_cast<GESTURE>(event);
        pendingAt = now;
      }
    } else {
      // An I2C failure is not a release and must not synthesize a tap.
      ++diagnostics.errors;
      tracker = TouchGestureTracker{};
      ignoreUntilRelease = true;
    }
    xSemaphoreGive(sampleMutex());
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
}

uint8_t Touch_Read_Data(void) {
  xSemaphoreTake(sampleMutex(), portMAX_DELAY);
  touch_data = latest;
  touch_data.gesture = uint32_t(millis() - pendingAt) <= 1500 ? pendingGesture : NONE;
  pendingGesture = NONE;
  const bool valid = sampleValid;
  if (!valid || ignoreUntilRelease) {
    touch_data.points = 0;
    touch_data.gesture = NONE;
  }
  xSemaphoreGive(sampleMutex());
  return valid;
}

void Touch_DiscardPending() {
  xSemaphoreTake(sampleMutex(), portMAX_DELAY);
  pendingGesture = NONE;
  ignoreUntilRelease = latest.points != 0 || !sampleValid;
  xSemaphoreGive(sampleMutex());
}

void example_touchpad_read(void){
  Touch_Read_Data();
  if (touch_data.gesture != NONE ||  touch_data.points != 0x00) {
      // printf("Touch : X=%u Y=%u points=%d\r\n",  touch_data.x , touch_data.y,touch_data.points);
  } else {
      // data->state = LV_INDEV_STATE_REL;
  }
}
void Touch_Loop(void){
  if(Touch_interrupts){
    Touch_interrupts = false;
    example_touchpad_read();
  }
}

/*!
    @brief  handle interrupts
*/
uint8_t Touch_interrupts;
void IRAM_ATTR Touch_CST820_ISR(void) {
  Touch_interrupts = true;
}

/*!
    @brief  get the gesture event name
*/
String Touch_GestureName(void) {
  switch (touch_data.gesture) {
    case NONE:
      return "NONE";
      break;
    case SWIPE_DOWN:
      return "SWIPE DOWN";
      break;
    case SWIPE_UP:
      return "SWIPE UP";
      break;
    case SWIPE_LEFT:
      return "SWIPE LEFT";
      break;
    case SWIPE_RIGHT:
      return "SWIPE RIGHT";
      break;
    case SINGLE_CLICK:
      return "SINGLE CLICK";
      break;
    case DOUBLE_CLICK:
      return "DOUBLE CLICK";
      break;
    case LONG_PRESS:
      return "LONG PRESS";
      break;
    default:
      return "UNKNOWN";
      break;
  }
}

TouchDiagnostics Touch_GetDiagnostics() {
  xSemaphoreTake(sampleMutex(), portMAX_DELAY);
  TouchDiagnostics result = diagnostics;
  result.ready = sampler != nullptr;
  xSemaphoreGive(sampleMutex());
  return result;
}
