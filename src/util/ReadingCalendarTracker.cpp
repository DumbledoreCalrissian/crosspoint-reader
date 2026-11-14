#include "ReadingCalendarTracker.h"

#include <Logging.h>

#include <algorithm>

#include "ReadingCalendarClock.h"
namespace readingcalendar {
Tracker::Tracker() { store.load(); }
void Tracker::pageShown(uint32_t now) {
  startLocal = localMilliseconds();
  startMono = now;
}
void Tracker::record(uint64_t milliseconds, uint32_t end, uint32_t now, bool force) {
  const uint64_t observed = localMilliseconds();
  const uint64_t expected = startLocal + uint32_t(now - startMono);
  const uint64_t difference = observed > expected ? observed - expected : expected - observed;
  if (milliseconds && store.canSave() && startLocal && observed && difference <= 2000 && milliseconds <= 300000 &&
      milliseconds == uint32_t(end - startMono)) {
    const auto first = int32_t(startLocal / Model::MAX_DAY_MS);
    const uint32_t beforeMidnight = Model::MAX_DAY_MS - startLocal % Model::MAX_DAY_MS;
    const auto goal = GOALS[store.goalIndex()];
    if (first >= store.model().latestDay() && store.model().changeGoal(first, goal)) {
      store.model().add(first, std::min<uint64_t>(milliseconds, beforeMidnight), goal);
      if (milliseconds > beforeMidnight) store.model().add(first + 1, milliseconds - beforeMidnight, goal);
      dirty = true;
    }
  }
  startLocal = 0;
  if (dirty && (force || uint32_t(now - checkpoint) >= 60000)) {
    checkpoint = now;
    if (store.save())
      dirty = false;
    else
      LOG_ERR("CAL", "Calendar save failed; pending daily totals retained");
  }
}
}  // namespace readingcalendar
