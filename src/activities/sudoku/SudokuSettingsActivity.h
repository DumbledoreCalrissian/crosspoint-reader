#pragma once

#include "activities/UiListActivity.h"

// On/off options for Sudoku, opened from the Sudoku header. Tapping a row
// switches it and saves settings.json.
class SudokuSettingsActivity final : public UiListActivity {
 public:
  SudokuSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("SudokuSettings", renderer, mappedInput) {}

  static constexpr int ITEM_COUNT = 4;

  void onEnter() override;

 private:
  int listCount() const override { return ITEM_COUNT; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  freeink::ui::ListItem rowItems_[ITEM_COUNT]{};
};
