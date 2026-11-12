#pragma once

#include "activities/UiListActivity.h"

// Pomodoro phase lengths and cycle size, opened from the Pomodoro header.
// Value rows edit one persisted value each through the shared interval dialog;
// the last row restores all four defaults after confirmation.
class PomodoroSettingsActivity final : public UiListActivity {
 public:
  explicit PomodoroSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  // Four value rows, then Reset to defaults.
  static constexpr int VALUE_COUNT = 4;
  static constexpr int ITEM_COUNT = VALUE_COUNT + 1;

  void onEnter() override;

 private:
  int listCount() const override { return ITEM_COUNT; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  void confirmResetDefaults();

  // Formatted row values; pointers stay valid for the frame.
  char values_[VALUE_COUNT][16]{};
  freeink::ui::ListItem rowItems_[ITEM_COUNT]{};
};
