#include "ReadingStats.h"

#include <limits>

namespace readingstats {
namespace {
uint64_t addSaturated(const uint64_t value, const uint64_t delta) {
  const uint64_t limit = std::numeric_limits<uint64_t>::max();
  return delta > limit - value ? limit : value + delta;
}
}  // namespace

void Collector::pageShown(const uint32_t now) {
  if (timing) return;
  shownAt = now;
  timing = true;
}

uint32_t Collector::qualifyingDwell(const uint32_t now) const {
  if (!timing) return 0;
  const uint32_t elapsed = now - shownAt;
  return elapsed >= MIN_DWELL_MS && elapsed <= MAX_DWELL_MS ? elapsed : 0;
}

Totals Collector::preview(const uint32_t now) const {
  Totals result = settled;
  result.readingMilliseconds = addSaturated(result.readingMilliseconds, qualifyingDwell(now));
  return result;
}

void Collector::depart(const uint32_t now, const Departure reason) {
  const uint32_t dwell = qualifyingDwell(now);
  settled = preview(now);
  switch (reason) {
    case Departure::ForwardTurn:
      if (dwell) settled.forwardTurns = addSaturated(settled.forwardTurns, 1);
      break;
    case Departure::Navigation:
    case Departure::Pause:
      break;
  }
  timing = false;
}

}  // namespace readingstats
