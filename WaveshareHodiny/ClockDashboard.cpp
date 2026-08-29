#include "ClockDashboard.h"
#include "OpenWeatherIcons.h"
#include "WeatherIconMapping.h"

#include <lvgl.h>

#include <cmath>
#include <cstring>

#include "ClockFonts.h"
#include "FirmwareUpdateService.h"

namespace {
const lv_color_t COLOR_BACKGROUND = LV_COLOR_MAKE(0, 0, 0);
const lv_color_t COLOR_TEXT = LV_COLOR_MAKE(246, 246, 246);
const lv_color_t COLOR_MUTED = LV_COLOR_MAKE(181, 181, 181);
const lv_color_t COLOR_DIVIDER = LV_COLOR_MAKE(47, 47, 47);
const lv_color_t COLOR_OUTSIDE = LV_COLOR_MAKE(76, 203, 236);
const lv_color_t COLOR_ROOM = LV_COLOR_MAKE(255, 184, 67);
const lv_color_t COLOR_AIR = LV_COLOR_MAKE(101, 199, 68);
const lv_color_t COLOR_HUMIDITY = LV_COLOR_MAKE(63, 151, 219);
const lv_color_t COLOR_ERROR = LV_COLOR_MAKE(255, 72, 72);
constexpr int SECOND_DOT_COUNT = 60;
constexpr float SECOND_RING_RADIUS = 226.0f;
constexpr float PI_VALUE = 3.14159265358979323846f;
constexpr unsigned long SECOND_DOT_FADE_TOTAL_MS = 2000;
constexpr unsigned long SECOND_LINE_FADE_TOTAL_MS = 4000;
constexpr unsigned long SECOND_FADE_DOT_MS = 200;
constexpr unsigned long SECOND_FADE_START_SPAN_MS =
    SECOND_DOT_FADE_TOTAL_MS - SECOND_FADE_DOT_MS;
// Plynulé efekty držíme pod fyzickou obnovovací frekvencí panelu, aby se do
// jednoho snímku zbytečně neposílalo více různých stavů.
constexpr unsigned long SMOOTH_EFFECT_FRAME_MS = 40;
constexpr float SECOND_COMET_TRAIL_SECONDS = 12.0f;
constexpr unsigned long WEATHER_ANIMATION_REVEAL_DELAY_MS = 100;

lv_obj_t *timeLabel = nullptr;
lv_obj_t *dateLabel = nullptr;
lv_obj_t *outsideTitleLabel = nullptr;
lv_obj_t *outsideIntegerLabel = nullptr;
lv_obj_t *outsideDecimalLabel = nullptr;
lv_obj_t *outsideUnitLabel = nullptr;
lv_obj_t *roomTitleLabel = nullptr;
lv_obj_t *roomIntegerLabel = nullptr;
lv_obj_t *roomDecimalLabel = nullptr;
lv_obj_t *roomUnitLabel = nullptr;
lv_obj_t *outsideIconLabel = nullptr;
lv_obj_t *roomIconLabel = nullptr;
lv_obj_t *co2TitleLabel = nullptr;
lv_obj_t *co2ValueLabel = nullptr;
lv_obj_t *co2UnitLabel = nullptr;
lv_obj_t *humidityTitleLabel = nullptr;
lv_obj_t *humidityValueLabel = nullptr;
lv_obj_t *humidityUnitLabel = nullptr;
lv_obj_t *weatherImage = nullptr;
lv_obj_t *roomWeatherImage = nullptr;
lv_obj_t *weatherAnimation = nullptr;
lv_obj_t *roomWeatherAnimation = nullptr;
lv_obj_t *wifiStatusLabel = nullptr;
lv_obj_t *statusLabel = nullptr;
lv_obj_t *webStatusLabel = nullptr;
lv_obj_t *dashboardContent = nullptr;
lv_obj_t *radarPage = nullptr;
lv_obj_t *radarCanvas = nullptr;
lv_obj_t *radarTitleLabel = nullptr;
lv_obj_t *radarProgressBar = nullptr;
bool radarFullPreparationInProgress = false;
lv_obj_t *radarStatusLabel = nullptr;
lv_obj_t *settingsPage = nullptr;
lv_obj_t *dayBrightnessSlider = nullptr;
lv_obj_t *nightBrightnessSlider = nullptr;
lv_obj_t *dayBrightnessValueLabel = nullptr;
lv_obj_t *nightBrightnessValueLabel = nullptr;
lv_obj_t *automaticDayNightSwitch = nullptr;
lv_obj_t *secondModeDropdown = nullptr;
lv_obj_t *weatherIconModeDropdown = nullptr;
lv_obj_t *automaticUpdateSwitch = nullptr;
lv_obj_t *webModeDropdown = nullptr;
lv_obj_t *settingsContent[3] = {};
lv_obj_t *settingsPreviousButton = nullptr;
lv_obj_t *settingsNextButton = nullptr;
lv_obj_t *settingsPageNumberLabel = nullptr;
lv_obj_t *deviceInfoLabel = nullptr;
lv_obj_t *firmwareStatusLabel = nullptr;
lv_obj_t *firmwareCheckButton = nullptr;
lv_obj_t *firmwareInstallButton = nullptr;
lv_obj_t *dayBrightnessTitleLabel = nullptr;
lv_obj_t *nightBrightnessTitleLabel = nullptr;
lv_obj_t *automaticDayNightTitleLabel = nullptr;
lv_obj_t *weatherIconModeTitleLabel = nullptr;
lv_obj_t *secondModeTitleLabel = nullptr;
lv_obj_t *webModeTitleLabel = nullptr;
lv_obj_t *automaticUpdateTitleLabel = nullptr;
lv_obj_t *firmwareCheckLabel = nullptr;
lv_obj_t *firmwareInstallLabel = nullptr;
lv_obj_t *wifiAddressLabel = nullptr;
lv_obj_t *firmwareVersionLabel = nullptr;
lv_obj_t *firmwareUpdateOverlay = nullptr;
lv_obj_t *firmwareUpdateTitleLabel = nullptr;
lv_obj_t *firmwareUpdateCountdownLabel = nullptr;
lv_obj_t *secondDots[SECOND_DOT_COUNT] = {};
lv_obj_t *secondLineBackgroundArc = nullptr;
lv_obj_t *secondLineFadeArc = nullptr;
lv_obj_t *secondLineActiveArc = nullptr;
lv_obj_t *secondLineActiveBridge = nullptr;
lv_obj_t *secondCometHead = nullptr;
int16_t secondDotCenterX[SECOND_DOT_COUNT] = {};
int16_t secondDotCenterY[SECOND_DOT_COUNT] = {};
lv_point_t secondLineActiveBridgePoints[2] = {};
uint8_t displayedSecond = 255;
unsigned long secondTickStartedAt = 0;
bool secondFadeActive = false;
unsigned long secondFadeStartedAt = 0;
unsigned long lastSecondFadeFrameAt = 0;
bool settingsVisible = false;
bool radarVisible = false;
bool radarFeatureAvailable = true;
bool nightModeEnabled = false;
uint8_t nightVisualMode = CLOCK_NIGHT_VISUAL_RED;
bool automaticDayNightEnabled = true;
uint8_t savedDayBrightness = 35;
uint8_t savedNightBrightness = 10;
bool secondRingEnabled = true;
uint8_t secondEffect = CLOCK_SECOND_EFFECT_DOTS;
uint32_t secondRingBackgroundColor = 0xFFFFFF;
uint8_t secondRingBackgroundBrightness = 0;
uint8_t secondRingBackgroundDotSize = 3;
uint8_t secondDotSize = 3;
uint32_t secondDotColor = 0xFFFFFF;
uint32_t leftWeatherIconColor = 0xFFFFFF;
uint32_t rightWeatherIconColor = 0xFFFFFF;
uint8_t secondDotBrightness = 175;
uint32_t outsideColor = 0x4CCBEC;
uint32_t roomColor = 0xFFB843;
uint32_t timeColor = 0xF6F6F6;
uint32_t dateColor = 0xB5B5B5;
uint8_t timeFont = CLOCK_TIME_FONT_BARLOW;
uint8_t language = CLOCK_LANGUAGE_CZECH;
bool outsideUsesWeatherIcon = true;
bool roomUsesWeatherIcon = false;
bool homeAssistantStatusRelevant = true;
char outsideUnit[16] = "°C";
char roomUnit[16] = "°C";
uint8_t outsideDecimals = 1;
uint8_t roomDecimals = 1;
bool weatherConfigured = false;
bool outsideConfigured = false;
bool roomConfigured = false;
bool metricAConfigured = false;
bool metricBConfigured = false;
bool weatherAnimationAvailable = false;
bool animatedWeatherIconsEnabled = true;
uint8_t configuredWeatherIconStyle = CLOCK_WEATHER_ICON_STYLE_MONOCHROME;

uint8_t selectedSecondMode() {
  return secondRingEnabled ? secondEffect + 1 : 0;
}

void applySelectedSecondMode(uint8_t selected) {
  secondRingEnabled = selected != 0;
  if (secondRingEnabled) {
    secondEffect = constrain(
        static_cast<uint8_t>(selected - 1),
        static_cast<uint8_t>(CLOCK_SECOND_EFFECT_DOTS),
        static_cast<uint8_t>(CLOCK_SECOND_EFFECT_COMET));
  }
}

uint8_t selectedWeatherIconMode() {
  if (!animatedWeatherIconsEnabled) return 0;
  if (configuredWeatherIconStyle == CLOCK_WEATHER_ICON_STYLE_FLAT) return 1;
  if (configuredWeatherIconStyle == CLOCK_WEATHER_ICON_STYLE_LINE) return 2;
  return 3;
}

void applySelectedWeatherIconMode(uint8_t selected) {
  animatedWeatherIconsEnabled = selected != 0;
  if (selected == 1) {
    configuredWeatherIconStyle = CLOCK_WEATHER_ICON_STYLE_FLAT;
  } else if (selected == 2) {
    configuredWeatherIconStyle = CLOCK_WEATHER_ICON_STYLE_LINE;
  } else {
    configuredWeatherIconStyle = CLOCK_WEATHER_ICON_STYLE_MONOCHROME;
  }
}
bool weatherAnimationRevealPending = false;
unsigned long weatherAnimationRevealAt = 0;
bool firmwareUpdateActive = false;
uint8_t settingsPageIndex = 0;
uint8_t selectedWebMode = 0;
bool automaticFirmwareUpdateEnabled = false;
bool displayedCanInstall = false;
unsigned long lastSettingsInfoRefreshAt = 0;
char displayedDeviceInfo[64] = "";
char displayedFirmwareStatus[160] = "";
char weatherAnimationKey[48] = "";
char leftWeatherDecoderKey[48] = "";
char rightWeatherDecoderKey[48] = "";
lv_img_dsc_t weatherAnimationSource = {};
bool webActive = false;
bool wifiConnected = false;
uint8_t timeColonEffect = CLOCK_TIME_COLON_STEADY;
char displayedTimeText[6] = "--:--";
uint32_t lastRenderedTimeColonColor = UINT32_MAX;
ClockValues currentValues;
ClockMetricConfig metricAConfig;
ClockMetricConfig metricBConfig;
ClockMetricColorScale metricAColorScale;
ClockMetricColorScale metricBColorScale;
BrightnessPreviewCallback brightnessPreviewCallback = nullptr;
SettingsOpenCallback settingsOpenCallback = nullptr;
SettingsSaveCallback settingsSaveCallback = nullptr;
SettingsActionCallback firmwareCheckCallback = nullptr;
SettingsActionCallback firmwareInstallCallback = nullptr;
RadarVisibilityCallback radarVisibilityCallback = nullptr;
RadarRangeCallback radarRangeCallback = nullptr;

bool redNightVisualEnabled() {
  return nightModeEnabled && nightVisualMode == CLOCK_NIGHT_VISUAL_RED;
}

const lv_font_t *configuredTimeFont() {
  if (timeFont == CLOCK_TIME_FONT_LIBERATION_SANS)
    return &clock_time_liberation_110;
  if (timeFont == CLOCK_TIME_FONT_LCD) return &clock_time_lcd_80;
  if (timeFont == CLOCK_TIME_FONT_DOTO) return &clock_time_doto_98;
  return &clock_time_110;
}

void showSettingsSubpage(uint8_t page);
void alignCenter(lv_obj_t *object, int x, int y);

bool englishLanguage() { return language == CLOCK_LANGUAGE_ENGLISH; }

void applyDashboardLanguage() {
  const bool english = englishLanguage();
  if (dayBrightnessTitleLabel != nullptr)
    lv_label_set_text(dayBrightnessTitleLabel,
                      english ? "DAY BRIGHTNESS" : "DENNÍ JAS");
  if (nightBrightnessTitleLabel != nullptr)
    lv_label_set_text(nightBrightnessTitleLabel,
                      english ? "NIGHT BRIGHTNESS" : "NOČNÍ JAS");
  if (automaticDayNightTitleLabel != nullptr)
    lv_label_set_text(automaticDayNightTitleLabel,
                      english ? "AUTOMATIC DAY/NIGHT"
                              : "AUTOMATICKY DEN/NOC");
  if (weatherIconModeTitleLabel != nullptr)
    lv_label_set_text(weatherIconModeTitleLabel,
                      english ? "WEATHER ICONS" : "IKONY POČASÍ");
  if (weatherIconModeDropdown != nullptr) {
    const uint16_t selected = lv_dropdown_get_selected(weatherIconModeDropdown);
    lv_dropdown_set_options(
        weatherIconModeDropdown,
        english ? "STATIC MONOCHROME\nANIMATED FLAT\nANIMATED LINE\nANIMATED MONOCHROME"
                : "STATICKÉ MONOCHROMATICKÉ\nANIMOVANÉ FLAT\nANIMOVANÉ LINE\nANIMOVANÉ MONOCHROMATICKÉ");
    lv_dropdown_set_selected(weatherIconModeDropdown, selected);
  }
  if (secondModeTitleLabel != nullptr)
    lv_label_set_text(secondModeTitleLabel,
                      english ? "SECONDS" : "VTEŘINY");
  if (secondModeDropdown != nullptr) {
    const uint16_t selected = lv_dropdown_get_selected(secondModeDropdown);
    lv_dropdown_set_options(secondModeDropdown,
                            english ? "OFF\nDOTS\nLINE\nCOMET"
                                    : "VYPNUTO\nTEČKY\nLINKA\nKOMETA");
    lv_dropdown_set_selected(secondModeDropdown, selected);
  }
  if (webModeDropdown != nullptr) {
    const uint16_t selected = lv_dropdown_get_selected(webModeDropdown);
    lv_dropdown_set_options(webModeDropdown,
                            english ? "10 MINUTES\nALWAYS\nOFF"
                                    : "10 MINUT\nVŽDY\nVYPNUTÝ");
    lv_dropdown_set_selected(webModeDropdown, selected);
  }
  if (automaticUpdateTitleLabel != nullptr)
    lv_label_set_text(automaticUpdateTitleLabel,
                      english ? "AUTOMATIC OTA" : "AUTOMATICKÉ OTA");
  if (firmwareCheckLabel != nullptr)
    lv_label_set_text(firmwareCheckLabel,
                      english ? "CHECK" : "ZKONTROLOVAT");
  if (firmwareInstallLabel != nullptr)
    lv_label_set_text(firmwareInstallLabel,
                      english ? "UPDATE" : "AKTUALIZOVAT");
  if (firmwareUpdateTitleLabel != nullptr)
    lv_label_set_text(firmwareUpdateTitleLabel,
                      english ? "FIRMWARE UPDATE"
                              : "AKTUALIZACE FIRMWARE");
  displayedDeviceInfo[0] = '\0';
  displayedFirmwareStatus[0] = '\0';
}

void setRadarVisible(bool visible) {
  if (visible && !radarFeatureAvailable) return;
  if (radarVisible == visible || settingsVisible) return;
  radarVisible = visible;
  if (visible) {
    // Při návratu na radar neodkrývej snímek, který zůstal v canvasu z
    // předchozího cyklu. Canvas znovu zobrazí až první snapshot nové animace.
    lv_obj_add_flag(radarCanvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(radarProgressBar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(dashboardContent, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(radarPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(radarPage);
  } else {
    lv_obj_add_flag(radarPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(dashboardContent, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(dashboardContent);
  }
  if (radarVisibilityCallback != nullptr) radarVisibilityCallback(visible);
}

void setObjectVisible(lv_obj_t *object, bool visible) {
  if (visible) {
    lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
  }
}

const char *roomIconGlyph(const char *icon) {
  if (strcmp(icon, "home") == 0) return "\xEF\x80\x95";         // U+F015
  if (strcmp(icon, "living-room") == 0) return "\xEF\x92\xB8";  // U+F4B8
  if (strcmp(icon, "kitchen") == 0) return "\xEF\x8B\xA7";      // U+F2E7
  if (strcmp(icon, "none") == 0 || strcmp(icon, "weather") == 0) return "";
  return "\xEF\x88\xB6";  // U+F236
}

void normalizeMicroSign(char *text) {
  if (text == nullptr) return;
  for (size_t index = 0; text[index] != '\0'; ++index) {
    if (static_cast<uint8_t>(text[index]) == 0xCE &&
        static_cast<uint8_t>(text[index + 1]) == 0xBC) {
      // Řecké malé mí U+03BC nahraď znakem mikro U+00B5, který obsahují
      // dashboardové fonty. Oba znaky mají v UTF-8 stejnou délku.
      text[index] = static_cast<char>(0xC2);
      text[index + 1] = static_cast<char>(0xB5);
      ++index;
    }
  }
}

void ensureWeatherAnimationDecoders() {
  if (!weatherAnimationAvailable || !animatedWeatherIconsEnabled ||
      weatherAnimationKey[0] == '\0') {
    return;
  }
  auto updateDecoder = [](lv_obj_t *&decoder, char *decoderKey,
                          size_t decoderKeySize, bool used, lv_coord_t x) {
    if (used) {
      if (strcmp(decoderKey, weatherAnimationKey) != 0) {
        lv_gif_set_src(decoder, &weatherAnimationSource);
        strlcpy(decoderKey, weatherAnimationKey, decoderKeySize);
      }
      return;
    }
    if (decoderKey[0] == '\0') return;

    // Skrytí LVGL GIF objektu nezastaví jeho timer. Objekt proto před
    // uvolněním starého assetu zrušíme a vytvoříme znovu bez zdroje.
    lv_obj_t *parent = lv_obj_get_parent(decoder);
    lv_obj_del(decoder);
    decoder = lv_gif_create(parent);
    alignCenter(decoder, x, 107);
    lv_obj_add_flag(decoder, LV_OBJ_FLAG_HIDDEN);
    decoderKey[0] = '\0';
  };

  updateDecoder(weatherAnimation, leftWeatherDecoderKey,
                sizeof(leftWeatherDecoderKey), outsideUsesWeatherIcon, -142);
  updateDecoder(roomWeatherAnimation, rightWeatherDecoderKey,
                sizeof(rightWeatherDecoderKey), roomUsesWeatherIcon, 142);
}

lv_color_t configuredColor(uint32_t color) {
  // LVGL 8.3 rozbaluje LV_COLOR_MAKE do makra, jehož argumenty nejsou
  // uzávorkované. Bitové výrazy by se proto kvůli prioritě operátorů
  // vyhodnotily chybně. Kanály nejdřív materializujeme do samostatných hodnot.
  const uint8_t red = static_cast<uint8_t>((color >> 16) & 0xFF);
  const uint8_t green = static_cast<uint8_t>((color >> 8) & 0xFF);
  const uint8_t blue = static_cast<uint8_t>(color & 0xFF);
  return LV_COLOR_MAKE(red, green, blue);
}

lv_color_t interpolateColor(uint32_t from, uint32_t to, float progress) {
  progress = constrain(progress, 0.0f, 1.0f);
  const uint8_t fromRed = static_cast<uint8_t>((from >> 16) & 0xFF);
  const uint8_t fromGreen = static_cast<uint8_t>((from >> 8) & 0xFF);
  const uint8_t fromBlue = static_cast<uint8_t>(from & 0xFF);
  const uint8_t toRed = static_cast<uint8_t>((to >> 16) & 0xFF);
  const uint8_t toGreen = static_cast<uint8_t>((to >> 8) & 0xFF);
  const uint8_t toBlue = static_cast<uint8_t>(to & 0xFF);
  const uint8_t red = static_cast<uint8_t>(
      fromRed + (static_cast<int>(toRed) - fromRed) * progress);
  const uint8_t green = static_cast<uint8_t>(
      fromGreen + (static_cast<int>(toGreen) - fromGreen) * progress);
  const uint8_t blue = static_cast<uint8_t>(
      fromBlue + (static_cast<int>(toBlue) - fromBlue) * progress);
  return LV_COLOR_MAKE(red, green, blue);
}

lv_color_t metricColorForValue(float value,
                               const ClockMetricColorScale &scale) {
  if (std::isnan(value)) return COLOR_MUTED;
  const uint8_t count = constrain(
      scale.count, static_cast<uint8_t>(1),
      static_cast<uint8_t>(CLOCK_METRIC_COLOR_POINT_COUNT));
  if (value <= scale.points[0].value) {
    return configuredColor(scale.points[0].color);
  }
  for (uint8_t index = 1; index < count; ++index) {
    if (value < scale.points[index].value) {
      const ClockMetricColorPoint &from = scale.points[index - 1];
      const ClockMetricColorPoint &to = scale.points[index];
      return interpolateColor(from.color, to.color,
                              (value - from.value) / (to.value - from.value));
    }
  }
  return configuredColor(scale.points[count - 1].color);
}

lv_obj_t *makeLabel(lv_obj_t *parent, const lv_font_t *font, lv_color_t color) {
  lv_obj_t *label = lv_label_create(parent);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  return label;
}

void alignCenter(lv_obj_t *object, int x, int y) {
  lv_obj_align(object, LV_ALIGN_CENTER, x, y);
}

void alignConnectionStatusIcons() {
  constexpr int STATUS_Y = 202;
  constexpr int STATUS_SPACING = 28;
  const bool redNightVisual = redNightVisualEnabled();
  const bool showWifi = !redNightVisual || wifiConnected;
  const bool showHomeAssistant =
      homeAssistantStatusRelevant &&
      (!redNightVisual || currentValues.homeAssistantOnline);
  const bool showWeb = webActive;

  lv_obj_t *icons[] = {wifiStatusLabel, statusLabel, webStatusLabel};
  const bool visible[] = {showWifi, showHomeAssistant, showWeb};
  int visibleCount = 0;
  for (bool iconVisible : visible) {
    if (iconVisible) ++visibleCount;
  }

  int visibleIndex = 0;
  for (int index = 0; index < 3; ++index) {
    if (!visible[index]) {
      lv_obj_add_flag(icons[index], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_clear_flag(icons[index], LV_OBJ_FLAG_HIDDEN);
    const int x =
        visibleIndex * STATUS_SPACING - (visibleCount - 1) * STATUS_SPACING / 2;
    alignCenter(icons[index], x, STATUS_Y);
    ++visibleIndex;
  }
}

void makeChildrenTapThrough(lv_obj_t *parent) {
  const uint32_t childCount = lv_obj_get_child_cnt(parent);
  for (uint32_t index = 0; index < childCount; ++index) {
    lv_obj_t *child = lv_obj_get_child(parent, index);
    lv_obj_clear_flag(child, LV_OBJ_FLAG_CLICKABLE);
    makeChildrenTapThrough(child);
  }
}

void makeDivider(lv_obj_t *parent, int width, int height, int x, int y) {
  lv_obj_t *divider = lv_obj_create(parent);
  lv_obj_set_size(divider, width, height);
  lv_obj_set_style_radius(divider, 3, 0);
  lv_obj_set_style_border_width(divider, 0, 0);
  lv_obj_set_style_bg_color(divider, COLOR_DIVIDER, 0);
  alignCenter(divider, x, y);
  lv_obj_clear_flag(divider, LV_OBJ_FLAG_SCROLLABLE);
}

void setTopValue(float value, uint8_t decimals, int centerX,
                 lv_obj_t *integerLabel, lv_obj_t *decimalLabel,
                 lv_obj_t *unitLabel) {
  char valueText[20];
  if (std::isnan(value)) {
    snprintf(valueText, sizeof(valueText), "--");
  } else {
    snprintf(valueText, sizeof(valueText), decimals == 0 ? "%.0f" : "%.1f",
             value);
  }

  char *decimalPoint = strchr(valueText, '.');
  char decimalText[4] = "";
  if (decimalPoint != nullptr) {
    snprintf(decimalText, sizeof(decimalText), ",%c", decimalPoint[1]);
    *decimalPoint = '\0';
  }

  lv_label_set_text(integerLabel, valueText);
  lv_label_set_text(decimalLabel, decimalText);
  lv_obj_update_layout(integerLabel);
  lv_obj_update_layout(decimalLabel);
  lv_obj_update_layout(unitLabel);

  constexpr int DECIMAL_GAP = 1;
  constexpr int UNIT_GAP = 6;
  const int decimalWidth = decimalText[0] == '\0' ? 0 : lv_obj_get_width(decimalLabel);
  const int totalWidth = lv_obj_get_width(integerLabel) + decimalWidth +
                         (decimalWidth > 0 ? DECIMAL_GAP : 0) + UNIT_GAP +
                         lv_obj_get_width(unitLabel);
  int x = centerX - totalWidth / 2;
  lv_obj_set_pos(integerLabel, x, 262);
  x += lv_obj_get_width(integerLabel) + (decimalWidth > 0 ? DECIMAL_GAP : 0);
  lv_obj_set_pos(decimalLabel, x, 275);
  x += decimalWidth + UNIT_GAP;
  lv_obj_set_pos(unitLabel, x, 264);
}

void alignCo2Value() {
  lv_obj_update_layout(co2ValueLabel);
  lv_obj_update_layout(co2UnitLabel);
  constexpr int GAP = 7;
  const int totalWidth = lv_obj_get_width(co2ValueLabel) + GAP +
                         lv_obj_get_width(co2UnitLabel);
  const int x = 240 - totalWidth / 2;
  lv_obj_set_pos(co2ValueLabel, x, 320);
  lv_obj_set_pos(co2UnitLabel, x + lv_obj_get_width(co2ValueLabel) + GAP, 334);
}

void alignMetricBValue() {
  lv_obj_update_layout(humidityTitleLabel);
  lv_obj_update_layout(humidityValueLabel);
  lv_obj_update_layout(humidityUnitLabel);
  constexpr int TITLE_VALUE_GAP = 12;
  constexpr int VALUE_UNIT_GAP = 4;
  const int titleWidth = lv_obj_get_width(humidityTitleLabel);
  const int valueWidth = lv_obj_get_width(humidityValueLabel);
  const int unitWidth = lv_obj_get_width(humidityUnitLabel);
  const int totalWidth = titleWidth + TITLE_VALUE_GAP + valueWidth +
                         (unitWidth > 0 ? VALUE_UNIT_GAP + unitWidth : 0);
  int x = -totalWidth / 2;
  alignCenter(humidityTitleLabel, x + titleWidth / 2, 151);
  x += titleWidth + TITLE_VALUE_GAP;
  alignCenter(humidityValueLabel, x + valueWidth / 2, 151);
  x += valueWidth + VALUE_UNIT_GAP;
  alignCenter(humidityUnitLabel, x + unitWidth / 2, 154);
}

void formatMetricValue(char *buffer, size_t bufferSize, float value,
                       uint8_t decimals) {
  if (std::isnan(value)) {
    strlcpy(buffer, "--", bufferSize);
    return;
  }
  snprintf(buffer, bufferSize, "%.*f", constrain(decimals, 0, 2), value);
}

void makeSecondRing(lv_obj_t *parent) {
  for (int index = 0; index < SECOND_DOT_COUNT; ++index) {
    const float angle = (static_cast<float>(index) / SECOND_DOT_COUNT) *
                            2.0f * PI_VALUE -
                        PI_VALUE / 2.0f;
    const int x = 240 + static_cast<int>(std::round(std::cos(angle) *
                                                    SECOND_RING_RADIUS));
    const int y = 240 + static_cast<int>(std::round(std::sin(angle) *
                                                    SECOND_RING_RADIUS));
    secondDotCenterX[index] = x;
    secondDotCenterY[index] = y;
    secondDots[index] = lv_obj_create(parent);
    lv_obj_set_size(secondDots[index], secondRingBackgroundDotSize,
                    secondRingBackgroundDotSize);
    lv_obj_set_style_radius(secondDots[index], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(secondDots[index], 0, 0);
    lv_obj_set_style_bg_color(secondDots[index],
                              lv_color_make(secondRingBackgroundBrightness,
                                            secondRingBackgroundBrightness,
                                            secondRingBackgroundBrightness),
                              0);
    lv_obj_set_style_pad_all(secondDots[index], 0, 0);
    lv_obj_set_pos(secondDots[index], x - secondRingBackgroundDotSize / 2,
                   y - secondRingBackgroundDotSize / 2);
    lv_obj_clear_flag(secondDots[index], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(secondDots[index], LV_OBJ_FLAG_CLICKABLE);
  }

  auto makeArc = [parent]() {
    lv_obj_t *arc = lv_arc_create(parent);
    lv_arc_set_rotation(arc, 270);
    lv_obj_remove_style(arc, nullptr, LV_PART_KNOB);
    lv_obj_remove_style(arc, nullptr, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, false, LV_PART_MAIN);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    return arc;
  };
  auto makeBridge = [parent]() {
    lv_obj_t *line = lv_line_create(parent);
    lv_obj_set_size(line, 480, 480);
    lv_obj_set_pos(line, 0, 0);
    lv_obj_set_style_line_rounded(line, false, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
    return line;
  };
  secondLineBackgroundArc = makeArc();
  secondLineFadeArc = makeArc();
  secondLineActiveArc = makeArc();
  secondLineActiveBridge = makeBridge();

  secondCometHead = lv_obj_create(parent);
  lv_obj_set_style_radius(secondCometHead, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(secondCometHead, 0, 0);
  lv_obj_set_style_pad_all(secondCometHead, 0, 0);
  lv_obj_clear_flag(secondCometHead, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(secondCometHead, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(secondCometHead, LV_OBJ_FLAG_HIDDEN);
}

lv_color_t brightnessScaledColor(uint32_t color, uint8_t brightness) {
  const uint8_t red = static_cast<uint8_t>(
      (((color >> 16) & 0xFF) * brightness + 127) / 255);
  const uint8_t green = static_cast<uint8_t>(
      (((color >> 8) & 0xFF) * brightness + 127) / 255);
  const uint8_t blue = static_cast<uint8_t>(
      ((color & 0xFF) * brightness + 127) / 255);
  return lv_color_make(red, green, blue);
}

uint8_t brightnessScaledChannel(uint32_t color, uint8_t brightness,
                                uint8_t shift) {
  return static_cast<uint8_t>(
      (((color >> shift) & 0xFF) * brightness + 127) / 255);
}

lv_point_t secondLinePoint(float fraction) {
  const float angle = fraction * 2.0f * PI_VALUE - PI_VALUE / 2.0f;
  return {
      static_cast<lv_coord_t>(240 + std::round(std::cos(angle) * SECOND_RING_RADIUS)),
      static_cast<lv_coord_t>(240 + std::round(std::sin(angle) * SECOND_RING_RADIUS)),
  };
}

void configureSecondArc(lv_obj_t *arc, uint8_t width, lv_color_t color) {
  const int diameter = static_cast<int>(SECOND_RING_RADIUS * 2.0f) + width;
  lv_obj_set_size(arc, diameter, diameter);
  alignCenter(arc, 0, 0);
  lv_obj_set_style_arc_width(arc, width, LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, color, LV_PART_MAIN);
}

void renderSecondDots(unsigned long now) {
  const unsigned long elapsed = now - secondFadeStartedAt;
  for (int index = 0; index < SECOND_DOT_COUNT; ++index) {
    const bool dotsEffect = secondEffect == CLOCK_SECOND_EFFECT_DOTS;
    if (!secondRingEnabled || !dotsEffect) {
      lv_obj_add_flag(secondDots[index], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_clear_flag(secondDots[index], LV_OBJ_FLAG_HIDDEN);
    float intensity = 0.0f;
    if (secondFadeActive) {
      const unsigned long fadeStart =
          static_cast<unsigned long>(index) * SECOND_FADE_START_SPAN_MS /
          (SECOND_DOT_COUNT - 1);
      if (elapsed <= fadeStart) {
        intensity = 1.0f;
      } else if (elapsed < fadeStart + SECOND_FADE_DOT_MS) {
        const unsigned long dotElapsed = elapsed - fadeStart;
        const float fadeProgress = static_cast<float>(dotElapsed) /
                                   SECOND_FADE_DOT_MS;
        intensity = 1.0f - fadeProgress;
      }
    }

    // Nová minuta má před doznívajícím starým prstencem prioritu.
    if (index < displayedSecond) intensity = 1.0f;
    // Noční režim mění pouze aktivní část prstence. Neaktivní pozadí zůstává
    // v uživatelem nastavené barvě a jasu.
    const uint32_t activeColor =
        redNightVisualEnabled() ? 0xFF0000 : secondDotColor;
    const uint8_t backgroundRed = brightnessScaledChannel(
        secondRingBackgroundColor, secondRingBackgroundBrightness, 16);
    const uint8_t backgroundGreen = brightnessScaledChannel(
        secondRingBackgroundColor, secondRingBackgroundBrightness, 8);
    const uint8_t backgroundBlue = brightnessScaledChannel(
        secondRingBackgroundColor, secondRingBackgroundBrightness, 0);
    const uint8_t activeRed =
        brightnessScaledChannel(activeColor, secondDotBrightness, 16);
    const uint8_t activeGreen =
        brightnessScaledChannel(activeColor, secondDotBrightness, 8);
    const uint8_t activeBlue =
        brightnessScaledChannel(activeColor, secondDotBrightness, 0);
    const uint8_t red = static_cast<uint8_t>(
        backgroundRed + (static_cast<int>(activeRed) - backgroundRed) *
                            intensity);
    const uint8_t green = static_cast<uint8_t>(
        backgroundGreen + (static_cast<int>(activeGreen) - backgroundGreen) *
                              intensity);
    const uint8_t blue = static_cast<uint8_t>(
        backgroundBlue + (static_cast<int>(activeBlue) - backgroundBlue) *
                             intensity);
    const uint8_t renderedSize = constrain(
        static_cast<int>(std::round(
            secondRingBackgroundDotSize +
            (static_cast<int>(secondDotSize) - secondRingBackgroundDotSize) *
                intensity)),
        1, 10);
    lv_obj_set_size(secondDots[index], renderedSize, renderedSize);
    lv_obj_set_pos(secondDots[index],
                   secondDotCenterX[index] - renderedSize / 2,
                   secondDotCenterY[index] - renderedSize / 2);
    lv_obj_set_style_bg_color(
        secondDots[index], lv_color_make(red, green, blue), 0);
  }
}

void renderSecondComet(unsigned long now) {
  const bool visible = secondRingEnabled &&
                       secondEffect == CLOCK_SECOND_EFFECT_COMET;
  if (visible)
    lv_obj_clear_flag(secondCometHead, LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_add_flag(secondCometHead, LV_OBJ_FLAG_HIDDEN);
  if (!visible) return;

  const uint32_t configuredActiveColor =
      redNightVisualEnabled() ? 0xFF0000 : secondDotColor;
  const float elapsedWithinSecond = constrain(
      static_cast<float>(now - secondTickStartedAt) / 1000.0f, 0.0f, 1.0f);
  const float headSecond =
      static_cast<float>(displayedSecond) + elapsedWithinSecond;
  const float pulse = std::sin(elapsedWithinSecond * PI_VALUE);
  const uint8_t pulsingBrightness = constrain(
      static_cast<int>(std::round(secondDotBrightness *
                                  (1.0f + 0.35f * pulse))),
      0, 255);

  // Tečky zůstávají na pevných bodech kružnice. Plynulý pohyb vzniká pouze
  // přeléváním jasu mezi sousedními body, takže ocas nemůže kličkovat do stran.
  for (int index = 0; index < SECOND_DOT_COUNT; ++index) {
    float distanceBehind = headSecond - static_cast<float>(index);
    while (distanceBehind < -30.0f) distanceBehind += 60.0f;
    while (distanceBehind >= 30.0f) distanceBehind -= 60.0f;

    float trailIntensity = 0.0f;
    if (distanceBehind >= 0.45f &&
        distanceBehind <= SECOND_COMET_TRAIL_SECONDS) {
      const float trailPosition =
          distanceBehind / SECOND_COMET_TRAIL_SECONDS;
      trailIntensity = 0.65f *
                       (1.0f - trailPosition) * (1.0f - trailPosition);
    }

    if (trailIntensity < 0.015f) {
      lv_obj_add_flag(secondDots[index], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_clear_flag(secondDots[index], LV_OBJ_FLAG_HIDDEN);
    const uint8_t activeRed =
        brightnessScaledChannel(configuredActiveColor, secondDotBrightness, 16);
    const uint8_t activeGreen =
        brightnessScaledChannel(configuredActiveColor, secondDotBrightness, 8);
    const uint8_t activeBlue =
        brightnessScaledChannel(configuredActiveColor, secondDotBrightness, 0);
    const uint8_t renderedSize = secondDotSize;
    lv_obj_set_size(secondDots[index], renderedSize, renderedSize);
    lv_obj_set_pos(secondDots[index],
                   secondDotCenterX[index] - renderedSize / 2,
                   secondDotCenterY[index] - renderedSize / 2);
    lv_obj_set_style_bg_color(
        secondDots[index],
        lv_color_make(static_cast<uint8_t>(activeRed * trailIntensity),
                      static_cast<uint8_t>(activeGreen * trailIntensity),
                      static_cast<uint8_t>(activeBlue * trailIntensity)),
        0);
  }

  const uint8_t headRed =
      brightnessScaledChannel(configuredActiveColor, pulsingBrightness, 16);
  const uint8_t headGreen =
      brightnessScaledChannel(configuredActiveColor, pulsingBrightness, 8);
  const uint8_t headBlue =
      brightnessScaledChannel(configuredActiveColor, pulsingBrightness, 0);
  const uint8_t headSize =
      constrain(static_cast<int>(secondDotSize) + 3, 4, 13);
  const lv_point_t headCenter = secondLinePoint(headSecond / 60.0f);
  lv_obj_set_size(secondCometHead, headSize, headSize);
  lv_obj_set_pos(secondCometHead, headCenter.x - headSize / 2,
                 headCenter.y - headSize / 2);
  lv_obj_set_style_bg_color(secondCometHead,
                            lv_color_make(headRed, headGreen, headBlue), 0);
}

void renderSecondLine(unsigned long now) {
  const bool visible = secondRingEnabled &&
                       secondEffect == CLOCK_SECOND_EFFECT_LINE;
  lv_obj_t *objects[] = {secondLineBackgroundArc, secondLineFadeArc,
                         secondLineActiveArc, secondLineActiveBridge};
  for (lv_obj_t *object : objects) {
    if (visible) {
      lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (!visible) return;

  const uint32_t configuredActiveColor =
      redNightVisualEnabled() ? 0xFF0000 : secondDotColor;
  const lv_color_t backgroundColor = brightnessScaledColor(
      secondRingBackgroundColor, secondRingBackgroundBrightness);
  const lv_color_t activeColor =
      brightnessScaledColor(configuredActiveColor, secondDotBrightness);
  configureSecondArc(secondLineBackgroundArc, secondRingBackgroundDotSize,
                     backgroundColor);
  configureSecondArc(secondLineActiveArc, secondDotSize, activeColor);
  lv_arc_set_bg_angles(secondLineBackgroundArc, 0, 360);
  lv_obj_set_style_line_color(secondLineActiveBridge, activeColor, 0);
  lv_obj_set_style_line_width(secondLineActiveBridge, secondDotSize, 0);

  const float elapsedWithinSecond = constrain(
      static_cast<float>(now - secondTickStartedAt) / 1000.0f, 0.0f, 1.0f);
  const float activeDegrees = constrain(
      (static_cast<float>(displayedSecond) + elapsedWithinSecond) * 6.0f,
      0.0f, 360.0f);
  const int activeWholeDegrees = static_cast<int>(std::floor(activeDegrees));
  if (activeWholeDegrees <= 0) {
    lv_obj_add_flag(secondLineActiveArc, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_arc_set_bg_angles(secondLineActiveArc, 0,
                         min(activeWholeDegrees, 360));
  }
  if (activeDegrees > activeWholeDegrees && activeDegrees < 360.0f) {
    secondLineActiveBridgePoints[0] =
        secondLinePoint(activeWholeDegrees / 360.0f);
    secondLineActiveBridgePoints[1] = secondLinePoint(activeDegrees / 360.0f);
    lv_line_set_points(secondLineActiveBridge, secondLineActiveBridgePoints, 2);
  } else {
    lv_obj_add_flag(secondLineActiveBridge, LV_OBJ_FLAG_HIDDEN);
  }

  if (secondFadeActive) {
    const float progress = constrain(
        static_cast<float>(now - secondFadeStartedAt) /
            SECOND_LINE_FADE_TOTAL_MS,
        0.0f, 1.0f);
    // Smoothstep má nulovou rychlost na začátku i na konci. Starý kruh tak
    // neproblikne ani náhle nezmizí, ale plynule se rozpustí do pozadí.
    const float intensity = 1.0f - progress * progress * (3.0f - 2.0f * progress);
    const uint8_t backgroundRed = brightnessScaledChannel(
        secondRingBackgroundColor, secondRingBackgroundBrightness, 16);
    const uint8_t backgroundGreen = brightnessScaledChannel(
        secondRingBackgroundColor, secondRingBackgroundBrightness, 8);
    const uint8_t backgroundBlue = brightnessScaledChannel(
        secondRingBackgroundColor, secondRingBackgroundBrightness, 0);
    const uint8_t activeRed = brightnessScaledChannel(
        configuredActiveColor, secondDotBrightness, 16);
    const uint8_t activeGreen = brightnessScaledChannel(
        configuredActiveColor, secondDotBrightness, 8);
    const uint8_t activeBlue = brightnessScaledChannel(
        configuredActiveColor, secondDotBrightness, 0);
    const lv_color_t fadingColor = lv_color_make(
        static_cast<uint8_t>(backgroundRed +
                             (activeRed - backgroundRed) * intensity),
        static_cast<uint8_t>(backgroundGreen +
                             (activeGreen - backgroundGreen) * intensity),
        static_cast<uint8_t>(backgroundBlue +
                             (activeBlue - backgroundBlue) * intensity));
    const uint8_t fadingWidth = constrain(
        static_cast<int>(std::round(
            secondRingBackgroundDotSize +
            (static_cast<int>(secondDotSize) - secondRingBackgroundDotSize) *
                intensity)),
        1, 10);
    configureSecondArc(secondLineFadeArc, fadingWidth, fadingColor);
    lv_arc_set_bg_angles(secondLineFadeArc, 0, 360);
  } else {
    lv_obj_add_flag(secondLineFadeArc, LV_OBJ_FLAG_HIDDEN);
  }
}

void renderSecondRing(unsigned long now) {
  const unsigned long fadeDuration =
      secondEffect == CLOCK_SECOND_EFFECT_LINE ? SECOND_LINE_FADE_TOTAL_MS
                                               : SECOND_DOT_FADE_TOTAL_MS;
  if (secondFadeActive && now - secondFadeStartedAt >= fadeDuration) {
    secondFadeActive = false;
  }
  renderSecondDots(now);
  renderSecondLine(now);
  renderSecondComet(now);
}

void setTextColor(lv_obj_t *object, lv_color_t color) {
  if (object == nullptr) return;
  lv_obj_set_style_text_color(object, color, 0);
}

void renderTimeColon(unsigned long now, bool force = false) {
  if (timeLabel == nullptr) return;
  if (timeColonEffect == CLOCK_TIME_COLON_STEADY) {
    if (force || lastRenderedTimeColonColor != UINT32_MAX) {
      lv_label_set_text(timeLabel, displayedTimeText);
      alignCenter(timeLabel, 0, -105);
      lastRenderedTimeColonColor = UINT32_MAX;
    }
    return;
  }

  const char *colon = strchr(displayedTimeText, ':');
  if (colon == nullptr) return;
  float intensity = (displayedSecond & 1U) == 0 ? 1.0f : 0.0f;
  if (timeColonEffect == CLOCK_TIME_COLON_FADE) {
    const float elapsedWithinSecond = constrain(
        static_cast<float>(now - secondTickStartedAt) / 1000.0f, 0.0f, 1.0f);
    const float phase = static_cast<float>(displayedSecond & 1U) +
                        elapsedWithinSecond;
    intensity = 0.5f - 0.5f * std::cos(phase * PI_VALUE);
  }
  // Inline barva dvojtečky musí na maximu přesně odpovídat barvě celého
  // časového labelu v červeném nočním režimu (COLOR_ERROR).
  const uint32_t baseColor = redNightVisualEnabled() ? 0xFF4848 : timeColor;
  const uint8_t red = static_cast<uint8_t>(
      std::round(((baseColor >> 16) & 0xFF) * intensity));
  const uint8_t green = static_cast<uint8_t>(
      std::round(((baseColor >> 8) & 0xFF) * intensity));
  const uint8_t blue = static_cast<uint8_t>(
      std::round((baseColor & 0xFF) * intensity));
  const uint32_t renderedColor =
      (static_cast<uint32_t>(red) << 16) |
      (static_cast<uint32_t>(green) << 8) | blue;
  if (!force && renderedColor == lastRenderedTimeColonColor) return;

  char renderedText[24];
  snprintf(renderedText, sizeof(renderedText), "%.*s#%02x%02x%02x :#%s",
           static_cast<int>(colon - displayedTimeText), displayedTimeText, red,
           green, blue, colon + 1);
  lv_label_set_text(timeLabel, renderedText);
  alignCenter(timeLabel, 0, -105);
  lastRenderedTimeColonColor = renderedColor;
}

void applyDashboardColors() {
  const bool animationIsMonochrome =
      strncmp(weatherAnimationKey, "monochrome-", 11) == 0;
  if (redNightVisualEnabled()) {
    lv_obj_t *coloredLabels[] = {
        timeLabel,          dateLabel,          outsideTitleLabel,
        outsideIntegerLabel, outsideDecimalLabel, outsideUnitLabel,
        roomTitleLabel, roomIntegerLabel, roomDecimalLabel,
        roomUnitLabel,  outsideIconLabel,     roomIconLabel,
        co2TitleLabel,
        co2ValueLabel,     co2UnitLabel,        humidityTitleLabel,
        humidityValueLabel, humidityUnitLabel,
    };
    for (lv_obj_t *label : coloredLabels) setTextColor(label, COLOR_ERROR);
    setTextColor(radarTitleLabel, COLOR_ERROR);
    setTextColor(radarStatusLabel, COLOR_ERROR);
    if (radarProgressBar != nullptr) {
      lv_obj_set_style_bg_color(radarProgressBar,
                                LV_COLOR_MAKE(58, 14, 14), LV_PART_MAIN);
      lv_obj_set_style_bg_color(radarProgressBar, COLOR_ERROR,
                                LV_PART_INDICATOR);
    }
    lv_obj_set_style_img_recolor(weatherImage, COLOR_ERROR, 0);
    lv_obj_set_style_img_recolor_opa(weatherImage, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(roomWeatherImage, COLOR_ERROR, 0);
    lv_obj_set_style_img_recolor_opa(roomWeatherImage, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(weatherAnimation, COLOR_ERROR, 0);
    lv_obj_set_style_img_recolor_opa(weatherAnimation, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(roomWeatherAnimation, COLOR_ERROR, 0);
    lv_obj_set_style_img_recolor_opa(roomWeatherAnimation, LV_OPA_COVER, 0);
  } else {
    const lv_color_t configuredOutsideColor = configuredColor(outsideColor);
    const lv_color_t configuredRoomColor = configuredColor(roomColor);
    setTextColor(timeLabel, configuredColor(timeColor));
    setTextColor(radarTitleLabel, COLOR_TEXT);
    setTextColor(radarStatusLabel, COLOR_OUTSIDE);
    if (radarProgressBar != nullptr) {
      lv_obj_set_style_bg_color(radarProgressBar, COLOR_DIVIDER,
                                LV_PART_MAIN);
      lv_obj_set_style_bg_color(radarProgressBar,
                                radarFullPreparationInProgress ? COLOR_ERROR
                                                               : COLOR_OUTSIDE,
                                LV_PART_INDICATOR);
    }
    setTextColor(dateLabel, configuredColor(dateColor));
    setTextColor(outsideTitleLabel, configuredOutsideColor);
    setTextColor(outsideIntegerLabel, configuredOutsideColor);
    setTextColor(outsideDecimalLabel, configuredOutsideColor);
    setTextColor(outsideUnitLabel, configuredOutsideColor);
    setTextColor(outsideIconLabel, configuredColor(leftWeatherIconColor));
    setTextColor(roomTitleLabel, configuredRoomColor);
    setTextColor(roomIntegerLabel, configuredRoomColor);
    setTextColor(roomDecimalLabel, configuredRoomColor);
    setTextColor(roomUnitLabel, configuredRoomColor);
    setTextColor(roomIconLabel, configuredColor(rightWeatherIconColor));
    const lv_color_t co2Color =
        metricColorForValue(currentValues.metricAValue, metricAColorScale);
    setTextColor(co2TitleLabel, co2Color);
    setTextColor(co2ValueLabel, co2Color);
    setTextColor(co2UnitLabel, co2Color);
    const lv_color_t configuredMetricBColor =
        metricColorForValue(currentValues.metricBValue, metricBColorScale);
    setTextColor(humidityTitleLabel, configuredMetricBColor);
    setTextColor(humidityValueLabel, configuredMetricBColor);
    setTextColor(humidityUnitLabel, configuredMetricBColor);
    lv_obj_set_style_img_recolor(weatherImage,
                                 configuredColor(leftWeatherIconColor), 0);
    lv_obj_set_style_img_recolor_opa(weatherImage, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(roomWeatherImage,
                                 configuredColor(rightWeatherIconColor), 0);
    lv_obj_set_style_img_recolor_opa(roomWeatherImage, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(weatherAnimation,
                                 configuredColor(leftWeatherIconColor), 0);
    lv_obj_set_style_img_recolor_opa(
        weatherAnimation,
        animationIsMonochrome ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_img_recolor(roomWeatherAnimation,
                                 configuredColor(rightWeatherIconColor), 0);
    lv_obj_set_style_img_recolor_opa(
        roomWeatherAnimation,
        animationIsMonochrome ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  }
  if (redNightVisualEnabled()) {
    setTextColor(wifiStatusLabel, COLOR_ERROR);
    setTextColor(statusLabel, COLOR_ERROR);
    setTextColor(webStatusLabel, COLOR_ERROR);
  } else {
    setTextColor(wifiStatusLabel, wifiConnected ? COLOR_AIR : COLOR_ERROR);
    setTextColor(statusLabel,
                 currentValues.homeAssistantOnline ? COLOR_AIR : COLOR_ERROR);
    setTextColor(webStatusLabel, COLOR_OUTSIDE);
  }
  alignConnectionStatusIcons();
  renderSecondRing(millis());
  renderTimeColon(millis(), true);
}

void updateBrightnessLabel(lv_obj_t *label, int brightness, int x, int y) {
  char text[8];
  snprintf(text, sizeof(text), "%d %%", brightness);
  lv_label_set_text(label, text);
  alignCenter(label, x, y);
}

void showSettings() {
  if (settingsVisible) return;
  if (settingsOpenCallback != nullptr) settingsOpenCallback();
  lv_slider_set_value(dayBrightnessSlider, savedDayBrightness, LV_ANIM_OFF);
  lv_slider_set_value(nightBrightnessSlider, savedNightBrightness, LV_ANIM_OFF);
  updateBrightnessLabel(dayBrightnessValueLabel, savedDayBrightness, 105, -72);
  updateBrightnessLabel(nightBrightnessValueLabel, savedNightBrightness, 105, 8);
  if (automaticDayNightEnabled) {
    lv_obj_add_state(automaticDayNightSwitch, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(automaticDayNightSwitch, LV_STATE_CHECKED);
  }
  lv_dropdown_set_selected(secondModeDropdown, selectedSecondMode());
  lv_dropdown_set_selected(weatherIconModeDropdown,
                           selectedWeatherIconMode());
  if (automaticFirmwareUpdateEnabled)
    lv_obj_add_state(automaticUpdateSwitch, LV_STATE_CHECKED);
  else
    lv_obj_clear_state(automaticUpdateSwitch, LV_STATE_CHECKED);
  lv_dropdown_set_selected(webModeDropdown, selectedWebMode);
  showSettingsSubpage(0);
  settingsVisible = true;
  lv_obj_clear_flag(settingsPage, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(settingsPage);
}

void openSettingsEvent(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_LONG_PRESSED) showSettings();
}

void createRadarPage(lv_obj_t *screen) {
  radarPage = lv_obj_create(screen);
  lv_obj_set_size(radarPage, 480, 480);
  lv_obj_center(radarPage);
  lv_obj_set_style_bg_color(radarPage, COLOR_BACKGROUND, 0);
  lv_obj_set_style_bg_opa(radarPage, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(radarPage, 0, 0);
  lv_obj_set_style_pad_all(radarPage, 0, 0);
  lv_obj_set_style_radius(radarPage, 0, 0);
  lv_obj_clear_flag(radarPage, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(radarPage, LV_OBJ_FLAG_CLICKABLE);

  radarCanvas = lv_canvas_create(radarPage);
  lv_obj_center(radarCanvas);
  lv_obj_add_flag(radarCanvas, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(radarCanvas, LV_OBJ_FLAG_CLICKABLE);

  radarTitleLabel = makeLabel(radarPage, &clock_czech_16, COLOR_TEXT);
  lv_label_set_recolor(radarTitleLabel, true);
  lv_label_set_text(radarTitleLabel, "ČHMÚ - 50 km");
  lv_obj_set_style_bg_color(radarTitleLabel, COLOR_BACKGROUND, 0);
  lv_obj_set_style_bg_opa(radarTitleLabel, LV_OPA_80, 0);
  lv_obj_set_style_pad_hor(radarTitleLabel, 8, 0);
  lv_obj_set_style_pad_ver(radarTitleLabel, 4, 0);
  alignCenter(radarTitleLabel, 0, -205);

  radarProgressBar = lv_bar_create(radarPage);
  lv_obj_set_size(radarProgressBar, 220, 3);
  lv_obj_align(radarProgressBar, LV_ALIGN_CENTER, 0, -186);
  lv_bar_set_range(radarProgressBar, 0, 1);
  lv_bar_set_value(radarProgressBar, 0, LV_ANIM_OFF);
  lv_obj_set_style_radius(radarProgressBar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_radius(radarProgressBar, LV_RADIUS_CIRCLE,
                          LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(radarProgressBar, COLOR_DIVIDER, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(radarProgressBar, LV_OPA_70, LV_PART_MAIN);
  lv_obj_set_style_bg_color(radarProgressBar, COLOR_OUTSIDE,
                            LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(radarProgressBar, LV_OPA_70, LV_PART_INDICATOR);
  lv_obj_clear_flag(radarProgressBar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(radarProgressBar, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(radarProgressBar, LV_OBJ_FLAG_HIDDEN);

  radarStatusLabel = makeLabel(radarPage, &clock_czech_16, COLOR_OUTSIDE);
  lv_label_set_long_mode(radarStatusLabel, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(radarStatusLabel, 340);
  lv_obj_set_style_text_align(radarStatusLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(radarStatusLabel, englishLanguage()
                                          ? "LOADING CHMI RADAR..."
                                          : "Načítám radar ČHMÚ...");
  lv_obj_set_style_bg_color(radarStatusLabel, COLOR_BACKGROUND, 0);
  lv_obj_set_style_bg_opa(radarStatusLabel, LV_OPA_80, 0);
  lv_obj_set_style_pad_all(radarStatusLabel, 6, 0);
  alignCenter(radarStatusLabel, 0, 205);
  lv_obj_add_flag(radarStatusLabel, LV_OBJ_FLAG_HIDDEN);

  makeChildrenTapThrough(radarPage);
  lv_obj_add_flag(radarPage, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(radarPage, openSettingsEvent, LV_EVENT_LONG_PRESSED,
                      nullptr);
  lv_obj_add_flag(radarPage, LV_OBJ_FLAG_HIDDEN);
}

void closeSettings(bool saveChanges) {
  if (!settingsVisible) return;
  if (saveChanges) {
    savedDayBrightness =
        static_cast<uint8_t>(lv_slider_get_value(dayBrightnessSlider));
    savedNightBrightness =
        static_cast<uint8_t>(lv_slider_get_value(nightBrightnessSlider));
    automaticDayNightEnabled =
        lv_obj_has_state(automaticDayNightSwitch, LV_STATE_CHECKED);
    applySelectedSecondMode(lv_dropdown_get_selected(secondModeDropdown));
    applySelectedWeatherIconMode(
        lv_dropdown_get_selected(weatherIconModeDropdown));
    automaticFirmwareUpdateEnabled =
        lv_obj_has_state(automaticUpdateSwitch, LV_STATE_CHECKED);
    selectedWebMode = lv_dropdown_get_selected(webModeDropdown);
    if (settingsSaveCallback != nullptr) {
      settingsSaveCallback(savedDayBrightness, savedNightBrightness,
                           automaticDayNightEnabled, secondRingEnabled,
                           secondEffect, animatedWeatherIconsEnabled,
                           configuredWeatherIconStyle,
                           automaticFirmwareUpdateEnabled, selectedWebMode);
    }
    renderSecondRing(millis());
  }
  if (automaticDayNightEnabled && currentValues.sunStateAvailable) {
    const bool lightForcesDay = currentValues.dayNightLightStateAvailable &&
                                currentValues.dayNightLightOn;
    clockDashboardSetNightMode(!currentValues.weatherIsDay && !lightForcesDay);
  } else if (brightnessPreviewCallback != nullptr) {
    brightnessPreviewCallback(nightModeEnabled ? savedNightBrightness
                                               : savedDayBrightness);
  }
  settingsVisible = false;
  lv_obj_add_flag(settingsPage, LV_OBJ_FLAG_HIDDEN);
}

void cancelSettingsEvent(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_SHORT_CLICKED) closeSettings(false);
}

void saveSettingsEvent(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_SHORT_CLICKED) closeSettings(true);
}

void brightnessSliderEvent(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) return;
  lv_obj_t *slider = lv_event_get_target(event);
  const uint8_t brightness = static_cast<uint8_t>(lv_slider_get_value(slider));
  if (slider == dayBrightnessSlider) {
    updateBrightnessLabel(dayBrightnessValueLabel, brightness, 105, -72);
  } else {
    updateBrightnessLabel(nightBrightnessValueLabel, brightness, 105, 8);
  }
  if (brightnessPreviewCallback != nullptr) brightnessPreviewCallback(brightness);
}

void showSettingsSubpage(uint8_t page) {
  settingsPageIndex = constrain(page, static_cast<uint8_t>(0),
                                static_cast<uint8_t>(2));
  for (uint8_t index = 0; index < 3; ++index) {
    setObjectVisible(settingsContent[index], index == settingsPageIndex);
  }
  if (settingsPageNumberLabel != nullptr) {
    char pageNumber[2] = {static_cast<char>('1' + settingsPageIndex), '\0'};
    lv_label_set_text(settingsPageNumberLabel, pageNumber);
    alignCenter(settingsPageNumberLabel, 0, -184);
  }
  if (settingsPreviousButton != nullptr) {
    if (settingsPageIndex == 0)
      lv_obj_add_state(settingsPreviousButton, LV_STATE_DISABLED);
    else
      lv_obj_clear_state(settingsPreviousButton, LV_STATE_DISABLED);
  }
  if (settingsNextButton != nullptr) {
    if (settingsPageIndex == 2)
      lv_obj_add_state(settingsNextButton, LV_STATE_DISABLED);
    else
      lv_obj_clear_state(settingsNextButton, LV_STATE_DISABLED);
  }
}

void settingsPreviousEvent(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_RELEASED &&
      settingsPageIndex > 0)
    showSettingsSubpage(settingsPageIndex - 1);
}

void settingsNextEvent(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_RELEASED &&
      settingsPageIndex < 2)
    showSettingsSubpage(settingsPageIndex + 1);
}

lv_obj_t *makeSettingsSwitch(lv_obj_t *parent, const char *title, int y,
                             bool checked, lv_obj_t **titleLabel = nullptr) {
  lv_obj_t *label = makeLabel(parent, &clock_czech_16, COLOR_TEXT);
  if (titleLabel != nullptr) *titleLabel = label;
  lv_label_set_text(label, title);
  alignCenter(label, -55, y);
  lv_obj_t *control = lv_switch_create(parent);
  lv_obj_set_size(control, 54, 28);
  alignCenter(control, 132, y);
  lv_obj_set_style_bg_color(control, COLOR_DIVIDER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(control, COLOR_AIR,
                            LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(control, COLOR_TEXT, LV_PART_KNOB);
  if (checked) lv_obj_add_state(control, LV_STATE_CHECKED);
  return control;
}

lv_obj_t *makeSettingsDropdown(lv_obj_t *parent, const char *title,
                               const char *options, int y, uint8_t selected,
                               int labelX = -92, int controlX = 95,
                               int controlWidth = 180,
                               int labelYOffset = 0,
                               bool centerText = false,
                               lv_obj_t **titleLabel = nullptr) {
  lv_obj_t *label = makeLabel(parent, &clock_czech_16, COLOR_TEXT);
  if (titleLabel != nullptr) *titleLabel = label;
  lv_label_set_text(label, title);
  alignCenter(label, labelX, y + labelYOffset);
  lv_obj_t *control = lv_dropdown_create(parent);
  lv_obj_set_size(control, controlWidth, 42);
  alignCenter(control, controlX, y);
  lv_dropdown_set_options(control, options);
  lv_dropdown_set_symbol(control, centerText ? nullptr : "v");
  lv_dropdown_set_selected(control, selected);
  lv_obj_set_style_bg_color(control, COLOR_DIVIDER, 0);
  lv_obj_set_style_text_color(control, COLOR_TEXT, 0);
  lv_obj_set_style_text_font(control, &clock_czech_16, 0);
  lv_obj_set_style_text_font(lv_dropdown_get_list(control), &clock_czech_16, 0);
  if (centerText) {
    lv_obj_set_style_text_align(control, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(lv_dropdown_get_list(control),
                                LV_TEXT_ALIGN_CENTER, 0);
  }
  return control;
}

void firmwareCheckEvent(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_SHORT_CLICKED &&
      firmwareCheckCallback != nullptr) firmwareCheckCallback();
}

void firmwareInstallEvent(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_SHORT_CLICKED &&
      firmwareInstallCallback != nullptr) firmwareInstallCallback();
}

void createSettingsPage(lv_obj_t *screen) {
  settingsPage = lv_obj_create(screen);
  lv_obj_set_size(settingsPage, 480, 480);
  lv_obj_center(settingsPage);
  lv_obj_set_style_bg_color(settingsPage, COLOR_BACKGROUND, 0);
  lv_obj_set_style_bg_opa(settingsPage, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(settingsPage, 0, 0);
  lv_obj_set_style_pad_all(settingsPage, 0, 0);
  lv_obj_set_style_radius(settingsPage, 0, 0);
  lv_obj_clear_flag(settingsPage, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *settingsRing = lv_obj_create(settingsPage);
  lv_obj_set_size(settingsRing, 474, 474);
  lv_obj_center(settingsRing);
  lv_obj_set_style_radius(settingsRing, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(settingsRing, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(settingsRing, 3, 0);
  lv_obj_set_style_border_color(settingsRing, COLOR_DIVIDER, 0);
  lv_obj_clear_flag(settingsRing, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(settingsRing, LV_OBJ_FLAG_CLICKABLE);

  settingsPreviousButton = lv_btn_create(settingsPage);
  lv_obj_set_size(settingsPreviousButton, 58, 50);
  alignCenter(settingsPreviousButton, -58, -184);
  lv_obj_set_style_bg_color(settingsPreviousButton, COLOR_ROOM, 0);
  lv_obj_set_style_bg_opa(settingsPreviousButton, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(settingsPreviousButton, 25, 0);
  lv_obj_set_style_shadow_width(settingsPreviousButton, 0, 0);
  lv_obj_set_style_border_width(settingsPreviousButton, 0, 0);
  lv_obj_add_event_cb(settingsPreviousButton, settingsPreviousEvent,
                      LV_EVENT_RELEASED, nullptr);
  lv_obj_t *previousLabel =
      makeLabel(settingsPreviousButton, &lv_font_montserrat_28, COLOR_BACKGROUND);
  lv_label_set_text(previousLabel, LV_SYMBOL_LEFT);
  lv_obj_center(previousLabel);
  lv_obj_clear_flag(previousLabel, LV_OBJ_FLAG_CLICKABLE);

  settingsNextButton = lv_btn_create(settingsPage);
  lv_obj_set_size(settingsNextButton, 58, 50);
  alignCenter(settingsNextButton, 58, -184);
  lv_obj_set_style_bg_color(settingsNextButton, COLOR_ROOM, 0);
  lv_obj_set_style_bg_opa(settingsNextButton, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(settingsNextButton, 25, 0);
  lv_obj_set_style_shadow_width(settingsNextButton, 0, 0);
  lv_obj_set_style_border_width(settingsNextButton, 0, 0);
  lv_obj_add_event_cb(settingsNextButton, settingsNextEvent,
                      LV_EVENT_RELEASED, nullptr);
  lv_obj_t *nextLabel =
      makeLabel(settingsNextButton, &lv_font_montserrat_28, COLOR_BACKGROUND);
  lv_label_set_text(nextLabel, LV_SYMBOL_RIGHT);
  lv_obj_center(nextLabel);
  lv_obj_clear_flag(nextLabel, LV_OBJ_FLAG_CLICKABLE);

  settingsPageNumberLabel =
      makeLabel(settingsPage, &lv_font_montserrat_28, COLOR_TEXT);
  lv_label_set_text(settingsPageNumberLabel, "1");
  alignCenter(settingsPageNumberLabel, 0, -184);

  for (lv_obj_t *&content : settingsContent) {
    content = lv_obj_create(settingsPage);
    lv_obj_set_size(content, 430, 315);
    alignCenter(content, 0, -5);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
  }

  dayBrightnessTitleLabel =
      makeLabel(settingsContent[0], &clock_czech_16, COLOR_ROOM);
  lv_label_set_text(dayBrightnessTitleLabel, "DENNÍ JAS");
  alignCenter(dayBrightnessTitleLabel, -55, -72);

  dayBrightnessValueLabel =
      makeLabel(settingsContent[0], &lv_font_montserrat_28, COLOR_TEXT);
  updateBrightnessLabel(dayBrightnessValueLabel, savedDayBrightness, 105, -72);

  dayBrightnessSlider = lv_slider_create(settingsContent[0]);
  lv_obj_set_size(dayBrightnessSlider, 330, 20);
  alignCenter(dayBrightnessSlider, 0, -40);
  lv_slider_set_range(dayBrightnessSlider, 1, 100);
  lv_slider_set_value(dayBrightnessSlider, savedDayBrightness, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(dayBrightnessSlider, COLOR_DIVIDER, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(dayBrightnessSlider, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(dayBrightnessSlider, COLOR_ROOM,
                            LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(dayBrightnessSlider, LV_OPA_COVER,
                          LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(dayBrightnessSlider, COLOR_TEXT, LV_PART_KNOB);
  lv_obj_set_style_pad_all(dayBrightnessSlider, 5, LV_PART_KNOB);
  lv_obj_add_event_cb(dayBrightnessSlider, brightnessSliderEvent, LV_EVENT_ALL,
                      nullptr);

  nightBrightnessTitleLabel =
      makeLabel(settingsContent[0], &clock_czech_16, COLOR_OUTSIDE);
  lv_label_set_text(nightBrightnessTitleLabel, "NOČNÍ JAS");
  alignCenter(nightBrightnessTitleLabel, -55, 8);

  nightBrightnessValueLabel =
      makeLabel(settingsContent[0], &lv_font_montserrat_28, COLOR_TEXT);
  updateBrightnessLabel(nightBrightnessValueLabel, savedNightBrightness, 105, 8);

  nightBrightnessSlider = lv_slider_create(settingsContent[0]);
  lv_obj_set_size(nightBrightnessSlider, 330, 20);
  alignCenter(nightBrightnessSlider, 0, 40);
  lv_slider_set_range(nightBrightnessSlider, 1, 100);
  lv_slider_set_value(nightBrightnessSlider, savedNightBrightness, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(nightBrightnessSlider, COLOR_DIVIDER, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(nightBrightnessSlider, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(nightBrightnessSlider, COLOR_OUTSIDE,
                            LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(nightBrightnessSlider, LV_OPA_COVER,
                          LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(nightBrightnessSlider, COLOR_TEXT, LV_PART_KNOB);
  lv_obj_set_style_pad_all(nightBrightnessSlider, 5, LV_PART_KNOB);
  lv_obj_add_event_cb(nightBrightnessSlider, brightnessSliderEvent, LV_EVENT_ALL,
                      nullptr);

  automaticDayNightSwitch = makeSettingsSwitch(
      settingsContent[0], "AUTOMATICKY DEN/NOC", 92, automaticDayNightEnabled,
      &automaticDayNightTitleLabel);

  weatherIconModeDropdown = makeSettingsDropdown(
      settingsContent[1], "IKONY POČASÍ",
      "STATICKÉ MONOCHROMATICKÉ\nANIMOVANÉ FLAT\nANIMOVANÉ LINE\nANIMOVANÉ MONOCHROMATICKÉ",
      -54, selectedWeatherIconMode(), 0, 0, 360, -40, true,
      &weatherIconModeTitleLabel);
  secondModeDropdown = makeSettingsDropdown(
      settingsContent[1], "VTEŘINY", "VYPNUTO\nTEČKY\nLINKA\nKOMETA", 50,
      selectedSecondMode(), 0, 0, 360, -40, true, &secondModeTitleLabel);

  wifiAddressLabel = makeLabel(settingsContent[2], &lv_font_montserrat_16, COLOR_MUTED);
  lv_label_set_text(wifiAddressLabel, "IP: —");
  alignCenter(wifiAddressLabel, 0, -112);
  firmwareVersionLabel = makeLabel(settingsContent[2], &lv_font_montserrat_16, COLOR_MUTED);
  lv_label_set_text(firmwareVersionLabel, "FIRMWARE: —");
  alignCenter(firmwareVersionLabel, 0, -88);
  deviceInfoLabel = makeLabel(settingsContent[2], &clock_czech_16, COLOR_MUTED);
  lv_label_set_text(deviceInfoLabel, "");
  alignCenter(deviceInfoLabel, 0, -64);
  webModeDropdown = makeSettingsDropdown(
      settingsContent[2], "WEB", "10 MINUT\nVŽDY\nVYPNUTÝ", -24,
      selectedWebMode, -92, 95, 180, 0, false, &webModeTitleLabel);
  automaticUpdateSwitch = makeSettingsSwitch(
      settingsContent[2], "AUTOMATICKÉ OTA", 30,
      automaticFirmwareUpdateEnabled, &automaticUpdateTitleLabel);

  firmwareCheckButton = lv_btn_create(settingsContent[2]);
  lv_obj_set_size(firmwareCheckButton, 190, 42);
  alignCenter(firmwareCheckButton, 0, 88);
  lv_obj_set_style_radius(firmwareCheckButton, 21, 0);
  lv_obj_set_style_bg_color(firmwareCheckButton, COLOR_HUMIDITY, 0);
  lv_obj_add_event_cb(firmwareCheckButton, firmwareCheckEvent, LV_EVENT_SHORT_CLICKED, nullptr);
  firmwareCheckLabel =
      makeLabel(firmwareCheckButton, &clock_czech_16, COLOR_TEXT);
  lv_label_set_text(firmwareCheckLabel, "ZKONTROLOVAT");
  lv_obj_center(firmwareCheckLabel);

  firmwareInstallButton = lv_btn_create(settingsContent[2]);
  lv_obj_set_size(firmwareInstallButton, 190, 42);
  alignCenter(firmwareInstallButton, 0, 88);
  lv_obj_set_style_radius(firmwareInstallButton, 21, 0);
  lv_obj_set_style_bg_color(firmwareInstallButton, COLOR_AIR, 0);
  lv_obj_add_event_cb(firmwareInstallButton, firmwareInstallEvent,
                      LV_EVENT_SHORT_CLICKED, nullptr);
  firmwareInstallLabel =
      makeLabel(firmwareInstallButton, &clock_czech_16, COLOR_BACKGROUND);
  lv_label_set_text(firmwareInstallLabel, "AKTUALIZOVAT");
  lv_obj_center(firmwareInstallLabel);
  lv_obj_add_flag(firmwareInstallButton, LV_OBJ_FLAG_HIDDEN);
  firmwareStatusLabel = makeLabel(settingsContent[2], &clock_czech_16, COLOR_MUTED);
  lv_obj_set_width(firmwareStatusLabel, 360);
  lv_obj_set_style_text_align(firmwareStatusLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(firmwareStatusLabel, "");
  alignCenter(firmwareStatusLabel, 0, 123);

  lv_obj_t *cancelButton = lv_btn_create(settingsPage);
  lv_obj_set_size(cancelButton, 64, 64);
  alignCenter(cancelButton, 50, 180);
  lv_obj_set_style_radius(cancelButton, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(cancelButton, COLOR_ERROR, 0);
  lv_obj_add_event_cb(cancelButton, cancelSettingsEvent, LV_EVENT_SHORT_CLICKED, nullptr);
  lv_obj_t *cancelLabel = makeLabel(cancelButton, &lv_font_montserrat_28, COLOR_TEXT);
  lv_label_set_text(cancelLabel, LV_SYMBOL_CLOSE);
  lv_obj_center(cancelLabel);

  lv_obj_t *saveButton = lv_btn_create(settingsPage);
  lv_obj_set_size(saveButton, 64, 64);
  alignCenter(saveButton, -50, 180);
  lv_obj_set_style_radius(saveButton, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(saveButton, COLOR_AIR, 0);
  lv_obj_add_event_cb(saveButton, saveSettingsEvent, LV_EVENT_SHORT_CLICKED, nullptr);
  lv_obj_t *saveLabel = makeLabel(saveButton, &lv_font_montserrat_28, COLOR_BACKGROUND);
  lv_label_set_text(saveLabel, LV_SYMBOL_OK);
  lv_obj_center(saveLabel);

  showSettingsSubpage(0);

  lv_obj_add_flag(settingsPage, LV_OBJ_FLAG_HIDDEN);
}
}  // namespace

void clockDashboardInit(const ClockValues &values, uint8_t dayBrightness,
                        uint8_t nightBrightness, bool automaticDayNight,
                        BrightnessPreviewCallback brightnessPreview,
                        SettingsOpenCallback settingsOpen,
                        SettingsSaveCallback settingsSave,
                        SettingsActionCallback firmwareCheck,
                        SettingsActionCallback firmwareInstall,
                        RadarVisibilityCallback radarVisibility,
                        RadarRangeCallback radarRange) {
  savedDayBrightness = constrain(dayBrightness, 1, 100);
  savedNightBrightness = constrain(nightBrightness, 1, 100);
  automaticDayNightEnabled = automaticDayNight;
  brightnessPreviewCallback = brightnessPreview;
  settingsOpenCallback = settingsOpen;
  settingsSaveCallback = settingsSave;
  firmwareCheckCallback = firmwareCheck;
  firmwareInstallCallback = firmwareInstall;
  radarVisibilityCallback = radarVisibility;
  radarRangeCallback = radarRange;
  lv_obj_t *screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, COLOR_BACKGROUND, 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

  makeSecondRing(screen);

  dashboardContent = lv_obj_create(screen);
  lv_obj_set_size(dashboardContent, 480, 480);
  lv_obj_set_pos(dashboardContent, 0, -10);
  lv_obj_set_style_bg_opa(dashboardContent, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(dashboardContent, 0, 0);
  lv_obj_set_style_pad_all(dashboardContent, 0, 0);
  lv_obj_set_style_radius(dashboardContent, 0, 0);
  lv_obj_clear_flag(dashboardContent, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(dashboardContent, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t *content = dashboardContent;

  timeLabel = makeLabel(content, &lv_font_montserrat_48, COLOR_TEXT);
  lv_label_set_recolor(timeLabel, true);
  lv_label_set_text(timeLabel, "--:--");
  alignCenter(timeLabel, 0, -105);

  dateLabel = makeLabel(content, &clock_czech_18, COLOR_MUTED);
  lv_obj_set_style_text_letter_space(dateLabel, 4, 0);
  lv_label_set_text(dateLabel, "");
  alignCenter(dateLabel, 0, -43);

  outsideTitleLabel = makeLabel(content, &clock_czech_18, COLOR_OUTSIDE);
  lv_obj_set_style_text_letter_space(outsideTitleLabel, 2, 0);
  lv_label_set_text(outsideTitleLabel, "VENKU");
  alignCenter(outsideTitleLabel, -122, 5);

  roomTitleLabel = makeLabel(content, &clock_czech_18, COLOR_ROOM);
  lv_obj_set_style_text_letter_space(roomTitleLabel, 2, 0);
  lv_label_set_text(roomTitleLabel, "MÍSTNOST");
  alignCenter(roomTitleLabel, 127, 5);

  outsideIntegerLabel = makeLabel(content, &lv_font_montserrat_48, COLOR_OUTSIDE);
  outsideDecimalLabel = makeLabel(content, &lv_font_montserrat_32, COLOR_OUTSIDE);
  outsideUnitLabel = makeLabel(content, &lv_font_montserrat_24, COLOR_OUTSIDE);
  lv_label_set_text(outsideUnitLabel, "°C");

  roomIntegerLabel = makeLabel(content, &lv_font_montserrat_48, COLOR_ROOM);
  roomDecimalLabel = makeLabel(content, &lv_font_montserrat_32, COLOR_ROOM);
  roomUnitLabel = makeLabel(content, &lv_font_montserrat_24, COLOR_ROOM);
  lv_label_set_text(roomUnitLabel, "°C");

  weatherImage = lv_img_create(content);
  alignCenter(weatherImage, -142, 107);

  roomWeatherImage = lv_img_create(content);
  alignCenter(roomWeatherImage, 142, 107);
  lv_obj_add_flag(roomWeatherImage, LV_OBJ_FLAG_HIDDEN);

  weatherAnimation = lv_gif_create(content);
  alignCenter(weatherAnimation, -142, 107);
  lv_obj_add_flag(weatherAnimation, LV_OBJ_FLAG_HIDDEN);

  roomWeatherAnimation = lv_gif_create(content);
  alignCenter(roomWeatherAnimation, 142, 107);
  lv_obj_add_flag(roomWeatherAnimation, LV_OBJ_FLAG_HIDDEN);

  outsideIconLabel = makeLabel(content, &clock_icons_42, COLOR_OUTSIDE);
  lv_label_set_text(outsideIconLabel, "");
  alignCenter(outsideIconLabel, -142, 107);
  lv_obj_add_flag(outsideIconLabel, LV_OBJ_FLAG_HIDDEN);

  roomIconLabel = makeLabel(content, &clock_icons_42, COLOR_ROOM);
  lv_label_set_text(roomIconLabel, "\xEF\x80\x95");  // home (U+F015)
  alignCenter(roomIconLabel, 142, 107);

  // Středový oblouk: dvě svislé nohy, horní půlkruh a krátký dřík.
  lv_obj_t *airArc = lv_arc_create(content);
  lv_obj_set_size(airArc, 196, 196);
  lv_arc_set_bg_angles(airArc, 180, 360);
  lv_arc_set_range(airArc, 0, 100);
  lv_arc_set_value(airArc, 0);
  lv_obj_remove_style(airArc, nullptr, LV_PART_KNOB);
  lv_obj_remove_style(airArc, nullptr, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(airArc, 3, LV_PART_MAIN);
  lv_obj_set_style_arc_color(airArc, COLOR_DIVIDER, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(airArc, false, LV_PART_MAIN);
  alignCenter(airArc, 0, 142);
  lv_obj_clear_flag(airArc, LV_OBJ_FLAG_CLICKABLE);

  makeDivider(content, 3, 34, 0, 28);
  makeDivider(content, 3, 34, -98, 158);
  makeDivider(content, 3, 34, 98, 158);

  co2TitleLabel = makeLabel(content, &clock_czech_16, COLOR_AIR);
  lv_label_set_text(co2TitleLabel, "CO₂");
  alignCenter(co2TitleLabel, 0, 69);

  co2ValueLabel = makeLabel(content, &lv_font_montserrat_32, COLOR_AIR);

  co2UnitLabel = makeLabel(content, &clock_czech_16, COLOR_AIR);
  lv_label_set_text(co2UnitLabel, "ppm");

  makeDivider(content, 152, 2, 0, 121);

  humidityTitleLabel =
      makeLabel(content, &clock_czech_16, COLOR_HUMIDITY);
  lv_label_set_text(humidityTitleLabel, "VLHKOST");
  alignCenter(humidityTitleLabel, -43, 151);

  humidityValueLabel = makeLabel(content, &lv_font_montserrat_36, COLOR_HUMIDITY);
  alignCenter(humidityValueLabel, 34, 151);

  humidityUnitLabel =
      makeLabel(content, &clock_czech_20, COLOR_HUMIDITY);
  lv_label_set_text(humidityUnitLabel, "%");
  alignCenter(humidityUnitLabel, 73, 154);

  makeDivider(content, 286, 2, 0, 174);

  wifiStatusLabel = makeLabel(content, &lv_font_montserrat_16, COLOR_ERROR);
  lv_label_set_text(wifiStatusLabel, LV_SYMBOL_WIFI);

  statusLabel = makeLabel(content, &lv_font_montserrat_16, COLOR_ERROR);
  lv_label_set_text(statusLabel, LV_SYMBOL_HOME);

  webStatusLabel = makeLabel(content, &lv_font_montserrat_16, COLOR_OUTSIDE);
  lv_label_set_text(webStatusLabel, LV_SYMBOL_SETTINGS);
  lv_obj_add_flag(webStatusLabel, LV_OBJ_FLAG_HIDDEN);
  alignConnectionStatusIcons();

  clockDashboardUpdate(values);
  makeChildrenTapThrough(dashboardContent);
  lv_obj_add_event_cb(dashboardContent, openSettingsEvent, LV_EVENT_LONG_PRESSED,
                      nullptr);
  createRadarPage(screen);
  createSettingsPage(screen);

  firmwareUpdateOverlay = lv_obj_create(screen);
  lv_obj_set_size(firmwareUpdateOverlay, 480, 480);
  lv_obj_set_pos(firmwareUpdateOverlay, 0, 0);
  lv_obj_set_style_bg_color(firmwareUpdateOverlay, COLOR_BACKGROUND, 0);
  lv_obj_set_style_bg_opa(firmwareUpdateOverlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(firmwareUpdateOverlay, 0, 0);
  lv_obj_set_style_radius(firmwareUpdateOverlay, 0, 0);
  lv_obj_clear_flag(firmwareUpdateOverlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(firmwareUpdateOverlay, LV_OBJ_FLAG_HIDDEN);
  firmwareUpdateTitleLabel =
      makeLabel(firmwareUpdateOverlay, &lv_font_montserrat_32, COLOR_TEXT);
  lv_obj_set_width(firmwareUpdateTitleLabel, 460);
  lv_obj_set_style_text_align(firmwareUpdateTitleLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(firmwareUpdateTitleLabel, "AKTUALIZACE FIRMWARE");
  alignCenter(firmwareUpdateTitleLabel, 0, -38);
  firmwareUpdateCountdownLabel =
      makeLabel(firmwareUpdateOverlay, &lv_font_montserrat_48, COLOR_TEXT);
  lv_label_set_text(firmwareUpdateCountdownLabel, "5");
  alignCenter(firmwareUpdateCountdownLabel, 0, 35);
}

void clockDashboardApplyConfiguration(const ClockConfig &config) {
  radarFeatureAvailable = clockConfigRadarAvailable(config);
  if (!radarFeatureAvailable && radarVisible) {
    radarVisible = false;
    lv_obj_add_flag(radarPage, LV_OBJ_FLAG_HIDDEN);
    if (!settingsVisible && !firmwareUpdateActive) {
      lv_obj_clear_flag(dashboardContent, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(dashboardContent);
    }
    if (radarVisibilityCallback != nullptr) radarVisibilityCallback(false);
  }
  language = constrain(config.language,
                       static_cast<uint8_t>(CLOCK_LANGUAGE_UNSET),
                       static_cast<uint8_t>(CLOCK_LANGUAGE_ENGLISH));
  applyDashboardLanguage();
  const bool nightVisualChanged = nightVisualMode != config.nightVisualMode;
  nightVisualMode = config.nightVisualMode;
  metricAConfig = config.metricA;
  metricBConfig = config.metricB;
  normalizeMicroSign(metricAConfig.suffix);
  normalizeMicroSign(metricBConfig.suffix);
  metricAColorScale = config.metricAColorScale;
  metricBColorScale = config.metricBColorScale;
  const bool openMeteo = config.dataSource == CLOCK_DATA_SOURCE_OPEN_METEO;
  homeAssistantStatusRelevant = !openMeteo;
  const auto openMeteoUnit = [](const char *value) -> const char * {
    if (strcmp(value, "temperature_2m") == 0 ||
        strcmp(value, "apparent_temperature") == 0)
      return "°C";
    if (strcmp(value, "relative_humidity_2m") == 0 ||
        strcmp(value, "cloud_cover") == 0)
      return "%";
    if (strcmp(value, "pressure_msl") == 0 ||
        strcmp(value, "surface_pressure") == 0)
      return "hPa";
    if (strcmp(value, "wind_speed_10m") == 0 ||
        strcmp(value, "wind_gusts_10m") == 0)
      return "km/h";
    if (strcmp(value, "wind_direction_10m") == 0) return "°";
    if (strcmp(value, "snowfall") == 0) return "cm";
    if (strcmp(value, "precipitation") == 0 || strcmp(value, "rain") == 0 ||
        strcmp(value, "showers") == 0)
      return "mm";
    return "";
  };
  const auto openMeteoDecimals = [](const char *value) -> uint8_t {
    return strcmp(value, "relative_humidity_2m") == 0 ||
                   strcmp(value, "cloud_cover") == 0 ||
                   strcmp(value, "pressure_msl") == 0 ||
                   strcmp(value, "surface_pressure") == 0 ||
                   strcmp(value, "wind_direction_10m") == 0
               ? 0
               : 1;
  };
  if (openMeteo) {
    clockConfigCopy(metricAConfig.name, sizeof(metricAConfig.name),
                    config.openMeteoSlots[2].name);
    clockConfigCopy(metricAConfig.suffix, sizeof(metricAConfig.suffix),
                    openMeteoUnit(config.openMeteoSlots[2].value));
    metricAConfig.decimals = openMeteoDecimals(config.openMeteoSlots[2].value);
    clockConfigCopy(metricBConfig.name, sizeof(metricBConfig.name),
                    config.openMeteoSlots[3].name);
    clockConfigCopy(metricBConfig.suffix, sizeof(metricBConfig.suffix),
                    openMeteoUnit(config.openMeteoSlots[3].value));
    metricBConfig.decimals = openMeteoDecimals(config.openMeteoSlots[3].value);
    metricAColorScale = ClockMetricColorScale{};
    metricAColorScale.points[0].color = config.openMeteoSlots[2].color;
    metricBColorScale = ClockMetricColorScale{};
    metricBColorScale.points[0].color = config.openMeteoSlots[3].color;
    strlcpy(outsideUnit, openMeteoUnit(config.openMeteoSlots[0].value),
            sizeof(outsideUnit));
    strlcpy(roomUnit, openMeteoUnit(config.openMeteoSlots[1].value),
            sizeof(roomUnit));
    outsideDecimals = openMeteoDecimals(config.openMeteoSlots[0].value);
    roomDecimals = openMeteoDecimals(config.openMeteoSlots[1].value);
  } else {
    strlcpy(outsideUnit, "°C", sizeof(outsideUnit));
    strlcpy(roomUnit, "°C", sizeof(roomUnit));
    outsideDecimals = 1;
    roomDecimals = 1;
  }
  automaticDayNightEnabled = config.automaticDayNight;
  const bool timeColonModeChanged =
      timeColonEffect != config.timeColonEffect;
  timeColonEffect = constrain(
      config.timeColonEffect, static_cast<uint8_t>(CLOCK_TIME_COLON_STEADY),
      static_cast<uint8_t>(CLOCK_TIME_COLON_FADE));
  secondRingEnabled = config.secondRingEnabled;
  secondEffect = constrain(
      config.secondEffect, static_cast<uint8_t>(CLOCK_SECOND_EFFECT_DOTS),
      static_cast<uint8_t>(CLOCK_SECOND_EFFECT_COMET));
  secondRingBackgroundColor = config.secondRingBackgroundColor & 0xFFFFFF;
  secondRingBackgroundBrightness = config.secondRingBackgroundBrightness;
  secondRingBackgroundDotSize =
      constrain(config.secondRingBackgroundDotSize, 1, 10);
  secondDotSize = constrain(config.secondDotSize, 1, 10);
  secondDotColor = config.secondDotColor & 0xFFFFFF;
  secondDotBrightness = config.secondDotBrightness;
  outsideColor = (openMeteo ? config.openMeteoSlots[0].color
                            : config.leftSide.color) & 0xFFFFFF;
  roomColor = (openMeteo ? config.openMeteoSlots[1].color
                         : config.rightSide.color) & 0xFFFFFF;
  timeColor = config.timeColor & 0xFFFFFF;
  dateColor = config.dateColor & 0xFFFFFF;
  timeFont = constrain(config.timeFont,
                       static_cast<uint8_t>(CLOCK_TIME_FONT_BARLOW),
                       static_cast<uint8_t>(CLOCK_TIME_FONT_DOTO));
  lv_obj_set_style_text_font(timeLabel, configuredTimeFont(), 0);
  if (timeColonModeChanged) renderTimeColon(millis(), true);
  alignCenter(timeLabel, 0, -105);
  leftWeatherIconColor =
      (openMeteo ? config.openMeteoSlots[0].color
                 : config.leftWeatherIconColor) &
      0xFFFFFF;
  rightWeatherIconColor = config.rightWeatherIconColor & 0xFFFFFF;
  animatedWeatherIconsEnabled = config.animatedWeatherIcons;
  automaticFirmwareUpdateEnabled = config.automaticFirmwareUpdate;
  configuredWeatherIconStyle = constrain(
      config.weatherIconStyle,
      static_cast<uint8_t>(CLOCK_WEATHER_ICON_STYLE_MONOCHROME),
      static_cast<uint8_t>(CLOCK_WEATHER_ICON_STYLE_LINE));
  outsideUsesWeatherIcon = openMeteo || strcmp(config.leftSide.icon, "weather") == 0;
  roomUsesWeatherIcon = !openMeteo && strcmp(config.rightSide.icon, "weather") == 0;
  weatherConfigured = openMeteo || config.weatherEntityId[0] != '\0';
  outsideConfigured = openMeteo || config.leftSide.temperatureEntityId[0] != '\0';
  roomConfigured = openMeteo || config.rightSide.temperatureEntityId[0] != '\0';
  metricAConfigured = openMeteo || config.metricA.entityId[0] != '\0';
  metricBConfigured = openMeteo || config.metricB.entityId[0] != '\0';
  ensureWeatherAnimationDecoders();
  savedDayBrightness = constrain(config.dayBrightness, 1, 100);
  savedNightBrightness = constrain(config.nightBrightness, 1, 100);
  if (brightnessPreviewCallback != nullptr) {
    brightnessPreviewCallback(nightModeEnabled ? savedNightBrightness
                                               : savedDayBrightness);
  }
  renderSecondRing(millis());
  if (nightVisualChanged) applyDashboardColors();
  lv_label_set_text(outsideTitleLabel, openMeteo
                                           ? config.openMeteoSlots[0].name
                                           : config.leftSide.name[0] == '\0'
                                           ? (englishLanguage() ? "ROOM" : "MÍSTNOST")
                                           : config.leftSide.name);
  alignCenter(outsideTitleLabel, -122, 5);
  setObjectVisible(outsideTitleLabel, outsideConfigured);
  setObjectVisible(outsideIntegerLabel, outsideConfigured);
  setObjectVisible(outsideDecimalLabel, outsideConfigured);
  setObjectVisible(outsideUnitLabel, outsideConfigured);
  lv_label_set_text(roomTitleLabel, openMeteo
                                        ? config.openMeteoSlots[1].name
                                        : config.rightSide.name[0] == '\0'
                                        ? (englishLanguage() ? "ROOM" : "MÍSTNOST")
                                        : config.rightSide.name);
  alignCenter(roomTitleLabel, 127, 5);
  setObjectVisible(roomTitleLabel, roomConfigured);
  setObjectVisible(roomIntegerLabel, roomConfigured);
  setObjectVisible(roomDecimalLabel, roomConfigured);
  setObjectVisible(roomUnitLabel, roomConfigured);
  lv_label_set_text(outsideUnitLabel, outsideUnit);
  lv_label_set_text(roomUnitLabel, roomUnit);
  const char *outsideIconGlyph = roomIconGlyph(openMeteo ? "weather" : config.leftSide.icon);
  lv_label_set_text(outsideIconLabel, outsideIconGlyph);
  if (!outsideConfigured || outsideIconGlyph[0] == '\0') {
    lv_obj_add_flag(outsideIconLabel, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(outsideIconLabel, LV_OBJ_FLAG_HIDDEN);
    alignCenter(outsideIconLabel, -142, 107);
  }
  const char *roomIconGlyphValue = roomIconGlyph(openMeteo ? "none" : config.rightSide.icon);
  lv_label_set_text(roomIconLabel, roomIconGlyphValue);
  if (!roomConfigured || roomIconGlyphValue[0] == '\0') {
    lv_obj_add_flag(roomIconLabel, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(roomIconLabel, LV_OBJ_FLAG_HIDDEN);
    alignCenter(roomIconLabel, 142, 107);
  }
  lv_label_set_text(co2TitleLabel, metricAConfig.name);
  lv_label_set_text(co2UnitLabel, metricAConfig.suffix);
  lv_label_set_text(humidityTitleLabel, metricBConfig.name);
  lv_label_set_text(humidityUnitLabel, metricBConfig.suffix);
  setObjectVisible(co2TitleLabel, metricAConfigured);
  setObjectVisible(co2ValueLabel, metricAConfigured);
  setObjectVisible(co2UnitLabel, metricAConfigured);
  setObjectVisible(humidityTitleLabel, metricBConfigured);
  setObjectVisible(humidityValueLabel, metricBConfigured);
  setObjectVisible(humidityUnitLabel, metricBConfigured);
  clockDashboardUpdate(currentValues);
}

void clockDashboardUpdate(const ClockValues &values) {
  char text[32];
  currentValues = values;
  if (firmwareUpdateActive) return;

  const lv_img_dsc_t *weatherIcon =
      openWeatherIconForCode(values.weatherCode, values.weatherIsDay);
  char desiredAnimationKey[48] = "";
  const uint8_t effectiveWeatherIconStyle =
      redNightVisualEnabled() ? CLOCK_WEATHER_ICON_STYLE_MONOCHROME
                              : configuredWeatherIconStyle;
  const bool hasDesiredAnimation = weatherAnimationAssetKey(
      desiredAnimationKey, sizeof(desiredAnimationKey), values.weatherCode,
      values.weatherIsDay, effectiveWeatherIconStyle);
  const bool useAnimation = animatedWeatherIconsEnabled &&
                            weatherAnimationAvailable &&
                            !weatherAnimationRevealPending &&
                            hasDesiredAnimation &&
                            strcmp(desiredAnimationKey, weatherAnimationKey) == 0;
  if (!outsideConfigured || !weatherConfigured || !outsideUsesWeatherIcon ||
      weatherIcon == nullptr || useAnimation) {
    lv_obj_add_flag(weatherImage, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_img_set_src(weatherImage, weatherIcon);
    lv_obj_clear_flag(weatherImage, LV_OBJ_FLAG_HIDDEN);
  }

  if (!roomConfigured || !weatherConfigured || !roomUsesWeatherIcon ||
      weatherIcon == nullptr || useAnimation) {
    lv_obj_add_flag(roomWeatherImage, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_img_set_src(roomWeatherImage, weatherIcon);
    lv_obj_clear_flag(roomWeatherImage, LV_OBJ_FLAG_HIDDEN);
  }

  if (outsideConfigured && weatherConfigured && outsideUsesWeatherIcon &&
      useAnimation) {
    lv_obj_clear_flag(weatherAnimation, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(weatherAnimation, LV_OBJ_FLAG_HIDDEN);
  }
  if (roomConfigured && weatherConfigured && roomUsesWeatherIcon &&
      useAnimation) {
    lv_obj_clear_flag(roomWeatherAnimation, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(roomWeatherAnimation, LV_OBJ_FLAG_HIDDEN);
  }

  setTopValue(values.leftTemperatureC, outsideDecimals, 118,
              outsideIntegerLabel, outsideDecimalLabel, outsideUnitLabel);
  setTopValue(values.rightTemperatureC, roomDecimals, 367, roomIntegerLabel,
              roomDecimalLabel, roomUnitLabel);

  formatMetricValue(text, sizeof(text), values.metricAValue,
                    metricAConfig.decimals);
  lv_label_set_text(co2ValueLabel, text);
  alignCo2Value();
  const lv_color_t co2Color =
      metricColorForValue(values.metricAValue, metricAColorScale);
  lv_obj_set_style_text_color(co2TitleLabel, co2Color, 0);
  lv_obj_set_style_text_color(co2ValueLabel, co2Color, 0);
  lv_obj_set_style_text_color(co2UnitLabel, co2Color, 0);

  formatMetricValue(text, sizeof(text), values.metricBValue,
                    metricBConfig.decimals);
  lv_label_set_text(humidityValueLabel, text);
  alignMetricBValue();
  const lv_color_t metricBValueColor =
      metricColorForValue(values.metricBValue, metricBColorScale);
  lv_obj_set_style_text_color(humidityTitleLabel, metricBValueColor, 0);
  lv_obj_set_style_text_color(humidityValueLabel, metricBValueColor, 0);
  lv_obj_set_style_text_color(humidityUnitLabel, metricBValueColor, 0);

  const lv_color_t statusColor =
      values.homeAssistantOnline ? COLOR_AIR : COLOR_ERROR;
  lv_obj_set_style_text_color(statusLabel, statusColor, 0);
  if (automaticDayNightEnabled && currentValues.sunStateAvailable) {
    const bool lightForcesDay = currentValues.dayNightLightStateAvailable &&
                                currentValues.dayNightLightOn;
    clockDashboardSetNightMode(!currentValues.weatherIsDay && !lightForcesDay);
  } else {
    applyDashboardColors();
  }
}

void clockDashboardSetWeatherAnimation(const uint8_t *gifData, size_t size,
                                       const char *iconKey) {
  if (gifData == nullptr || size == 0 || iconKey == nullptr) return;
  weatherAnimationRevealPending = true;
  weatherAnimationRevealAt = millis() + WEATHER_ANIMATION_REVEAL_DELAY_MS;
  lv_obj_add_flag(weatherAnimation, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(roomWeatherAnimation, LV_OBJ_FLAG_HIDDEN);
  weatherAnimationSource = {};
  weatherAnimationSource.header.always_zero = 0;
  weatherAnimationSource.header.w = 84;
  weatherAnimationSource.header.h = 84;
  weatherAnimationSource.header.cf = LV_IMG_CF_RAW;
  weatherAnimationSource.data_size = size;
  weatherAnimationSource.data = gifData;
  weatherAnimationAvailable = true;
  strlcpy(weatherAnimationKey, iconKey, sizeof(weatherAnimationKey));
  ensureWeatherAnimationDecoders();
  applyDashboardColors();
  clockDashboardUpdate(currentValues);
}

void clockDashboardLoop() {
  if (firmwareUpdateActive) return;
  const unsigned long now = millis();
  if (settingsVisible && settingsPageIndex == 2 &&
      now - lastSettingsInfoRefreshAt >= 500) {
    lastSettingsInfoRefreshAt = now;
    char info[64];
    snprintf(info, sizeof(info),
             englishLanguage() ? "CPU: %u MHz   MEMORY: %lu kB"
                               : "CPU: %u MHz   PAMĚŤ: %lu kB",
             static_cast<unsigned>(getCpuFrequencyMhz()),
             static_cast<unsigned long>(ESP.getFreeHeap() / 1024));
    if (strcmp(displayedDeviceInfo, info) != 0) {
      strlcpy(displayedDeviceInfo, info, sizeof(displayedDeviceInfo));
      lv_label_set_text(deviceInfoLabel, displayedDeviceInfo);
      alignCenter(deviceInfoLabel, 0, -64);
    }
    const FirmwareUpdateSnapshot snapshot = firmwareUpdateServiceSnapshot();
    char statusText[160] = "";
    switch (snapshot.state) {
      case FirmwareUpdateState::Checking:
        strlcpy(statusText,
                englishLanguage() ? "CHECKING FOR UPDATE"
                                  : "KONTROLUJI AKTUALIZACI",
                sizeof(statusText));
        break;
      case FirmwareUpdateState::Available:
        snprintf(statusText, sizeof(statusText),
                 englishLanguage() ? "NEW VERSION  %s" : "NOVÁ VERZE  %s",
                 snapshot.serverVersion);
        break;
      case FirmwareUpdateState::Current:
        strlcpy(statusText,
                englishLanguage() ? "FIRMWARE IS UP TO DATE"
                                  : "FIRMWARE JE AKTUÁLNÍ",
                sizeof(statusText));
        break;
      case FirmwareUpdateState::Downloading:
        strlcpy(statusText,
                englishLanguage() ? "DOWNLOADING UPDATE"
                                  : "STAHUJI AKTUALIZACI",
                sizeof(statusText));
        break;
      case FirmwareUpdateState::Failed:
        strlcpy(statusText,
                englishLanguage() ? "UPDATE CHECK FAILED"
                                  : "KONTROLA SELHALA",
                sizeof(statusText));
        break;
      case FirmwareUpdateState::Restarting:
        strlcpy(statusText,
                englishLanguage() ? "RESTARTING DEVICE"
                                  : "RESTARTUJI ZAŘÍZENÍ",
                sizeof(statusText));
        break;
      default:
        strlcpy(statusText,
                snapshot.installationSupported
                    ? (englishLanguage() ? "UPDATE NOT CHECKED"
                                         : "AKTUALIZACE NEZKONTROLOVÁNA")
                    : (englishLanguage() ? "OTA IN RELEASE ONLY"
                                         : "OTA JEN V RELEASE"),
                sizeof(statusText));
        break;
    }
    if (strcmp(displayedFirmwareStatus, statusText) != 0) {
      strlcpy(displayedFirmwareStatus, statusText,
              sizeof(displayedFirmwareStatus));
      lv_label_set_text(firmwareStatusLabel, displayedFirmwareStatus);
      alignCenter(firmwareStatusLabel, 0, 123);
    }
    const bool canInstall = snapshot.updateAvailable &&
                            snapshot.installationSupported && !snapshot.busy;
    if (displayedCanInstall != canInstall) {
      displayedCanInstall = canInstall;
      setObjectVisible(firmwareInstallButton, canInstall);
      setObjectVisible(firmwareCheckButton, !canInstall);
    }
  }
  if (weatherAnimationRevealPending &&
      static_cast<long>(now - weatherAnimationRevealAt) >= 0) {
    weatherAnimationRevealPending = false;
    clockDashboardUpdate(currentValues);
  }
  const bool smoothSecondEffectActive =
      secondRingEnabled && (secondEffect == CLOCK_SECOND_EFFECT_LINE ||
                            secondEffect == CLOCK_SECOND_EFFECT_COMET);
  const bool smoothTimeColonActive =
      timeColonEffect == CLOCK_TIME_COLON_FADE;
  if (!secondFadeActive && !smoothSecondEffectActive &&
      !smoothTimeColonActive)
    return;
  const unsigned long frameInterval =
      SMOOTH_EFFECT_FRAME_MS;
  if (now - lastSecondFadeFrameAt < frameInterval) return;
  lastSecondFadeFrameAt = now;
  if (secondFadeActive || smoothSecondEffectActive) renderSecondRing(now);
  if (smoothTimeColonActive) renderTimeColon(now);
}

void clockDashboardShowSettings() {
  if (firmwareUpdateActive) return;
  showSettings();
}

void clockDashboardShowSettingsPage(uint8_t page) {
  if (firmwareUpdateActive) return;
  showSettings();
  showSettingsSubpage(page);
}

void clockDashboardSetNightMode(bool enabled) {
  const bool modeChanged = nightModeEnabled != enabled;
  nightModeEnabled = enabled;
  if (firmwareUpdateActive) return;
  applyDashboardColors();
  if (modeChanged) clockDashboardUpdate(currentValues);
  if (brightnessPreviewCallback != nullptr) {
    brightnessPreviewCallback(nightModeEnabled ? savedNightBrightness
                                               : savedDayBrightness);
  }
}

bool clockDashboardNightModeEnabled() { return nightModeEnabled; }

void clockDashboardHandleShortClick() {
  if (settingsVisible || firmwareUpdateActive || automaticDayNightEnabled)
    return;
  clockDashboardSetNightMode(!nightModeEnabled);
}

bool clockDashboardRadarVisible() { return radarVisible; }

void clockDashboardSetRadarVisible(bool visible) { setRadarVisible(visible); }

bool clockDashboardAutomaticRotationAllowed() {
  return !settingsVisible && !firmwareUpdateActive;
}

void clockDashboardSetRadarSnapshot(const uint16_t *pixels,
                                    const char *frameTime, uint16_t radiusKm,
                                    const char *message, bool loading,
                                    bool fullPreparationInProgress,
                                    bool latestFrame,
                                    uint8_t currentFrameNumber,
                                    uint8_t animationFrameCount,
                                    uint8_t pauseSeconds) {
  if (radarCanvas == nullptr || radarStatusLabel == nullptr ||
      radarTitleLabel == nullptr || radarProgressBar == nullptr)
    return;
  radarFullPreparationInProgress = fullPreparationInProgress;
  if (!redNightVisualEnabled()) {
    lv_obj_set_style_bg_color(
        radarProgressBar,
        fullPreparationInProgress ? COLOR_ERROR : COLOR_OUTSIDE,
        LV_PART_INDICATOR);
  }
  if (pixels != nullptr) {
    lv_canvas_set_buffer(radarCanvas, const_cast<uint16_t *>(pixels), 480, 480,
                         LV_IMG_CF_TRUE_COLOR);
    lv_obj_clear_flag(radarCanvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(radarCanvas);
  }
  if (pixels != nullptr && currentFrameNumber > 0 &&
      animationFrameCount > 1) {
    lv_bar_set_range(radarProgressBar, 0, animationFrameCount);
    if (latestFrame) {
      lv_bar_set_value(radarProgressBar, animationFrameCount, LV_ANIM_OFF);
      if (pauseSeconds > 0) {
        lv_obj_set_style_anim_time(
            radarProgressBar,
            static_cast<uint32_t>(pauseSeconds) * 1000UL, LV_PART_MAIN);
        lv_bar_set_value(radarProgressBar, 0, LV_ANIM_ON);
      } else {
        lv_bar_set_value(radarProgressBar, 0, LV_ANIM_OFF);
      }
    } else {
      lv_bar_set_value(radarProgressBar, currentFrameNumber, LV_ANIM_OFF);
    }
    lv_obj_clear_flag(radarProgressBar, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(radarProgressBar, LV_OBJ_FLAG_HIDDEN);
  }
  char title[96];
  const bool highlightLatestFrame = latestFrame && !redNightVisualEnabled();
  const char *timePrefix = highlightLatestFrame ? "#65FF45 " : "";
  const char *timeSuffix = highlightLatestFrame ? "#" : "";
  if (radiusKm == 0 && frameTime != nullptr && frameTime[0] != '\0')
    snprintf(title, sizeof(title), englishLanguage() ? "CHMI - CZ - %s%s%s"
                                                    : "ČHMÚ - ČR - %s%s%s",
             timePrefix,
             frameTime, timeSuffix);
  else if (radiusKm == 0)
    snprintf(title, sizeof(title), englishLanguage() ? "CHMI - CZ"
                                                    : "ČHMÚ - ČR");
  else if (frameTime != nullptr && frameTime[0] != '\0')
    snprintf(title, sizeof(title),
             englishLanguage() ? "CHMI - %u km - %s%s%s"
                               : "ČHMÚ - %u km - %s%s%s",
             radiusKm,
             timePrefix, frameTime, timeSuffix);
  else
    snprintf(title, sizeof(title), englishLanguage() ? "CHMI - %u km"
                                                    : "ČHMÚ - %u km",
             radiusKm);
  lv_label_set_text(radarTitleLabel, title);
  lv_label_set_text(radarStatusLabel, "");
  alignCenter(radarTitleLabel, 0, -205);
}

void clockDashboardSetWifiAddress(const char *ipAddress) {
  if (firmwareUpdateActive) return;
  if (wifiAddressLabel == nullptr) return;
  if (ipAddress == nullptr || ipAddress[0] == '\0') {
    lv_label_set_text(wifiAddressLabel, "");
    return;
  }
  char text[32];
  snprintf(text, sizeof(text), "IP  %s", ipAddress);
  lv_label_set_text(wifiAddressLabel, text);
  alignCenter(wifiAddressLabel, 0, -112);
}

void clockDashboardSetFirmwareVersion(const char *version,
                                      bool updateAvailable) {
  if (firmwareUpdateActive) return;
  if (firmwareVersionLabel == nullptr) return;
  if (version == nullptr || version[0] == '\0') {
    lv_label_set_text(firmwareVersionLabel, "");
    return;
  }
  char text[32];
  snprintf(text, sizeof(text), "FW  %s", version);
  lv_label_set_text(firmwareVersionLabel, text);
  lv_obj_set_style_text_color(
      firmwareVersionLabel, updateAvailable ? COLOR_ERROR : COLOR_MUTED, 0);
  alignCenter(firmwareVersionLabel, 0, -88);
}

void clockDashboardSetFirmwareUpdateActive(bool active) {
  if (firmwareUpdateOverlay == nullptr || firmwareUpdateActive == active) return;
  firmwareUpdateActive = active;
  if (active) {
    clockDashboardSetFirmwareUpdateBlack(false);
    lv_obj_add_flag(dashboardContent, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(radarPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(settingsPage, LV_OBJ_FLAG_HIDDEN);
    if (radarVisible && radarVisibilityCallback != nullptr)
      radarVisibilityCallback(false);
    lv_obj_clear_flag(firmwareUpdateOverlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(firmwareUpdateOverlay);
  } else {
    lv_obj_add_flag(firmwareUpdateOverlay, LV_OBJ_FLAG_HIDDEN);
    if (settingsVisible) {
      lv_obj_clear_flag(settingsPage, LV_OBJ_FLAG_HIDDEN);
    } else if (radarVisible) {
      lv_obj_clear_flag(radarPage, LV_OBJ_FLAG_HIDDEN);
      if (radarVisibilityCallback != nullptr) radarVisibilityCallback(true);
    } else {
      lv_obj_clear_flag(dashboardContent, LV_OBJ_FLAG_HIDDEN);
    }
    clockDashboardUpdate(currentValues);
  }
  lv_obj_invalidate(lv_scr_act());
}

void clockDashboardSetFirmwareUpdateBlack(bool black) {
  if (firmwareUpdateTitleLabel == nullptr ||
      firmwareUpdateCountdownLabel == nullptr) return;
  if (black) {
    lv_obj_add_flag(firmwareUpdateTitleLabel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(firmwareUpdateCountdownLabel, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(firmwareUpdateTitleLabel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(firmwareUpdateCountdownLabel, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_invalidate(firmwareUpdateOverlay);
}

void clockDashboardSetFirmwareUpdateCountdown(uint8_t seconds) {
  if (firmwareUpdateTitleLabel == nullptr ||
      firmwareUpdateCountdownLabel == nullptr) return;
  char text[4];
  snprintf(text, sizeof(text), "%u", seconds);
  lv_obj_clear_flag(firmwareUpdateTitleLabel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(firmwareUpdateCountdownLabel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_text_opa(firmwareUpdateTitleLabel, LV_OPA_COVER, 0);
  lv_obj_set_style_text_opa(firmwareUpdateCountdownLabel, LV_OPA_COVER, 0);
  lv_label_set_text(firmwareUpdateCountdownLabel, text);
  alignCenter(firmwareUpdateCountdownLabel, 0, 35);
  lv_obj_move_foreground(firmwareUpdateTitleLabel);
  lv_obj_move_foreground(firmwareUpdateCountdownLabel);
  lv_obj_invalidate(firmwareUpdateOverlay);
}

void clockDashboardSetWebActive(bool active) {
  webActive = active;
  if (firmwareUpdateActive) return;
  applyDashboardColors();
}

void clockDashboardSetWifiConnected(bool connected) {
  wifiConnected = connected;
  if (firmwareUpdateActive) return;
  applyDashboardColors();
}

void clockDashboardSetWebMode(uint8_t mode) {
  selectedWebMode = constrain(mode, static_cast<uint8_t>(0),
                              static_cast<uint8_t>(2));
  if (webModeDropdown != nullptr)
    lv_dropdown_set_selected(webModeDropdown, selectedWebMode);
}

void clockDashboardSetDate(const char *dateText) {
  if (firmwareUpdateActive) return;
  lv_label_set_text(dateLabel, dateText);
  alignCenter(dateLabel, 0, -43);
}

void clockDashboardSetSecond(uint8_t second) {
  if (firmwareUpdateActive) return;
  if (second > SECOND_DOT_COUNT) second = SECOND_DOT_COUNT;
  if (displayedSecond == second) return;
  const bool minuteRolledOver = displayedSecond >= 59 && second <= 1;
  displayedSecond = second;
  secondTickStartedAt = millis();
  lastRenderedTimeColonColor = UINT32_MAX;
  if (minuteRolledOver) {
    secondFadeActive = true;
    secondFadeStartedAt = millis();
    lastSecondFadeFrameAt = 0;
  }
  renderSecondRing(millis());
  if (timeColonEffect != CLOCK_TIME_COLON_STEADY)
    renderTimeColon(millis(), true);
}

void clockDashboardSetTime(const char *timeText) {
  if (firmwareUpdateActive) return;
  lv_obj_set_style_text_font(timeLabel, configuredTimeFont(), 0);
  strlcpy(displayedTimeText, timeText, sizeof(displayedTimeText));
  renderTimeColon(millis(), true);
}
