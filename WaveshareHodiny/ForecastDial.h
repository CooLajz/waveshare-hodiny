#pragma once
#include <lvgl.h>
lv_obj_t *forecastDialCreate(lv_obj_t *parent);
void forecastDialSetVisible(bool visible);
void forecastDialUpdate(bool redNight);
void forecastDialSetWeather(int code, bool isDay, float temperature, bool animate);
void forecastDialSetAnimation(const lv_img_dsc_t *source);

