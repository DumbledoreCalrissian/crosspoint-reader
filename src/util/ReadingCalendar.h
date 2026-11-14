#pragma once

#include <array>
#include <cstdint>

namespace readingcalendar {
// Civil-day numbers, independent of timezones and seconds since the epoch.
int32_t civilDay(int year, unsigned month, unsigned day);

struct Day {
  int32_t date = 0;
  uint32_t milliseconds = 0;
  uint16_t goalMinutes = 30;
  bool operator==(const Day&) const = default;
};

class Model {
 public:
  static constexpr unsigned CAPACITY = 366;
  static constexpr uint32_t MAX_DAY_MS = 86400000;
  static constexpr uint32_t MAX_READING_MS = 90000000;  // A DST fall-back day can have 25 hours.
  void clear() {
    priorStreak = 0;
    latest = 0;
    for (auto& day : days) day = {};
  }
  bool add(int32_t date, uint32_t milliseconds, uint16_t goalMinutes);
  bool changeGoal(int32_t today, uint16_t minutes);
  const Day* find(int32_t date) const;
  uint64_t minutes(int32_t first, int32_t last) const;
  unsigned streak(int32_t today) const;
  bool achieved(int32_t date) const;
  uint32_t previousStreak() const { return priorStreak; }
  void restorePreviousStreak(uint32_t value) { priorStreak = value; }
  int32_t latestDay() const { return latest; }
  const std::array<Day, CAPACITY>& records() const { return days; }

 private:
  Day* prepare(int32_t date, uint16_t goalMinutes);
  uint32_t priorStreak = 0;
  int32_t latest = 0;
  std::array<Day, CAPACITY> days{};
};
}  // namespace readingcalendar
