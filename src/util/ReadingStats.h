#pragma once

#include <cstdint>

namespace readingstats {

struct Totals {
  uint64_t readingMilliseconds = 0;
  uint64_t forwardTurns = 0;
  bool operator==(const Totals&) const = default;
};

// Single-owner state. Call only for visible book pages, never loading screens.
class Collector {
 public:
  enum class Departure { ForwardTurn, Navigation, Pause };
  static constexpr uint32_t MIN_DWELL_MS = 2000;
  static constexpr uint32_t MAX_DWELL_MS = 300000;

  explicit Collector(Totals initial = {}) : settled(initial) {}
  // Repainting the same page must not restart its dwell.
  void pageShown(uint32_t now);
  // A failed turn does not depart; a successful turn needs a new pageShown().
  void depart(uint32_t now, Departure reason);
  Totals totals() const { return settled; }
  // Includes the current qualifying dwell without settling or splitting it.
  Totals preview(uint32_t now) const;
  bool isTiming() const { return timing; }

 private:
  uint32_t qualifyingDwell(uint32_t now) const;
  Totals settled;
  uint32_t shownAt = 0;
  bool timing = false;
};

}  // namespace readingstats
