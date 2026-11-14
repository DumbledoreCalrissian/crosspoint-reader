#pragma once
#include "WeatherModel.h"
namespace weather {
bool parseLocations(const char* bytes, size_t size, Location* locations, unsigned& count);
bool parseForecast(const char* bytes, size_t size, Forecast& forecast);
}  // namespace weather
