#pragma once

#include <cstdint>
#include <string_view>

#include "util/ReadingStats.h"
#include "util/ReadingStatsStore.h"

// EPUB activity-owned policy. All access is serialized by RenderLock.
class EpubReadingStats {
 public:
  using Departure = readingstats::Collector::Departure;
  using Status = readingstats::Store::Status;

  EpubReadingStats(std::string_view path, uint32_t now);
  void pageShown(uint32_t now, bool samePage = false);
  void beginNavigation(uint32_t now, Departure reason);
  void completeNavigation(uint32_t now);
  void loading(uint32_t now);
  void pause(uint32_t now);
  void stop(uint32_t now);
  bool reset();
  bool available() const;
  bool canReset() const { return store.canReset(); }
  bool unsaved() const { return available() && collector.totals() != store.totals(); }
  bool writeFailed() const { return failedWrite; }
  Status status() const { return loadStatus; }
  readingstats::Totals totals() const { return collector.totals(); }

 private:
  void save(uint32_t now, bool force);
  readingstats::Store store;
  readingstats::Collector collector;
  readingstats::Collector beforeNavigation;
  uint32_t navigationAt = 0;
  bool pendingNavigation = false;
  Status loadStatus;
  uint32_t lastCheckpoint;
  bool stopped = false;
  bool failedWrite = false;
};
