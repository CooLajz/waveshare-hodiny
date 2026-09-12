#include "../WaveshareHodiny/HourlyForecast.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <iostream>
int main(int argc, char **argv) {
  HourlyForecast f;
  assert(parseHourlyForecast(R"({"current":{"temperature_2m":99},"hourly":{"time":[1789214400,1789218000],"temperature_2m":[24,null],"weather_code":[2,3],"is_day":[1,0]}})", f));
  assert(f.count == 2 && f.hours[0].temperature == 24 && f.hours[0].weatherCode == 802);
  assert(forecastHourAt(f,1789214400));
  assert(!forecastHourAt(f,1789218000));
  assert(!forecastHourAt(f,1789221600));
  assert(!parseHourlyForecast(R"({"hourly":{"time":[1789214400],"temperature_2m":[],"weather_code":[0],"is_day":[1]}})", f));
  assert(f.count == 2); // Failed responses must not erase the good cache.
  assert(!parseHourlyForecast(R"({"hourly":{"time":[1789214400,1789214400],"temperature_2m":[1,2],"weather_code":[0,0],"is_day":[1,1]}})", f));
  assert(!parseHourlyForecast("{}",f));
  assert(!parseHourlyForecast("{",f));
  assert(parseHourlyForecast(R"({"hourly":{"time":[1789214400],"temperature_2m":[100],"weather_code":[0],"is_day":[1]}})", f));
  assert(!f.hours[0].valid);
  if (argc > 1) {
    std::ifstream input(argv[1]); std::stringstream buffer; buffer << input.rdbuf();
    assert(parseHourlyForecast(buffer.str().c_str(),f));
    assert(f.count == 24);
    for(size_t i=0;i<24;++i) assert(f.hours[i].valid);
    assert(f.hours[23].timestamp - f.hours[0].timestamp == 23*3600);
  }
  std::cout << "Hourly forecast parser: OK\n";
}
