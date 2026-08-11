#pragma once

#include "lvgl.h"

extern const lv_img_dsc_t openweather_01d;
extern const lv_img_dsc_t openweather_01n;
extern const lv_img_dsc_t openweather_02d;
extern const lv_img_dsc_t openweather_02n;
extern const lv_img_dsc_t openweather_03d;
extern const lv_img_dsc_t openweather_03n;
extern const lv_img_dsc_t openweather_04d;
extern const lv_img_dsc_t openweather_04n;
extern const lv_img_dsc_t openweather_09d;
extern const lv_img_dsc_t openweather_09n;
extern const lv_img_dsc_t openweather_10d;
extern const lv_img_dsc_t openweather_10n;
extern const lv_img_dsc_t openweather_11d;
extern const lv_img_dsc_t openweather_11n;
extern const lv_img_dsc_t openweather_13d;
extern const lv_img_dsc_t openweather_13n;
extern const lv_img_dsc_t openweather_50d;
extern const lv_img_dsc_t openweather_50n;

const lv_img_dsc_t *openWeatherIconForCode(int weatherCode, bool isDay);
