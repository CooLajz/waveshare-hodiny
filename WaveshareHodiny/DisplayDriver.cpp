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
uint8_t partialRefreshWarmupFrames = 0;
bool partialRefreshWarmupRequested = false;
bool partialRefreshEnableRequested = false;

constexpr size_t FRAMEBUFFER_BYTES = 480 * 480 * sizeof(lv_color_t);
constexpr size_t SCREENSHOT_CHUNK_BYTES = 2048;

void flushDisplay(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels) {
  // V direct mode může LVGL zavolat flush pro několik samostatných
  // invalidovaných oblastí téhož snímku. Fyzický framebuffer přepneme až po
  // vykreslení poslední z nich; jinak by LCD zobrazilo rozpracovaný mezistav a
  // čekání na každý dílčí flush by zbytečně blokovalo hlavní smyčku.
  if (!lv_disp_flush_is_last(driver)) {
    lv_disp_flush_ready(driver);
    return;
  }

  // Oba draw buffery jsou přímo fyzické framebuffery RGB panelu. I při
  // částečném LVGL renderu proto panelu předáváme začátek celého hotového
  // framebufferu; area popisuje pouze oblast, kterou LVGL uvnitř něj změnilo.
  const bool framePresented = LCD_addWindow(
      0, 0, 479, 479, reinterpret_cast<uint8_t *>(&pixels->full));
  if (!framePresented) {
    // Při chybě zachováme LVGL živé; následný resync obnoví RGB DMA bez
    // předstírání, že čekání na bezpečné uvolnění framebufferu uspělo.
    LCD_Resync();
  }

  if (framePresented && partialRefreshWarmupFrames > 0) {
    // Direct mode předpokládá, že oba framebuffery obsahují stejný výchozí
    // snímek. Než jej zapneme, necháme LVGL oba buffery po jednom kompletně
    // vyrenderovat. Vyhneme se tak kopírování do framebufferu, který může panel
    // právě číst, a tedy i jednorázovému roztržení obrazu při přepnutí.
    --partialRefreshWarmupFrames;
    if (partialRefreshWarmupFrames == 0) {
      // Mezi dvěma plnými zahřívacími snímky může přeskočit sekunda nebo se
      // změnit jiný dynamický obsah. Oba buffery by pak před zapnutím LVGL
      // direct mode nebyly totožné a panel by mohl krátce střídat dvě polohy
      // ručičky. Po dokončení fyzického snímku už panel předchozí framebuffer
      // nečte, proto jej bezpečně sjednotíme s právě dokončeným obrazem.
      void *otherBuffer = nullptr;
      if (pixels == frameBuffer1)
        otherBuffer = frameBuffer2;
      else if (pixels == frameBuffer2)
        otherBuffer = frameBuffer1;
      if (otherBuffer == nullptr) {
        partialRefreshWarmupFrames = 2;
        partialRefreshWarmupRequested = true;
      } else {
        memcpy(otherBuffer, pixels, FRAMEBUFFER_BYTES);
        partialRefreshEnableRequested = true;
      }
    } else {
      partialRefreshWarmupRequested = true;
    }
  }
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
  if (partialRefreshEnableRequested) {
    partialRefreshEnableRequested = false;
    displayDriver.full_refresh = 0;
    displayDriver.direct_mode = 1;
  }
  if (partialRefreshWarmupRequested) {
    partialRefreshWarmupRequested = false;
    displayDriverRefresh();
  }
}

void displayDriverRefresh() {
  lv_obj_invalidate(lv_scr_act());
}

void displayDriverSetPartialRefresh(bool enabled, bool rebuildBuffers) {
  if (enabled) {
    if (!rebuildBuffers &&
        (displayDriver.direct_mode || partialRefreshWarmupFrames > 0)) {
      return;
    }
    partialRefreshWarmupFrames = 2;
    partialRefreshWarmupRequested = false;
    partialRefreshEnableRequested = false;
    displayDriver.direct_mode = 0;
    displayDriver.full_refresh = 1;
  } else {
    if (!displayDriver.direct_mode && partialRefreshWarmupFrames == 0) return;
    partialRefreshWarmupFrames = 0;
    partialRefreshWarmupRequested = false;
    partialRefreshEnableRequested = false;
    displayDriver.direct_mode = 0;
    displayDriver.full_refresh = 1;
  }
  displayDriverRefresh();
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
  // USB CDC může při souběhu s animací přijmout jen část 2kB bloku. Dříve se
  // částečný zápis považoval za dokončený přenos, takže na hostiteli chyběl
  // konec framebufferu. Posuneme se pouze o skutečně přijaté bajty a zbytek
  // stejného bloku odešleme v některém z dalších průchodů hlavní smyčkou.
  screenshotOffset +=
      output.write(screenshotBuffer + screenshotOffset, count);
  return screenshotOffset >= FRAMEBUFFER_BYTES;
}
