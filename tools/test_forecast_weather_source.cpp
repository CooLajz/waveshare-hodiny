#include "../WaveshareHodiny/ForecastWeatherSource.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main() {
  ForecastHour ha, om; ha.valid=om.valid=true;
  ha.temperature=14.8f; ha.weatherCode=500; ha.isDay=false;
  om.temperature=15.5f; om.weatherCode=800; om.isDay=true;
  assert(resolveForecastCurrent(true,ha,NAN,om).temperature==14.8f);
  assert(resolveForecastCurrent(true,ha,14.3f,om).temperature==14.3f);
  assert(resolveForecastCurrent(true,ha,14.3f,om).weatherCode==500);
  assert(resolveForecastCurrent(false,ha,14.3f,om).temperature==15.5f);
  ha.temperature=NAN;
  assert(resolveForecastCurrent(true,ha,NAN,om).temperature==15.5f);
  assert(resolveForecastCurrent(true,ha,NAN,om).weatherCode==500);
  ha.valid=false;
  assert(resolveForecastCurrent(true,ha,NAN,om).weatherCode==800);
  assert(resolveForecastCurrent(true,ha,14.3f,om).temperature==14.3f);
  om.temperature=NAN;assert(std::isnan(resolveForecastCurrent(true,ha,NAN,om).temperature));
  assert(forecastSensorTemperature("14.3","°C")==14.3f);
  assert(std::fabs(forecastSensorTemperature("68","°F")-20)<0.001);
  assert(std::fabs(forecastSensorTemperature("293.15","K")-20)<0.001);
  for(auto state:{"unknown","unavailable","nan","14.3 x","999"})assert(std::isnan(forecastSensorTemperature(state,"°C")));
  assert(std::isnan(forecastSensorTemperature("14.3","%")));
  assert(forecastEntityTemperature(R"({"state":"sunny","attributes":{"temperature":14.8,"temperature_unit":"\u00b0C"}})",false)==14.8f);
  assert(forecastEntityTemperature(R"({"state":"14.3","attributes":{"unit_of_measurement":"°C"}})",true)==14.3f);
  assert(std::isnan(forecastEntityTemperature(R"({"state":"804","attributes":{"unit_of_measurement":""}})",false)));
  assert(std::isnan(forecastEntityTemperature(R"({"state":"unavailable","attributes":{"unit_of_measurement":"°C"}})",true)));
  puts("PASS: explicit temperature priority, HA weather code fallback, disabled HA, units and unavailable values");
}
