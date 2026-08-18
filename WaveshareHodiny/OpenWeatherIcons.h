#pragma once

#include "lvgl.h"

extern const lv_img_dsc_t meteocons_static_clear_day;
extern const lv_img_dsc_t meteocons_static_clear_night;
extern const lv_img_dsc_t meteocons_static_mostly_clear_day;
extern const lv_img_dsc_t meteocons_static_mostly_clear_night;
extern const lv_img_dsc_t meteocons_static_partly_cloudy_day;
extern const lv_img_dsc_t meteocons_static_partly_cloudy_night;
extern const lv_img_dsc_t meteocons_static_overcast_day;
extern const lv_img_dsc_t meteocons_static_overcast_night;
extern const lv_img_dsc_t meteocons_static_overcast;
extern const lv_img_dsc_t meteocons_static_drizzle;
extern const lv_img_dsc_t meteocons_static_rain;
extern const lv_img_dsc_t meteocons_static_sleet;
extern const lv_img_dsc_t meteocons_static_snow;
extern const lv_img_dsc_t meteocons_static_mist;
extern const lv_img_dsc_t meteocons_static_thunderstorms;

const lv_img_dsc_t *openWeatherIconForCode(int weatherCode, bool isDay);
