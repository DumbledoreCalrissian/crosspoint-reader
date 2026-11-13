#pragma once
#include <SudokuStore.h>

#include "activities/UiListActivity.h"

// Sudoku start list: Continue (when a game is saved), then one row per
// difficulty. Puzzle files are read from /games/sudoku/; the gear opens
// Sudoku settings.
class SudokuActivity final : public UiListActivity {
 public:
  SudokuActivity(GfxRenderer& renderer, MappedInputManager& input, bool openSettingsOnEnter = false)
      : UiListActivity("Sudoku", renderer, input), openSettingsOnEnter(openSettingsOnEnter) {}
  void onEnter() override;
  static const char* difficultyLabel(sudoku::Difficulty difficulty);

 private:
  static constexpr auto ACTION_BACK = ACTION_USER;
  static constexpr auto ACTION_SETTINGS = ACTION_USER + 1;
  static constexpr int MAX_ROWS = 1 + sudoku::DIFFICULTIES;

  struct Row {
    bool isContinue;
    sudoku::Difficulty difficulty;
  };

  const bool openSettingsOnEnter;
  sudoku::SudokuStore store;
  sudoku::PackStatus packStatus[sudoku::DIFFICULTIES]{};
  sudoku::PackInfo packInfo[sudoku::DIFFICULTIES]{};
  Row rows[MAX_ROWS]{};
  int rowCount = 0;
  char values[MAX_ROWS][32]{};
  // Latest status shown above the list; nullptr when none.
  const char* notice = nullptr;

  void refresh();
  bool anyInstalled() const;
  int listCount() const override { return rowCount; }
  void buildScreen(UiScreen& screen) override;
  void buildHeader(UiScreen& screen);
  void activateIndex(int index) override;
  void onBackButton() override;
  void drawChrome() override {}
  void drawFooter() override {}
  void startPuzzle(sudoku::Difficulty difficulty);
  void confirmReplace(sudoku::Difficulty difficulty);
  void openBoard();
  void openSettings();
  static void provideRow(void* context, uint16_t index, freeink::ui::ListItem& item);
  static void action(const freeink::ui::ActionEvent& event, void* context);
};
