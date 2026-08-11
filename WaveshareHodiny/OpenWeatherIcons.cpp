#include "OpenWeatherIcons.h"

namespace {
const lv_img_dsc_t *dayOrNight(bool isDay, const lv_img_dsc_t &day,
                               const lv_img_dsc_t &night) {
  return isDay ? &day : &night;
}
}  // namespace

const lv_img_dsc_t *openWeatherIconForCode(int weatherCode, bool isDay) {
  if (weatherCode >= 200 && weatherCode <= 232)
    return dayOrNight(isDay, openweather_11d, openweather_11n);
  if (weatherCode >= 300 && weatherCode <= 321)
    return dayOrNight(isDay, openweather_09d, openweather_09n);
  if (weatherCode >= 500 && weatherCode <= 504)
    return dayOrNight(isDay, openweather_10d, openweather_10n);
  if (weatherCode == 511)
    return dayOrNight(isDay, openweather_13d, openweather_13n);
  if (weatherCode >= 520 && weatherCode <= 531)
    return dayOrNight(isDay, openweather_09d, openweather_09n);
  if (weatherCode >= 600 && weatherCode <= 622)
    return dayOrNight(isDay, openweather_13d, openweather_13n);
  if (weatherCode >= 701 && weatherCode <= 781)
    return dayOrNight(isDay, openweather_50d, openweather_50n);
  if (weatherCode == 800)
    return dayOrNight(isDay, openweather_01d, openweather_01n);
  if (weatherCode == 801)
    return dayOrNight(isDay, openweather_02d, openweather_02n);
  if (weatherCode == 802 || weatherCode == 803)
    return dayOrNight(isDay, openweather_03d, openweather_03n);
  if (weatherCode == 804)
    return dayOrNight(isDay, openweather_04d, openweather_04n);
  return nullptr;
}
