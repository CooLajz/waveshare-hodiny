#include "ForecastDial.h"
#include "ForecastFanRegion.h"
#include "HourlyForecastService.h"
#include "ClockTimezone.h"
#include "ClockFonts.h"
#include "OpenWeatherIcons.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace {
bool red = false;
lv_obj_t *dial = nullptr;
lv_obj_t *temperatureLabel = nullptr, *temperatureShadow = nullptr, *currentIcon = nullptr, *currentAnimation = nullptr;
lv_obj_t *temperatureUnit = nullptr, *temperatureUnitShadow = nullptr;
lv_obj_t *centerTime = nullptr, *centerTimeShadow = nullptr;
bool animationReady = false, animationShown = false;
lv_color_t *fanPixels = nullptr;
lv_img_dsc_t fanImage{};
ForecastFanKey renderedFanKey{};
bool fanInitialized = false;
ForecastFanBuild fanBuild;
float currentTemperature = NAN;
constexpr int CENTER_RADIUS = 108;
constexpr int OUTER_RING_RADIUS = 218;
constexpr int OUTER_RING_WIDTH = 2;
void applyCenterColors() {
  if (!temperatureLabel) return;
  const lv_color_t color = lv_color_hex(red ? 0xEE4232 : 0xF6F6F6);
  lv_obj_set_style_text_color(temperatureLabel, color, 0);
  if (temperatureUnit) lv_obj_set_style_text_color(temperatureUnit, color, 0);
  if (centerTime) lv_obj_set_style_text_color(centerTime, color, 0);
  for (lv_obj_t *icon : {currentIcon, currentAnimation}) {
    lv_obj_set_style_img_recolor(icon, color, 0);
    lv_obj_set_style_img_recolor_opa(icon, red ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  }
}
void setCenterTemperatureText(const char *value) {
  lv_label_set_text(temperatureLabel, value);
  lv_label_set_text(temperatureShadow, value);
  lv_point_t numberSize, unitSize;
  lv_txt_get_size(&numberSize, value, &lv_font_montserrat_48, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  lv_txt_get_size(&unitSize, "°C", &lv_font_montserrat_24, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  const int x = 240 - (numberSize.x + 4 + unitSize.x) / 2;
  const int y = 252;
  lv_obj_set_pos(temperatureShadow, x + 2, y + 2);
  lv_obj_set_pos(temperatureLabel, x, y);
  lv_obj_set_pos(temperatureUnitShadow, x + numberSize.x + 6, y + 7);
  lv_obj_set_pos(temperatureUnit, x + numberSize.x + 4, y + 5);
}
HourlyForecast data;
time_t displayedMinute = -1;
time_t cachedFanMinute = -1;
uint32_t lastCheck = 0;
constexpr float DIAL_PI = 3.14159265359f;

lv_point_t point(lv_point_t center, float degrees, float radius) {
  const float angle = (degrees - 90) * DIAL_PI / 180;
  return {static_cast<lv_coord_t>(center.x + std::lround(std::cos(angle) * radius)),
          static_cast<lv_coord_t>(center.y + std::lround(std::sin(angle) * radius))};
}
lv_color_t temperatureColor(float temperature) {
  if (red) return lv_color_hex(0xC83020);
  // Fixed scale: identical temperatures retain identical colors across days.
  const float stops[] = {-10, 0, 10, 20, 30, 40};
  const uint32_t colors[] = {0x6868E8, 0x3294F0, 0x33C7CE, 0x81CB68, 0xFFB843, 0xEF5543};
  for (int i = 1; i < 6; ++i) {
    if (temperature <= stops[i]) {
      const float fraction = fmaxf(0, fminf(1, (temperature - stops[i-1]) / 10));
      return lv_color_mix(lv_color_hex(colors[i]), lv_color_hex(colors[i-1]),
                          static_cast<uint8_t>(fraction * 255));
    }
  }
  return lv_color_hex(colors[5]);
}
bool rebuildFan(int rowBudget) {
  if (!fanPixels) return true;
  uint32_t colors[12];
  const time_t now = time(nullptr);
  struct tm local{};
  const bool valid = now >= CLOCK_TIMEZONE_START && clockLocaltime(&now, &local);
  const time_t first = now - local.tm_min * 60 - local.tm_sec + 3600;
  for (int i = 0; i < 12; ++i) {
    const ForecastHour *value = valid ? forecastHourAt(data, first + i * 3600) : nullptr;
    colors[(local.tm_hour + 1 + i) % 12] = lv_color_to32(value ? temperatureColor(value->temperature) : lv_color_hex(0x30343A));
  }
  const bool stale = !data.fetchedAt || now - data.fetchedAt > 2 * 3600;
  ForecastFanKey nextKey;
  memcpy(nextKey.colors, colors, sizeof(colors));
  nextKey.hour = local.tm_hour % 12;
  nextKey.minute = local.tm_min;
  nextKey.valid = valid;
  nextKey.stale = stale;
  // A restarted, partially painted image requires a full rebuild, not a seam update.
  fanBuild.begin(renderedFanKey, nextKey, fanInitialized);
  const ForecastFanRegion &region = fanBuild.region;
  const ForecastFanSeamUpdate seam(renderedFanKey, nextKey, fanBuild.allowSeam);
  // Only 16 dither variants are needed for the endpoint replacing the seam.
  lv_color_t seamColors[4][4];
  if (seam.active) {
    const uint32_t value = colors[(nextKey.hour +
        (nextKey.minute < renderedFanKey.minute ? 1 : 0)) % 12];
    for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) {
      seamColors[y][x] = lv_color_hex(forecastFanEndpointColor(value, stale, x, y));
    }
  }
  const int endRow = fanBuild.endRow(rowBudget);
  for (int y = fanBuild.row; y < endRow; ++y) {
    for (int x = region.x1; x <= region.x2; ++x) {
      if (seam.active) {
        if (seam.changes(x, y)) fanPixels[y * 480 + x] = seamColors[y & 3][x & 3];
        continue;
      }
      const float dx = x - 240, dy = y - 240;
      lv_color_t color = lv_color_black();
      if (dx * dx + dy * dy <= 240 * 240) {
        float angle = std::atan2(dy, dx) * 180 / DIAL_PI + 90;
        if (angle < 0) angle += 360;
        const float position = angle / 30;
        const int sector = static_cast<int>(position) % 12;
        float fraction = position - std::floor(position);
        // This interval contains the present-time seam: the last forecast
        // hour ends here and the next whole hour starts immediately after it.
        // Never interpolate tomorrow's last sample back into the first one.
        if (valid && sector == local.tm_hour % 12)
          fraction = fraction >= local.tm_min / 60.0f ? 1.0f : 0.0f;
        const uint32_t left = colors[sector], right = colors[(sector + 1) % 12];
        // Ordered dithering preserves a smooth angular blend on RGB565.
        static const uint8_t bayer[4][4] = {{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
        const float threshold = (bayer[y & 3][x & 3] + 0.5f) / 16;
        const auto channel = [&](int shift, int levels) -> uint8_t {
          const float a = (left >> shift) & 255, b = (right >> shift) & 255;
          const float value = (a + (b - a) * fraction) * (stale ? 0.375f : 0.75f);
          const int quantized = static_cast<int>(value * levels / 255 + threshold);
          return static_cast<uint8_t>((quantized * 255 + levels - 1) / levels);
        };
        color = lv_color_make(channel(16, 31), channel(8, 63), channel(0, 31));
      }
      fanPixels[y * 480 + x] = color;
    }
  }
  if (!fanBuild.advance(endRow)) return false;
  renderedFanKey = nextKey;
  fanInitialized = true;
  return true;
}
void text(lv_draw_ctx_t *ctx, lv_point_t p, const char *value,
          const lv_font_t *font, lv_color_t color, int width = 64) {
  lv_draw_label_dsc_t dsc;
  lv_draw_label_dsc_init(&dsc);
  dsc.font = font;
  dsc.color = color;
  dsc.align = LV_TEXT_ALIGN_CENTER;
  lv_area_t area = {static_cast<lv_coord_t>(p.x - width / 2),
                   static_cast<lv_coord_t>(p.y - font->line_height / 2),
                   static_cast<lv_coord_t>(p.x + width / 2 - 1),
                   static_cast<lv_coord_t>(p.y + font->line_height / 2)};
  lv_draw_label(ctx, &dsc, &area, value, nullptr);
}
void draw(lv_event_t *event) {
  lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(event);
  lv_area_t area;
  lv_obj_get_coords(dial, &area);
  const lv_point_t center = {static_cast<lv_coord_t>(area.x1 + 240),
                             static_cast<lv_coord_t>(area.y1 + 240)};
  const time_t now = time(nullptr);
  struct tm local{};
  const bool timeValid = now >= CLOCK_TIMEZONE_START && clockLocaltime(&now, &local);
  // Show the next twelve whole hours; current conditions live in the center.
  const time_t firstHour = now - local.tm_min * 60 - local.tm_sec + 3600;
  const lv_color_t fg = lv_color_hex(red ? 0xEE4232 : 0xF6F6F6);
  const lv_color_t muted = lv_color_hex(red ? 0x802018 : 0x9099A2);
  if (fanPixels) {
    lv_draw_img_dsc_t background;
    lv_draw_img_dsc_init(&background);
    lv_draw_img(ctx, &background, &area, &fanImage);
  }
  lv_draw_rect_dsc_t centerDisc;
  lv_draw_rect_dsc_init(&centerDisc);
  centerDisc.radius = LV_RADIUS_CIRCLE;
  centerDisc.bg_opa = LV_OPA_COVER;
  centerDisc.bg_color = std::isfinite(currentTemperature) ? lv_color_mix(temperatureColor(currentTemperature), lv_color_black(), 166) : lv_color_hex(0x30343A);
  centerDisc.border_color = lv_color_black();
  centerDisc.border_opa = LV_OPA_50;
  centerDisc.border_width = 2;
  lv_area_t disc = {static_cast<lv_coord_t>(center.x - CENTER_RADIUS), static_cast<lv_coord_t>(center.y - CENTER_RADIUS),
                   static_cast<lv_coord_t>(center.x + CENTER_RADIUS), static_cast<lv_coord_t>(center.y + CENTER_RADIUS)};
  lv_draw_rect(ctx, &centerDisc, &disc);
  if (timeValid) {
    const float angle = (local.tm_hour % 12 + local.tm_min / 60.0f) * 30;
    // Stop at the ring's inner edge; its foreground stroke covers the join.
    const lv_point_t edge = point(center, angle, OUTER_RING_RADIUS - OUTER_RING_WIDTH);
    const lv_point_t start = point(center, angle, CENTER_RADIUS + 2);
    lv_draw_line_dsc_t divider;
    lv_draw_line_dsc_init(&divider);
    divider.color = fg;
    divider.opa = LV_OPA_70;
    divider.width = 2;
    divider.round_start = true;
    divider.round_end = false;
    lv_draw_line(ctx, &divider, &start, &edge);
  }
  lv_draw_arc_dsc_t ring;
  lv_draw_arc_dsc_init(&ring);
  ring.width = OUTER_RING_WIDTH;
  ring.color = lv_color_black();
  const lv_point_t ringShadowCenter = {static_cast<lv_coord_t>(center.x + 2), static_cast<lv_coord_t>(center.y + 2)};
  lv_draw_arc(ctx, &ring, &ringShadowCenter, OUTER_RING_RADIUS, 0, 360);
  ring.color = fg;
  lv_draw_arc(ctx, &ring, &center, OUTER_RING_RADIUS, 0, 360);
  for (int i = 0; i < 12; ++i) {
    const time_t stamp = firstHour + i * 3600;
    struct tm hour{};
    if (timeValid) clockLocaltime(&stamp, &hour);
    else hour.tm_hour = i;
    const float angle = (hour.tm_hour % 12) * 30;
    const ForecastHour *value = timeValid ? forecastHourAt(data, stamp) : nullptr;
    char label[24];
    if (value) snprintf(label, sizeof(label), "%.0f°", std::fabs(value->temperature) < 0.5f ? 0.0 : value->temperature);
    else snprintf(label, sizeof(label), "--");
    const lv_point_t hourPoint = point(center, angle, 216);
    lv_area_t hourBounds = {static_cast<lv_coord_t>(hourPoint.x - 22),
                           static_cast<lv_coord_t>(hourPoint.y - 22),
                           static_cast<lv_coord_t>(hourPoint.x + 22),
                           static_cast<lv_coord_t>(hourPoint.y + 22)};
    lv_draw_rect_dsc_t hourCircle;
    lv_draw_rect_dsc_init(&hourCircle);
    hourCircle.radius = LV_RADIUS_CIRCLE;
    hourCircle.bg_color = lv_color_black();hourCircle.bg_opa = LV_OPA_COVER;
    hourCircle.border_width = 2;
    hourCircle.border_color = lv_color_black();
    lv_area_t circleShadow = hourBounds;
    circleShadow.x1 += 2; circleShadow.x2 += 2;
    circleShadow.y1 += 2; circleShadow.y2 += 2;
    lv_draw_rect(ctx, &hourCircle, &circleShadow);
    hourCircle.border_color = value ? fg : muted;
    hourCircle.bg_color = value ? lv_color_mix(temperatureColor(value->temperature), lv_color_black(), 128) : lv_color_hex(0x30343A);
    lv_draw_rect(ctx, &hourCircle, &hourBounds);
    const lv_point_t temperatureShadowPoint = {static_cast<lv_coord_t>(hourPoint.x + 2), static_cast<lv_coord_t>(hourPoint.y + 2)};
    text(ctx, temperatureShadowPoint, label, &clock_czech_20, lv_color_black(), 44);
    text(ctx, hourPoint, label, &clock_czech_20, fg, 44);
    if (value) {
      const lv_img_dsc_t *icon = openWeatherIconForCode(value->weatherCode, value->isDay);
      if (icon) {
        const lv_point_t p = point(center, angle, 161);
        lv_draw_img_dsc_t img;
        lv_draw_img_dsc_init(&img);
        img.zoom = 256 * 64 / icon->header.w;
        img.pivot = {static_cast<lv_coord_t>(icon->header.w / 2),
                     static_cast<lv_coord_t>(icon->header.h / 2)};
        if (red) { img.recolor = fg; img.recolor_opa = LV_OPA_COVER; }
        lv_area_t bounds = {static_cast<lv_coord_t>(p.x - icon->header.w / 2),
                            static_cast<lv_coord_t>(p.y - icon->header.h / 2),
                            static_cast<lv_coord_t>(p.x + icon->header.w / 2 - 1),
                            static_cast<lv_coord_t>(p.y + icon->header.h / 2 - 1)};
        lv_draw_img_dsc_t shadow = img;
        shadow.recolor = lv_color_black();
        shadow.recolor_opa = LV_OPA_COVER;
        lv_area_t shadowBounds = bounds;
        shadowBounds.x1 += 2; shadowBounds.x2 += 2;
        shadowBounds.y1 += 2; shadowBounds.y2 += 2;
        lv_draw_img(ctx, &shadow, &shadowBounds, icon);
        lv_draw_img(ctx, &img, &bounds, icon);
      }
    }
  }

  // Reuse the decoded GIF frame so its shadow stays exactly synchronized.
  lv_obj_t *iconObject = animationShown ? currentAnimation : currentIcon;
  if (iconObject && !lv_obj_has_flag(iconObject, LV_OBJ_FLAG_HIDDEN)) {
    const void *source = lv_img_get_src(iconObject);
    if (source) {
      lv_area_t bounds;
      lv_obj_get_coords(iconObject, &bounds);
      bounds.x1 += 2; bounds.x2 += 2;
      bounds.y1 += 2; bounds.y2 += 2;
      lv_draw_img_dsc_t shadow;
      lv_draw_img_dsc_init(&shadow);
      shadow.recolor = lv_color_black();
      shadow.recolor_opa = LV_OPA_COVER;
      lv_draw_img(ctx, &shadow, &bounds, source);
    }
  }

}
}

lv_obj_t *forecastDialCreate(lv_obj_t *parent) {
  dial = lv_obj_create(parent);
  lv_obj_remove_style_all(dial);
  lv_obj_set_size(dial, 480, 480);
  lv_obj_set_pos(dial, 0, 0);
  lv_obj_set_style_bg_color(dial, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(dial, LV_OPA_COVER, 0);
  lv_obj_clear_flag(dial, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(dial, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(dial, draw, LV_EVENT_DRAW_MAIN, nullptr);
  // Cache the smooth fan in PSRAM through the project's LVGL allocator.
  fanPixels = static_cast<lv_color_t *>(lv_mem_alloc(480 * 480 * sizeof(lv_color_t)));
  if (fanPixels) {
    fanImage.header.always_zero = 0;
    fanImage.header.w = 480; fanImage.header.h = 480;
    fanImage.header.cf = LV_IMG_CF_TRUE_COLOR;
    fanImage.data_size = 480 * 480 * sizeof(lv_color_t);
    fanImage.data = reinterpret_cast<const uint8_t *>(fanPixels);
    memset(fanPixels, 0, fanImage.data_size);
  }
  currentIcon = lv_img_create(dial);
  lv_obj_align(currentIcon, LV_ALIGN_CENTER, 0, -30);
  currentAnimation = lv_gif_create(dial);
  lv_obj_align(currentAnimation, LV_ALIGN_CENTER, 0, -30);
  lv_obj_add_flag(currentAnimation, LV_OBJ_FLAG_HIDDEN);
  temperatureShadow = lv_label_create(dial);
  lv_obj_set_style_text_font(temperatureShadow, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(temperatureShadow, lv_color_black(), 0);
  temperatureUnitShadow = lv_label_create(dial);
  lv_obj_set_style_text_font(temperatureUnitShadow, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(temperatureUnitShadow, lv_color_black(), 0);
  lv_label_set_text(temperatureUnitShadow, "°C");
  temperatureLabel = lv_label_create(dial);
  lv_obj_set_style_text_font(temperatureLabel, &lv_font_montserrat_48, 0);
  temperatureUnit = lv_label_create(dial);
  lv_obj_set_style_text_font(temperatureUnit, &lv_font_montserrat_24, 0);
  lv_label_set_text(temperatureUnit, "°C");
  centerTimeShadow = lv_label_create(dial);
  lv_obj_set_style_text_font(centerTimeShadow, &clock_czech_20, 0);
  lv_obj_set_style_text_color(centerTimeShadow, lv_color_black(), 0);
  lv_label_set_text(centerTimeShadow, "--:--");
  lv_obj_align(centerTimeShadow, LV_ALIGN_CENTER, 1, 79);
  centerTime = lv_label_create(dial);
  lv_obj_set_style_text_font(centerTime, &clock_czech_20, 0);
  lv_label_set_text(centerTime, "--:--");
  lv_obj_align(centerTime, LV_ALIGN_CENTER, 0, 78);
  setCenterTemperatureText("--");
  applyCenterColors();
  return dial;
}
void forecastDialSetVisible(bool visible) {
  if (!dial) return;
  if (visible) { lv_obj_clear_flag(dial, LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(dial); }
  else lv_obj_add_flag(dial, LV_OBJ_FLAG_HIDDEN);
  if (animationReady) {
    if (visible && animationShown) lv_timer_resume(reinterpret_cast<lv_gif_t *>(currentAnimation)->timer);
    else lv_timer_pause(reinterpret_cast<lv_gif_t *>(currentAnimation)->timer);
  }
  displayedMinute = -1;
}
void forecastDialUpdate(bool redNight) {
  if (!dial) return;
  const bool visible = !lv_obj_has_flag(dial, LV_OBJ_FLAG_HIDDEN);
  const uint32_t now = lv_tick_get();
  if (!fanBuild.pending && now - lastCheck < 1000 && displayedMinute >= 0 && red == redNight) return;
  lastCheck = now;
  const HourlyForecast next = hourlyForecastSnapshot();
  const time_t minute = time(nullptr) / 60;
  if (fanBuild.pending || minute != displayedMinute || next.fetchedAt != data.fetchedAt || next.count != data.count || red != redNight) {
    const bool fanChanged = fanBuild.pending || minute != cachedFanMinute || next.fetchedAt != data.fetchedAt || next.count != data.count || red != redNight;
    data = next; displayedMinute = minute; red = redNight;
    applyCenterColors();
    const time_t stamp = time(nullptr);
    struct tm local{};
    char timeText[8] = "--:--";
    if (stamp >= CLOCK_TIMEZONE_START && clockLocaltime(&stamp, &local))
      snprintf(timeText, sizeof(timeText), "%02d:%02d", local.tm_hour, local.tm_min);
    lv_label_set_text(centerTime, timeText);
    lv_label_set_text(centerTimeShadow, timeText);
    lv_obj_align(centerTime, LV_ALIGN_CENTER, 0, 78);
    lv_obj_align(centerTimeShadow, LV_ALIGN_CENTER, 1, 79);
    // Hidden preparation shares the UI task and paints only eight rows per
    // iteration. No extra framebuffer, worker task, or concurrent LVGL access.
    if (fanChanged && rebuildFan(visible ? 480 : 8)) cachedFanMinute = minute;
    if (visible) lv_obj_invalidate(dial);
  }
}

void forecastDialSetWeather(int code, bool isDay, float temperature, bool animate) {
  if (!dial) return;
  if ((std::isfinite(temperature) != std::isfinite(currentTemperature)) ||
      (std::isfinite(temperature) && temperature != currentTemperature)) {
    currentTemperature = temperature;
    lv_obj_invalidate(dial);
  }
  const lv_img_dsc_t *icon = openWeatherIconForCode(code, isDay);
  if (icon) { lv_img_set_src(currentIcon, icon); lv_obj_align(currentIcon, LV_ALIGN_CENTER, 0, -30); }
  animationShown = animate && animationReady && icon;
  if (!icon || animationShown) lv_obj_add_flag(currentIcon, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_clear_flag(currentIcon, LV_OBJ_FLAG_HIDDEN);
  if (animationShown) lv_obj_clear_flag(currentAnimation, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(currentAnimation, LV_OBJ_FLAG_HIDDEN);
  if (animationReady) {
    if (animationShown && !lv_obj_has_flag(dial, LV_OBJ_FLAG_HIDDEN)) lv_timer_resume(reinterpret_cast<lv_gif_t *>(currentAnimation)->timer);
    else lv_timer_pause(reinterpret_cast<lv_gif_t *>(currentAnimation)->timer);
  }
  char value[24];
  if (std::isfinite(temperature)) snprintf(value, sizeof(value), "%.1f", std::fabs(temperature) < 0.05f ? 0.0 : temperature);
  else snprintf(value, sizeof(value), "--");
  setCenterTemperatureText(value);
}
void forecastDialSetAnimation(const lv_img_dsc_t *source) {
  if (!currentAnimation) return;
  lv_gif_set_src(currentAnimation, source);
  animationReady = true;
  lv_obj_align(currentAnimation, LV_ALIGN_CENTER, 0, -30);
  lv_timer_pause(reinterpret_cast<lv_gif_t *>(currentAnimation)->timer);
}
