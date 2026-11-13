#pragma once

#include <SudokuCatalog.h>

#include "activities/UiListActivity.h"

class SudokuSettingsActivity final : public UiListActivity {
 public:
  SudokuSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool canDownload = true)
      : UiListActivity("SudokuSettings", renderer, mappedInput), canDownload(canDownload) {}
  static constexpr int ITEM_COUNT = 4;
  void onEnter() override;
  void onExit() override;
  bool preventAutoSleep() override { return state == State::Transferring; }

 private:
  enum class State : uint8_t { Browsing, SelectingWifi, Ready, Transferring, AwaitingSleep, Result };
  static constexpr int ROW_COUNT = ITEM_COUNT + 1 + 2 * sudoku::DIFFICULTIES;
  static constexpr auto ACTION_CANCEL = ACTION_USER;
  State state = State::Browsing;
  const bool canDownload;
  bool networkUsed = false;
  bool returnToSettings = false;
  bool cancelRequested = false;
  int requestedDifficulty = -1;
  int currentDifficulty = 0;
  int completed = 0;
  size_t received = 0;
  size_t total = 0;
  unsigned long lastPaint = 0;
  const char* notice = nullptr;
  char resultText[192]{};
  char labels[sudoku::DIFFICULTIES * 2][64]{};
  char subtitles[sudoku::DIFFICULTIES][96]{};
  freeink::ui::ListItem rowItems[ROW_COUNT]{};

  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  bool handleCustomInput() override;
  void onBackButton() override;
  void refresh();
  void beginDownload(int difficulty);
  void finishToSettings();
  void transfer();
  bool pollCancel();
  void confirmDelete(sudoku::Difficulty difficulty);
  static void cancelAction(const freeink::ui::ActionEvent&, void* context);
};
