#pragma once

#include "EpubReadingStats.h"
#include "activities/Activity.h"
#include "components/OptionPopup.h"

class EpubBookStatisticsActivity final : public Activity {
 public:
  EpubBookStatisticsActivity(GfxRenderer& renderer, MappedInputManager& input, EpubReadingStats* statistics,
                             int percent)
      : Activity("BookStatistics", renderer, input), statistics(statistics), percent(percent) {}
  void onEnter() override;
  void loop() override;
  void prepareForSleep() override { sleeping = true; }
  void render(RenderLock&&) override;

 private:
  enum class Action { None, Close, ConfirmReset, Reset, Summary, Retry };
  void showSummary();
  EpubReadingStats* statistics;  // Retained parent owns the sole collector/store.
  int percent;
  OptionPopup popup;
  Action pending = Action::None;
  bool confirmingReset = false;
  bool sleeping = false;
};
