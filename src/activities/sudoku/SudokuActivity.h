#pragma once
#include <SudokuStore.h>

#include "activities/UiListActivity.h"

// Sudoku start page: a filled Continue button (when a game is saved), then a
// "New game" section with one outlined button per difficulty. Puzzle files are
// read from /games/sudoku/; the gear opens Sudoku settings.
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
  // Continue progress, or a difficulty name with its pack status.
  char labels[MAX_ROWS][48]{};
  // Latest status shown above the list; nullptr when none.
  const char* notice = nullptr;

  void refresh();
  bool anyInstalled() const;
  int listCount() const override { return rowCount; }
  void buildScreen(UiScreen& screen) override;
  void buildHeader(UiScreen& screen);
  void buildContinue(UiScreen& screen, int index);
  void buildDifficulty(UiScreen& screen, int index, const freeink::ui::StyleSet& styles);
  void activateIndex(int index) override;
  // The page always fits, so vertical swipes have nothing to scroll.
  bool handleCustomInput() override;
  void onBackButton() override;
  void drawChrome() override {}
  void drawFooter() override {}
  void startPuzzle(sudoku::Difficulty difficulty);
  void confirmReplace(sudoku::Difficulty difficulty);
  void openBoard();
  void openSettings();
  static void action(const freeink::ui::ActionEvent& event, void* context);
};
