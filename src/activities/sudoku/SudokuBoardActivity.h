#pragma once
#include <SudokuStore.h>

#include "activities/Activity.h"
#include "components/UiAppHost.h"

// The Sudoku board: touch only. Tap a cell, then a digit. The whole board is
// one touch area mapped to a cell, so 81 cells use one of the host's
// interaction slots. Up/Down/Confirm do nothing here; Back saves and leaves.
class SudokuBoardActivity final : public Activity, private UiAppHost {
 public:
  // The retained start list owns the store throughout the board lifetime.
  SudokuBoardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, sudoku::SudokuStore& store)
      : Activity("SudokuBoard", renderer, mappedInput),
        UiAppHost(renderer),
        store(store),
        dirty(store.unsaved()),
        saveFailed(store.unsaved()) {}

  void onEnter() override;
  void onExit() override;
  void prepareForSleep() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // UiAppHost routes at most six actions; variants use the event value.
  enum Action : freeink::ui::ActionId { Back = 1, Board, Digit, Tool, Done };
  enum ToolId : int16_t { ToolNotes, ToolUndo, ToolCheck, ToolReveal };

  static void screenFn(UiScreen& screen, void* context);
  void buildScreen(UiScreen& screen);
  void buildHeader(UiScreen& screen);
  void buildPad(UiScreen& screen);
  void buildSolved(UiScreen& screen);
  void drawBoard(const freeink::ui::ThemeTokens& theme);
  void handle(const freeink::ui::ActionEvent& event, const freeink::ui::InputSnapshot& snap);
  void selectAt(int16_t x, int16_t y);
  void enterDigit(uint8_t digit);
  void useTool(int16_t tool);
  void afterEdit(bool saveImmediately);
  void saveNow();
  void leave();
  bool selectedEditable() const;

  sudoku::SudokuStore& store;
  freeink::ui::Rect board{};  // published by the render task, read under RenderLock
  int16_t cellSize = 0;
  int selected = -1;
  bool notesMode = false;
  bool checkShown = false;
  bool dirty = false;
  bool saveFailed;
  bool solved = false;
  int rendersUntilHalfRefresh = 0;
  char title[48]{};
  char solvedText[64]{};
};
