#include "OpenWeatherIcons.h"

#include "WeatherIconMapping.h"

const lv_img_dsc_t *openWeatherIconForCode(int weatherCode, bool isDay) {
  switch (weatherIconConditionForCode(weatherCode, isDay)) {
    case WeatherIconCondition::ClearDay:
      return &meteocons_static_clear_day;
    case WeatherIconCondition::ClearNight:
      return &meteocons_static_clear_night;
    case WeatherIconCondition::MostlyClearDay:
      return &meteocons_static_mostly_clear_day;
    case WeatherIconCondition::MostlyClearNight:
      return &meteocons_static_mostly_clear_night;
    case WeatherIconCondition::PartlyCloudyDay:
      return &meteocons_static_partly_cloudy_day;
    case WeatherIconCondition::PartlyCloudyNight:
      return &meteocons_static_partly_cloudy_night;
    case WeatherIconCondition::OvercastDay:
      return &meteocons_static_overcast_day;
    case WeatherIconCondition::OvercastNight:
      return &meteocons_static_overcast_night;
    case WeatherIconCondition::Overcast:
      return &meteocons_static_overcast;
    case WeatherIconCondition::Drizzle:
      return &meteocons_static_drizzle;
    case WeatherIconCondition::Rain:
      return &meteocons_static_rain;
    case WeatherIconCondition::Sleet:
      return &meteocons_static_sleet;
    case WeatherIconCondition::Snow:
      return &meteocons_static_snow;
    case WeatherIconCondition::Mist:
      return &meteocons_static_mist;
    case WeatherIconCondition::Thunderstorms:
      return &meteocons_static_thunderstorms;
    case WeatherIconCondition::Unknown:
      return nullptr;
  }
  return nullptr;
}
