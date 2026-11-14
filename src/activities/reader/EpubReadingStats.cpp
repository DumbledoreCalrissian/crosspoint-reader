#include "EpubReadingStats.h"

#include <Logging.h>
#include <Memory.h>

#include "util/ReadingCalendarClock.h"

EpubReadingStats::EpubReadingStats(const std::string_view path, const uint32_t now)
    : loadStatus(store.load(path)), lastCheckpoint(now) {
  if (available()) collector = readingstats::Collector(store.totals());
  calendarAccounted = collector.totals().readingMilliseconds;
  if (readingcalendar::localMilliseconds()) {
    calendar = makeUniqueNoThrow<readingcalendar::Tracker>();
    if (!calendar) LOG_ERR("CAL", "OOM: reader calendar (%u bytes)", unsigned(sizeof(readingcalendar::Tracker)));
  }
}

bool EpubReadingStats::available() const {
  return loadStatus == Status::Ok || loadStatus == Status::Missing || loadStatus == Status::Recovered;
}

void EpubReadingStats::pageShown(const uint32_t now, const bool samePage) {
  if (stopped || !available()) return;
  if (pendingNavigation && samePage) {
    collector = beforeNavigation;
    pendingNavigation = false;
  } else {
    completeNavigation(now);
  }
  if (calendar && !collector.isTiming()) calendar->pageShown(now);
  collector.pageShown(now);
}

void EpubReadingStats::beginNavigation(const uint32_t now, const Departure reason) {
  if (stopped || !available() || pendingNavigation) return;
  beforeNavigation = collector;
  navigationAt = now;
  pendingNavigation = true;
  collector.depart(now, reason);
}

void EpubReadingStats::completeNavigation(const uint32_t now) {
  if (!pendingNavigation) return;
  pendingNavigation = false;
  recordCalendar(navigationAt, now, false);
  save(now, false);
}

void EpubReadingStats::loading(const uint32_t now) {
  // A pending navigation already stopped timing; its target render still owns confirmation.
  if (!pendingNavigation) pause(now);
}

void EpubReadingStats::pause(const uint32_t now) {
  const uint32_t end = pendingNavigation ? navigationAt : now;
  if (pendingNavigation) {
    // The target never became readable; keep departure time without a turn.
    collector = beforeNavigation;
    collector.depart(navigationAt, Departure::Navigation);
    pendingNavigation = false;
  }
  collector.depart(now, Departure::Pause);
  recordCalendar(end, now, true);
  save(now, true);
}

void EpubReadingStats::stop(const uint32_t now) {
  stopped = true;
  pause(now);
}

void EpubReadingStats::save(const uint32_t now, const bool force) {
  if (!unsaved() || !store.canSave() || (!force && now - lastCheckpoint < 60000)) return;
  lastCheckpoint = now;
  failedWrite = store.save(collector.totals()) != Status::Ok;
  if (failedWrite) LOG_ERR("STATS", "EPUB statistics save failed; retaining pending totals");
}

bool EpubReadingStats::reset() {
  if (!store.canReset()) return false;
  if (store.reset() != Status::Ok) {
    failedWrite = true;
    LOG_ERR("STATS", "EPUB statistics reset failed");
    return false;
  }
  calendarAccounted = 0;
  collector = readingstats::Collector{};
  beforeNavigation = readingstats::Collector{};
  pendingNavigation = false;
  loadStatus = Status::Ok;
  failedWrite = false;
  return true;
}

void EpubReadingStats::recordCalendar(uint32_t end, uint32_t now, bool force) {
  const auto total = collector.totals().readingMilliseconds;
  if (calendar) calendar->record(total >= calendarAccounted ? total - calendarAccounted : 0, end, now, force);
  calendarAccounted = total;
}
