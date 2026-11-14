#include "ReadingCalendar.h"

#include <algorithm>

namespace readingcalendar {
int32_t civilDay(int year, const unsigned month, const unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const unsigned doy = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
  return era * 146097 + static_cast<int>(yoe * 365 + yoe / 4 - yoe / 100 + doy) - 719468;
}

Day* Model::prepare(const int32_t date, const uint16_t goalMinutes) {
  if (date <= 0 || !goalMinutes || goalMinutes > 1440 || (latest && date <= latest - int(CAPACITY))) return nullptr;
  if (date > latest) {
    const int32_t floor = date - int(CAPACITY) + 1;
    if (latest && floor > latest + 1)
      priorStreak = 0;
    else if (latest) {
      for (int32_t expired = latest - int(CAPACITY) + 1; expired < floor; ++expired) {
        priorStreak = achieved(expired) ? (priorStreak == UINT32_MAX ? priorStreak : priorStreak + 1) : 0;
      }
    }
    latest = date;
    for (auto& day : days) {
      if (day.date <= latest - int(CAPACITY)) day = {};
    }
  }
  auto& day = days[static_cast<unsigned>(date) % CAPACITY];
  if (day.date != date) day = Day{date, 0, goalMinutes};
  return &day;
}

bool Model::add(const int32_t date, const uint32_t milliseconds, const uint16_t goalMinutes) {
  auto* day = prepare(date, goalMinutes);
  if (!day) return false;
  day->milliseconds += std::min(milliseconds, MAX_READING_MS - day->milliseconds);
  return true;
}

bool Model::changeGoal(const int32_t today, const uint16_t minutes) {
  // A backwards clock must never rewrite historical goal results.
  if (today < latest) return false;
  auto* day = prepare(today, minutes);
  if (!day) return false;
  day->goalMinutes = minutes;
  return true;
}

const Day* Model::find(const int32_t date) const {
  if (date <= 0 || date > latest || date <= latest - int(CAPACITY)) return nullptr;
  const auto& day = days[static_cast<unsigned>(date) % CAPACITY];
  return day.date == date ? &day : nullptr;
}

bool Model::achieved(const int32_t date) const {
  const auto* day = find(date);
  return day && day->milliseconds >= uint32_t(day->goalMinutes) * 60000;
}

uint64_t Model::minutes(const int32_t first, const int32_t last) const {
  uint64_t total = 0;
  for (const auto& day : days) {
    if (day.date >= first && day.date <= last) total += day.milliseconds;
  }
  return total / 60000;
}

unsigned Model::streak(const int32_t today) const {
  int32_t date = achieved(today) ? today : today - 1;
  unsigned count = 0;
  while (count < CAPACITY && achieved(date)) {
    ++count;
    --date;
  }
  if (date < latest - int(CAPACITY) + 1 && count)
    return priorStreak > UINT32_MAX - count ? UINT32_MAX : count + priorStreak;
  return count;
}
}  // namespace readingcalendar
