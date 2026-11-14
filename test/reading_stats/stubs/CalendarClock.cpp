#include "util/ReadingCalendarClock.h"
uint64_t calendarTestLocal = 0;
uint16_t calendarTestGoal = 30;
namespace readingcalendar {
uint64_t localMilliseconds(tm*) { return calendarTestLocal; }
}  // namespace readingcalendar
