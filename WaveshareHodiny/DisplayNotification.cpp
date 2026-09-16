#include "DisplayNotification.h"

#include <Arduino.h>
#include <lvgl.h>
#include "ClockFonts.h"
#include "BuzzerService.h"
#include "DisplayDriver.h"
#include "NotificationRules.h"
#include "NotificationTextLayout.h"

namespace {
lv_obj_t *overlay = nullptr;
lv_obj_t *lineLabels[notification_layout::MAX_LINES] = {};
NotificationLifetime lifetime;
bool awaitingPresentation = false;
bool overlayDrawn = false;
uint32_t pendingSeconds = 0;
uint32_t pendingBeepMs = 0;
bool redNight = false;
uint32_t requestedTextColor = 0xffffff;
uint32_t requestedBackgroundColor = 0;

void applyColors() {
  if (!overlay) return;
  lv_obj_set_style_bg_color(overlay, lv_color_hex(redNight ? 0 : requestedBackgroundColor), 0);
  for (auto *label : lineLabels)
    if (label) lv_obj_set_style_text_color(label, lv_color_hex(redNight ? 0xff4848 : requestedTextColor), 0);
}

void overlayDrawFinished(lv_event_t *) {
  if (awaitingPresentation) overlayDrawn = true;
}

void renderText(const notification_layout::Layout &layout, uint32_t color) {
  char text[NOTIFICATION_MESSAGE_BYTES + 1];
  for (int i = 0; i < notification_layout::MAX_LINES; ++i) {
    if (i >= layout.count) {
      if (lineLabels[i]) lv_obj_add_flag(lineLabels[i], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    if (!lineLabels[i]) {
      lineLabels[i] = lv_label_create(overlay);
      lv_obj_set_style_text_align(lineLabels[i], LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_style_text_letter_space(lineLabels[i], 0, 0);
      lv_obj_set_style_text_line_space(lineLabels[i], 0, 0);
      lv_obj_clear_flag(lineLabels[i], LV_OBJ_FLAG_CLICKABLE);
    }
    const auto &line = layout.lines[i];
    lv_obj_t *label = lineLabels[i];
    lv_obj_set_style_text_font(label, notification_layout::font(line.title), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(label, (notification_layout::SIZE - line.width) / 2, line.y);
    lv_obj_set_size(label, line.width, notification_layout::lineHeight(line.title));
    memcpy(text, line.text, line.length);
    text[line.length] = '\0';
    lv_label_set_text(label, text);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
  }
}
}

bool displayNotificationShow(const char *title, const char *message,
                             uint32_t durationSeconds, uint32_t textColor,
                             uint32_t backgroundColor, uint32_t beepMs) {
  if (!notificationValidText(title, NOTIFICATION_TITLE_BYTES, false) ||
      !notificationValidText(message, NOTIFICATION_MESSAGE_BYTES, true) ||
      durationSeconds > NOTIFICATION_MAX_SECONDS || beepMs > NOTIFICATION_MAX_BEEP_MS) return false;
  const auto layout = notification_layout::fit(title, message);
  if (!layout.count) return false;
  if (!overlay) {
    overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(overlay);
    lv_obj_add_event_cb(overlay, overlayDrawFinished, LV_EVENT_DRAW_POST_END, nullptr);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_size(overlay, 480, 480);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
  }
  requestedTextColor = textColor;
  requestedBackgroundColor = backgroundColor;
  renderText(layout, textColor);
  applyColors();
  // Discard gestures that predate the new notification.
  displayDriverDiscardTouchUntilRelease();
  lv_obj_clear_flag(overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(overlay);
  // Stop the previous pulse immediately; this pulse waits for its image.
  buzzerServicePlay(0);
  pendingSeconds = durationSeconds;
  pendingBeepMs = beepMs;
  awaitingPresentation = true;
  overlayDrawn = false;
  lifetime.show(millis(), 0);
  // Identical replacements also need a fresh frame.
  lv_obj_invalidate(overlay);
  return true;
}

bool displayNotificationActive() { return lifetime.active; }

void displayNotificationDismiss() {
  if (!lifetime.active) return;
  lifetime.active = false;
  awaitingPresentation = false;
  overlayDrawn = false;
  lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
  displayDriverDiscardTouchUntilRelease();
}

void displayNotificationFramePresented(bool success) {
  if (!lifetime.active || !awaitingPresentation || !overlayDrawn) return;
  overlayDrawn = false;
  if (!success) return; // Retry the redraw from the UI loop.
  awaitingPresentation = false;
  lifetime.show(millis(), pendingSeconds);
  if (pendingBeepMs) buzzerServicePlay(pendingBeepMs);
}

void displayNotificationLoop() {
  if (awaitingPresentation) lv_obj_invalidate(overlay);
  if (lifetime.expired(millis())) displayNotificationDismiss();
}

void displayNotificationSetRedNight(bool enabled) {
  if (redNight == enabled) return;
  redNight = enabled;
  applyColors();
}
