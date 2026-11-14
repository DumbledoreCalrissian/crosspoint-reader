#pragma once
#include <cstddef>
#include <cstdint>

#include "util/WeatherModel.h"
namespace weatherui {
const char* condition(uint8_t code);
const uint8_t* icon(uint8_t code, bool daylight, bool large = false);
void temperature(char* out, size_t cap, int16_t value, bool fahrenheit, bool unit = false);
void range(char* out, size_t cap, const weather::Day& day, bool fahrenheit);
void precipitation(char* out, size_t cap, uint16_t value, bool fahrenheit);
void percent(char* out, size_t cap, uint8_t value);
void day(char* out, size_t cap, int32_t date, const weather::Forecast& forecast, uint64_t now, bool compact = false);
void updated(char* out, size_t cap, const weather::Forecast& forecast);
bool stale(const weather::Forecast& forecast, uint64_t now);
}  // namespace weatherui
