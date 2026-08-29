#include "DisplayDriver.h"

#include <esp_heap_caps.h>
#include <esp_timer.h>

#include <cstring>

#include "Display_ST7701.h"
#include "FirmwareBuild.h"
#include "Touch_CST820.h"

namespace {
lv_disp_draw_buf_t drawBuffer;
lv_disp_drv_t displayDriver;

void *frameBuffer1 = nullptr;
void *frameBuffer2 = nullptr;
uint8_t *screenshotBuffer = nullptr;
size_t screenshotOffset = 0;
bool horizontalSwipePending = false;
bool horizontalSwipeLatched = false;
int8_t verticalSwipePending = 0;
bool verticalSwipeLatched = false;
bool singleClickPending = false;
bool singleClickLatched = false;

constexpr size_t FRAMEBUFFER_BYTES = 480 * 480 * sizeof(lv_color_t);
constexpr size_t SCREENSHOT_CHUNK_BYTES = 2048;

void flushDisplay(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels) {
  LCD_addWindow(area->x1, area->y1, area->x2, area->y2,
                reinterpret_cast<uint8_t *>(&pixels->full));
  lv_disp_flush_ready(driver);
}

void readTouch(lv_indev_drv_t *, lv_indev_data_t *data) {
  Touch_Read_Data();
  const bool horizontalSwipe =
      touch_data.gesture == SWIPE_LEFT || touch_data.gesture == SWIPE_RIGHT;
  const bool verticalSwipe =
      touch_data.gesture == SWIPE_UP || touch_data.gesture == SWIPE_DOWN;
  const bool singleClick = touch_data.gesture == SINGLE_CLICK;
  if (horizontalSwipe) {
    if (!horizontalSwipeLatched) {
      horizontalSwipeLatched = true;
      horizontalSwipePending = true;
    }
    if (lv_indev_get_obj_act() != nullptr)
      lv_indev_wait_release(lv_indev_get_act());
    data->state = LV_INDEV_STATE_REL;
  } else if (verticalSwipe) {
    horizontalSwipeLatched = false;
    if (!verticalSwipeLatched) {
      verticalSwipePending = touch_data.gesture == SWIPE_UP ? -1 : 1;
      verticalSwipeLatched = true;
    }
    if (lv_indev_get_obj_act() != nullptr)
      lv_indev_wait_release(lv_indev_get_act());
    data->state = LV_INDEV_STATE_REL;
  } else {
    horizontalSwipeLatched = false;
    verticalSwipeLatched = false;
    if (singleClick && !singleClickLatched) {
      singleClickPending = true;
      singleClickLatched = true;
    } else if (!singleClick) {
      singleClickLatched = false;
    }
    if (touch_data.points > 0) {
      data->point.x = touch_data.x;
      data->point.y = touch_data.y;
      data->state = LV_INDEV_STATE_PR;
    } else {
      data->state = LV_INDEV_STATE_REL;
    }
  }

  touch_data.points = 0;
  touch_data.gesture = NONE;
}

void increaseTick(void *) {
  lv_tick_inc(2);
}
}  // namespace

void displayDriverInit() {
  lv_init();

  ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(
      panel_handle, 2, &frameBuffer1, &frameBuffer2));
#if !FIRMWARE_RELEASE
  screenshotBuffer = static_cast<uint8_t *>(heap_caps_malloc(
      FRAMEBUFFER_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#endif
  lv_disp_draw_buf_init(&drawBuffer, frameBuffer1, frameBuffer2, 480 * 480);

  lv_disp_drv_init(&displayDriver);
  displayDriver.hor_res = 480;
  displayDriver.ver_res = 480;
  displayDriver.flush_cb = flushDisplay;
  displayDriver.full_refresh = 1;
  displayDriver.draw_buf = &drawBuffer;
  lv_disp_drv_register(&displayDriver);

  static lv_indev_drv_t inputDriver;
  lv_indev_drv_init(&inputDriver);
  inputDriver.type = LV_INDEV_TYPE_POINTER;
  inputDriver.read_cb = readTouch;
  inputDriver.long_press_time = 800;
  lv_indev_drv_register(&inputDriver);

  const esp_timer_create_args_t tickTimerArgs = {
      .callback = increaseTick,
      .arg = nullptr,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "lvgl-tick",
      .skip_unhandled_events = true,
  };
  esp_timer_handle_t tickTimer = nullptr;
  ESP_ERROR_CHECK(esp_timer_create(&tickTimerArgs, &tickTimer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(tickTimer, 2000));
}

void displayDriverLoop() {
  lv_timer_handler();
}

void displayDriverRefresh() {
  lv_obj_invalidate(lv_scr_act());
}

bool displayDriverTakeHorizontalSwipe() {
  if (!horizontalSwipePending) return false;
  horizontalSwipePending = false;
  return true;
}

int8_t displayDriverTakeVerticalSwipe() {
  const int8_t direction = verticalSwipePending;
  verticalSwipePending = 0;
  return direction;
}

bool displayDriverTakeSingleClick() {
  if (!singleClickPending) return false;
  singleClickPending = false;
  return true;
}

bool displayDriverBeginFramebufferCapture(Print &output) {
  if (drawBuffer.buf1 == nullptr || drawBuffer.buf2 == nullptr ||
      screenshotBuffer == nullptr) {
    return false;
  }

  // LVGL kreslí do buf_act; druhý plný framebuffer je právě zobrazený panelem.
  const void *displayedBuffer = drawBuffer.buf_act == drawBuffer.buf1
                                    ? drawBuffer.buf2
                                    : drawBuffer.buf1;
  memcpy(screenshotBuffer, displayedBuffer, FRAMEBUFFER_BYTES);
  screenshotOffset = 0;

  output.println("WSFB1 480 480 RGB565LE 460800");
  return true;
}

bool displayDriverStreamFramebufferChunk(Print &output) {
  if (screenshotBuffer == nullptr || screenshotOffset >= FRAMEBUFFER_BYTES) {
    return true;
  }

  const size_t count =
      min(SCREENSHOT_CHUNK_BYTES, FRAMEBUFFER_BYTES - screenshotOffset);
  if (output.write(screenshotBuffer + screenshotOffset, count) != count) {
    screenshotOffset = FRAMEBUFFER_BYTES;
    return true;
  }
  screenshotOffset += count;
  return screenshotOffset >= FRAMEBUFFER_BYTES;
}
