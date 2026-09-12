#pragma once
#include "HourlyForecast.h"
#include "ClockConfig.h"
// Called only by the existing network worker; never from the LVGL thread.
void hourlyForecastRefresh(const ClockConfig &config);
HourlyForecast hourlyForecastSnapshot();
