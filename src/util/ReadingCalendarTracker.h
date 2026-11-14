#pragma once
#include "ReadingCalendarStore.h"
namespace readingcalendar {
// Reader-owned; all calls share the reader's RenderLock.
class Tracker {
 public:
  Tracker();
  void pageShown(uint32_t now);
  void record(uint64_t milliseconds, uint32_t end, uint32_t now, bool force);

 private:
  Store store;
  uint64_t startLocal = 0;
  uint32_t startMono = 0;
  uint32_t checkpoint = 0;
  bool dirty = false;
};
}  // namespace readingcalendar
