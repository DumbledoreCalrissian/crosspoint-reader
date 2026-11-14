#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>
namespace weather {
constexpr unsigned DAYS = 3;
constexpr unsigned MAX_PLACES = 5;
constexpr uint8_t UNKNOWN_PERCENT = 255;
constexpr uint16_t UNKNOWN_RAIN = 65535;
struct Location {
  char name[80]{};
  char region[64]{};
  char country[3]{};
  int32_t latitude = 0;  // millionths of a degree
  int32_t longitude = 0;
  bool operator==(const Location&) const = default;
};
struct Day {
  int32_t date = 0;
  int16_t high = 0;  // tenths Celsius
  int16_t low = 0;
  uint16_t rain = UNKNOWN_RAIN;  // tenths mm, water equivalent
  uint8_t chance = UNKNOWN_PERCENT;
  uint8_t code = 255;
  bool operator==(const Day&) const = default;
};
struct Forecast {
  Day days[DAYS]{};
  int32_t currentDate = 0;
  uint16_t currentMinute = 0;
  int16_t temperature = 0;
  int32_t utcOffset = 0;
  uint64_t sampleEpoch = 0;
  uint64_t fetchedEpoch = 0;
  uint8_t humidity = UNKNOWN_PERCENT;
  uint8_t code = 255;
  uint8_t isDay = 1;
  bool operator==(const Forecast&) const = default;
};
struct Snapshot {
  Location location;
  Forecast forecast;
  bool configured = false;
  bool cached = false;
  bool fahrenheit = false;
  bool operator==(const Snapshot&) const = default;
};
int32_t isoDay(std::string_view text);
void civilDate(int32_t date, unsigned& year, unsigned& month, unsigned& day);
bool validText(std::string_view text, bool empty = false);
bool validQuery(std::string_view query);
bool valid(const Location& place);
bool valid(const Forecast& forecast);
bool valid(const Snapshot& snapshot);
int temperature(int16_t tenthsC, bool fahrenheit);
}  // namespace weather
