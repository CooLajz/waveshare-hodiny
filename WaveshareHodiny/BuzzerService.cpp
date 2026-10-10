#include "BuzzerService.h"
#include "AlarmBuzzerPattern.h"
#include "NotificationRules.h"
#include "TCA9554PWR.h"
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
TaskHandle_t worker = nullptr;
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;
BuzzerSnapshot state;
bool alarmRequested = false;

void run(void *) {
  TickType_t wait = 1; // Verify silence at startup, retry if the bus fails.
  int64_t started = 0;
  bool active = false;
  bool alarm = false;
  int64_t alarmStart = 0;
  for (;;) {
    uint32_t duration = 0;
    const bool received = xTaskNotifyWait(0, UINT32_MAX, &duration, wait) == pdTRUE;
    if (received && duration == UINT32_MAX) {
      alarm = true; alarmStart = esp_timer_get_time();
    } else if (received) alarm = false;
    if (alarm) {
      const uint32_t elapsed = (esp_timer_get_time() - alarmStart) / 1000;
      duration = alarmBuzzerOn(elapsed) ? 50 : 0;
      if (elapsed >= 600000) { alarm = false; duration = 0; }
    } else if (!received) duration = 0;
    bool ok = Set_EXIO_Checked(EXIO_PIN8, duration ? High : Low);
    // Failed start: attempt to silence the output, then keep retrying OFF.
    if (!ok && duration) Set_EXIO_Checked(EXIO_PIN8, Low);
    const int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&stateMux);
    if (received) state.requestedMs = duration;
    state.ioOk = ok;
    if (ok) {
      if (active && !duration) state.lastPulseMs = (now - started) / 1000;
      active = duration != 0;
      state.active = active;
      if (active) started = now;
    }
    portEXIT_CRITICAL(&stateMux);
    if (alarm) {
      wait = pdMS_TO_TICKS(20) + 1;
    } else if (!ok) {
      wait = pdMS_TO_TICKS(10) + 1;
    } else if (duration) {
      wait = (duration + portTICK_PERIOD_MS - 1) / portTICK_PERIOD_MS;
    } else {
      wait = portMAX_DELAY;
    }
  }
}
}

void buzzerServiceBegin() {
  if (worker) return;
  const bool ok = Set_EXIO_Checked(EXIO_PIN8, Low);
  portENTER_CRITICAL(&stateMux);
  state.ioOk = ok;
  portEXIT_CRITICAL(&stateMux);
  const bool ready = xTaskCreate(run, "notification-beep", 3072, nullptr, 2, &worker) == pdPASS;
  portENTER_CRITICAL(&stateMux);
  state.ready = ready;
  portEXIT_CRITICAL(&stateMux);
}

bool buzzerServicePlay(uint32_t milliseconds) {
  portENTER_CRITICAL(&stateMux);
  const bool owned = alarmRequested;
  portEXIT_CRITICAL(&stateMux);
  if (owned || milliseconds > NOTIFICATION_MAX_BEEP_MS || !worker) return false;
  // A new notification replaces any pending/current pulse; zero cancels it.
  return xTaskNotify(worker, milliseconds, eSetValueWithOverwrite) == pdPASS;
}

BuzzerSnapshot buzzerServiceSnapshot() {
  portENTER_CRITICAL(&stateMux);
  const BuzzerSnapshot result = state;
  portEXIT_CRITICAL(&stateMux);
  return result;
}

void buzzerServiceAlarm(bool enabled) {
  portENTER_CRITICAL(&stateMux);
  alarmRequested = enabled;
  portEXIT_CRITICAL(&stateMux);
  if (worker) xTaskNotify(worker, enabled ? UINT32_MAX : 0, eSetValueWithOverwrite);
}
