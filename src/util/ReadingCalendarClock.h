#pragma once
#include <cstdint>
#include <ctime>

#include "ReadingCalendar.h"

namespace readingcalendar {
// Zero means that the device has no trusted local date.
uint64_t localMilliseconds(tm* date = nullptr);
void loadGoalSetting();
constexpr uint8_t GOALS[] = {5, 10, 15, 20, 30, 45, 60};
}  // namespace readingcalendar
