#include "EpubReadingStats.h"

#include <Logging.h>

EpubReadingStats::EpubReadingStats(const std::string_view path, const uint32_t now)
    : loadStatus(store.load(path)), lastCheckpoint(now) {
  if (available()) collector = readingstats::Collector(store.totals());
}

bool EpubReadingStats::available() const {
  return loadStatus == Status::Ok || loadStatus == Status::Missing || loadStatus == Status::Recovered;
}

void EpubReadingStats::pageShown(const uint32_t now, const bool samePage) {
  if (stopped || !available()) return;
  if (pendingNavigation && samePage) collector = beforeNavigation;
  completeNavigation(now);
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
  save(now, false);
}

void EpubReadingStats::loading(const uint32_t now) {
  // A pending navigation already stopped timing; its target render still owns confirmation.
  if (!pendingNavigation) pause(now);
}

void EpubReadingStats::pause(const uint32_t now) {
  if (pendingNavigation) {
    // The target never became readable; keep departure time without a turn.
    collector = beforeNavigation;
    collector.depart(navigationAt, Departure::Navigation);
    pendingNavigation = false;
  }
  collector.depart(now, Departure::Pause);
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
  collector = readingstats::Collector{};
  beforeNavigation = readingstats::Collector{};
  pendingNavigation = false;
  loadStatus = Status::Ok;
  failedWrite = false;
  return true;
}
