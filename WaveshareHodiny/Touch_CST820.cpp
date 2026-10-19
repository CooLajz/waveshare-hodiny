#include "Touch_CST820.h"
#include "TouchGestureTracker.h"
#include "Touch_GT911.h"
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
TouchController controller = TouchController::Unknown;
uint32_t lastFreshSampleAt = 0;
void sampleTouch(void *);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// I2C读写
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool I2C_Read_Touch(uint8_t Driver_addr, uint8_t Reg_addr, uint8_t *Reg_data, uint32_t Length, bool logError = true)
{
  I2CBusGuard guard;
  Wire.beginTransmission(Driver_addr);
  Wire.write(Reg_addr);
  if ( Wire.endTransmission(true)){
    if (logError) printf("The I2C transmission fails. - I2C Read\r\n");
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
bool Touch_DetectHardware() {
  controller = TouchController::Unknown;
  // Probe CST820 first without driving its INT output. Retry after reset
  // because a sleeping/booting CST820 must not send us into GT911 reset.
  pinMode(CST820_INT_PIN, INPUT_PULLUP);
  if (!Set_EXIO_Checked(EXIO_PIN2, Low)) return false;
  vTaskDelay(pdMS_TO_TICKS(10));
  if (!Set_EXIO_Checked(EXIO_PIN2, High)) return false;
  vTaskDelay(pdMS_TO_TICKS(60));
  for (uint8_t attempt = 0; attempt < 6; ++attempt) {
    uint8_t identity[3] = {};
    if (!I2C_Read_Touch(CST820_ADDR, CST820_REG_ChipID, identity, 3, false)) {
      controller = TouchController::CST820;
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  // Both models share EXIO2 reset and GPIO16 INT. Only after excluding
  // CST820 hold INT low to select GT911's documented 0x5d address.
  pinMode(CST820_INT_PIN, OUTPUT);
  digitalWrite(CST820_INT_PIN, LOW);
  const bool resetLow = Set_EXIO_Checked(EXIO_PIN2, Low);
  vTaskDelay(pdMS_TO_TICKS(10));
  const bool resetHigh = Set_EXIO_Checked(EXIO_PIN2, High);
  vTaskDelay(pdMS_TO_TICKS(200));
  digitalWrite(CST820_INT_PIN, HIGH);
  vTaskDelay(pdMS_TO_TICKS(1));
  pinMode(CST820_INT_PIN, INPUT_PULLUP);
  vTaskDelay(pdMS_TO_TICKS(50));
  if (!resetLow || !resetHigh) return false;
  for (uint8_t attempt = 0; attempt < 3; ++attempt) {
    if (GT911_Detect()) {
      controller = TouchController::GT911;
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  return controller != TouchController::Unknown;
}

TouchController Touch_GetController() { return controller; }
const char *Touch_GetBoardName() {
  switch (controller) {
    case TouchController::CST820: return "ESP32-S3-Touch-LCD-2.1";
    case TouchController::GT911: return "ESP32-S3-Touch-LCD-2.8C";
    default: return "unknown";
  }
}
const char *Touch_GetControllerName() {
  switch (controller) {
    case TouchController::CST820: return "CST820";
    case TouchController::GT911: return "GT911";
    default: return "unknown";
  }
}

uint8_t Touch_Init(void) {
  if (controller == TouchController::Unknown) return false;
  if (controller == TouchController::CST820) {
    pinMode(CST820_INT_PIN, INPUT_PULLUP);
    CST820_AutoSleep(false);
    CST820_Read_cfg();
  }

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
    bool fresh = true;
    const bool gt911 = controller == TouchController::GT911;
    sampleValid = gt911 ? GT911_ReadSample(latest.points, latest.x, latest.y, fresh) :
        !I2C_Read_Touch(CST820_ADDR, CST820_REG_GestureID, buf, 6);
    const uint32_t now = millis();
    if (sampleValid && fresh) lastFreshSampleAt = now;
    if (gt911 && !fresh && latest.points && uint32_t(now - lastFreshSampleAt) > 300) {
      // A lost release packet must not leave LVGL permanently pressed or
      // synthesize a tap. Cancel and wait for an explicit release instead.
      sampleValid = false;
      latest.points = 0;
    }
    diagnostics.ioOk = sampleValid;
    if (sampleValid) {
      ++diagnostics.samples;
      if (!gt911) latest.points = buf[1] ? 1 : 0;
      // Release packets may omit coordinates; keep the last contact position.
      if (!gt911 && latest.points) {
        latest.x = ((buf[2] & 0x0f) << 8) | buf[3];
        latest.y = ((buf[4] & 0x0f) << 8) | buf[5];
      }
      if (gt911 && latest.points > 1) {
        // Never turn adding/removing a second finger into a tap or swipe.
        tracker = TouchGestureTracker{};
        ignoreUntilRelease = true;
        pendingGesture = NONE;
      }
      const uint8_t event = fresh && !ignoreUntilRelease ?
          tracker.sample(latest.points, latest.x, latest.y, buf[0], now, gt911) : 0;
      if (event == SINGLE_CLICK) ++diagnostics.taps;
      if (ignoreUntilRelease) {
        if (fresh && !latest.points && !buf[0]) {
          ignoreUntilRelease = false;
          tracker = TouchGestureTracker{};
        }
      } else if (event) {
        pendingGesture = static_cast<GESTURE>(event);
        pendingAt = now;
      }
    } else {
      // An I2C failure is not a release and must not synthesize a tap.
      ++diagnostics.errors;
      tracker = TouchGestureTracker{};
      ignoreUntilRelease = true;
      pendingGesture = NONE;
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
