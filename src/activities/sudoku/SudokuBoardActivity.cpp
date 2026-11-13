#include "SudokuBoardActivity.h"

#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "SudokuActivity.h"
#include "components/UIScale.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "components/UiScreenHeader.h"
#include "fontIds.h"

namespace fui = freeink::ui;
using sudoku::CELLS;

namespace {
constexpr const char* DIGIT_LABELS[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9"};
constexpr int16_t THIN_LINE = 1;
constexpr int16_t BOX_LINE = 3;
}  // namespace

void SudokuBoardActivity::onEnter() {
  Activity::onEnter();
  if (!store.hasGame()) {
    finish();
    return;
  }
  const auto& game = store.game();
  std::snprintf(title, sizeof(title), "%s %u.%u", SudokuActivity::difficultyLabel(game.difficulty()),
                static_cast<unsigned>(game.rating10() / 10), static_cast<unsigned>(game.rating10() % 10));
  rendersUntilHalfRefresh = 1;  // first frame clears the previous screen
  resetUi();
  app.setScreen(screenFn, this);
  requestUpdate();
}

void SudokuBoardActivity::onExit() {
  // Notes-only changes are saved here; failures were already reported.
  if (dirty) saveNow();
  Activity::onExit();
}

void SudokuBoardActivity::prepareForSleep() {
  if (dirty) saveNow();
}

void SudokuBoardActivity::saveNow() {
  saveFailed = !store.save();
  dirty = saveFailed;
  if (saveFailed) LOG_ERR("SUDOKU", "Save failed; game kept in memory");
}

void SudokuBoardActivity::leave() {
  if (dirty) saveNow();
  finish();
}

void SudokuBoardActivity::loop() {
  RenderLock lock(*this);
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    leave();
    return;
  }
  const auto touch = routeTouch(mappedInput);
  if (touch.event) {
    handle(touch.event, touch.snap);
    return;
  }
  if (touch.routed && app.invalidated()) requestUpdate();
}

void SudokuBoardActivity::handle(const fui::ActionEvent& event, const fui::InputSnapshot& snap) {
  switch (event.action) {
    case Back:
    case Done:
      app.clearTapFlash();
      leave();
      return;
    case Board:
      // A gray flash over the whole board would only add ghosting.
      app.clearTapFlash();
      if (!solved) selectAt(snap.touchX, snap.touchY);
      return;
    case Digit:
      if (!solved) enterDigit(static_cast<uint8_t>(event.value));
      return;
    case Tool:
      if (!solved) useTool(event.value);
      return;
  }
}

void SudokuBoardActivity::selectAt(const int16_t x, const int16_t y) {
  if (cellSize <= 0 || x < board.x || y < board.y) return;
  const int col = (x - board.x) / cellSize;
  const int row = (y - board.y) / cellSize;
  if (col > 8 || row > 8) return;
  selected = row * 9 + col;
  requestUpdate();
}

bool SudokuBoardActivity::selectedEditable() const {
  return selected >= 0 && selected < CELLS && !store.game().isGiven(selected);
}

void SudokuBoardActivity::enterDigit(const uint8_t digit) {
  if (!selectedEditable() || digit > 9) return;
  auto& game = store.game();
  bool changed = false;
  if (digit == 0) {
    changed = game.erase(selected);
  } else if (notesMode) {
    changed = game.toggleNote(selected, digit);
  } else {
    changed = game.setValue(selected, digit);
  }
  // Notes alone are saved on exit or with the next entry.
  if (changed) afterEdit(digit == 0 || !notesMode);
}

void SudokuBoardActivity::useTool(const int16_t tool) {
  auto& game = store.game();
  switch (tool) {
    case ToolNotes:
      notesMode = !notesMode;
      requestUpdate();
      return;
    case ToolUndo:
      if (game.undo()) afterEdit(true);
      return;
    case ToolCheck:
      if (!SETTINGS.sudokuCheckButton) return;
      game.countCheck();
      checkShown = true;
      dirty = true;
      saveNow();
      requestUpdate();
      return;
    case ToolReveal:
      if (SETTINGS.sudokuRevealButton && selectedEditable() && game.reveal(selected)) afterEdit(true);
      return;
  }
}

void SudokuBoardActivity::afterEdit(const bool saveImmediately) {
  checkShown = false;
  dirty = true;
  auto& game = store.game();
  if (game.solved()) {
    solved = true;
    selected = -1;
    std::snprintf(solvedText, sizeof(solvedText), tr(STR_SUDOKU_SOLVED_BODY),
                  static_cast<unsigned>(game.rating10() / 10), static_cast<unsigned>(game.rating10() % 10),
                  static_cast<unsigned>(game.hints()));
    // The finished board stays on screen; the saved game is cleared.
    saveFailed = !store.finishGame();
    dirty = saveFailed;
    rendersUntilHalfRefresh = 1;
  } else if (saveImmediately) {
    saveNow();
  }
  requestUpdate();
}

void SudokuBoardActivity::screenFn(UiScreen& screen, void* context) {
  static_cast<SudokuBoardActivity*>(context)->buildScreen(screen);
}

void SudokuBoardActivity::buildHeader(UiScreen& screen) {
  fui::HeaderProps header;
  const auto band = prepareUiHeader(screen, renderer, header, title, Back);
  fui::header(screen.frame(), band, header);
}

void SudokuBoardActivity::buildPad(UiScreen& screen) {
  const auto& theme = screen.theme();
  const auto& game = store.game();
  const auto stroke = static_cast<uint8_t>(std::max(2, UITheme::getInstance().getMetrics().popupFrameThickness));
  fui::StyleSet styles = themedSelectionStyles(theme);
  for (fui::BoxStyle* box : {&styles.normal, &styles.selected, &styles.focused, &styles.active, &styles.disabled}) {
    box->border = fui::Paint::solid(fui::Color::Black);
    box->borderWidth = stroke;
  }
  const int16_t height = std::max(theme.rowHeight, theme.listTouchMinRowHeight);
  const int16_t gap = theme.spaceSm;
  const bool editable = selectedEditable();

  auto place = [&](const fui::Rect row, const int index, const int count, const char* label, const Action action,
                   const int16_t value, const bool enabled, const bool on) {
    const int16_t width = static_cast<int16_t>((row.width - gap * (count - 1)) / count);
    fui::ButtonProps button;
    button.label = label;
    button.action = action;
    button.value = value;
    button.styles = styles;
    button.enabled = enabled;
    button.state = on ? fui::StateSelected : fui::StateNormal;
    button.text = theme.bodyText;
    button.text.align = fui::TextAlign::Center;
    screen.button(button, fui::Rect{static_cast<int16_t>(row.x + index * (width + gap)), row.y, width, row.height});
  };

  // Bottom row first: tools, then digits 6-9 and Erase, then digits 1-5.
  const fui::Rect tools = screen.takeBottom(height, 0);
  int toolCount = 2 + (SETTINGS.sudokuCheckButton ? 1 : 0) + (SETTINGS.sudokuRevealButton ? 1 : 0);
  int slot = 0;
  place(tools, slot++, toolCount, tr(STR_SUDOKU_NOTES), Tool, ToolNotes, true, notesMode);
  place(tools, slot++, toolCount, tr(STR_SUDOKU_UNDO), Tool, ToolUndo, game.canUndo(), false);
  if (SETTINGS.sudokuCheckButton) place(tools, slot++, toolCount, tr(STR_SUDOKU_CHECK), Tool, ToolCheck, true, false);
  if (SETTINGS.sudokuRevealButton) {
    place(tools, slot++, toolCount, tr(STR_SUDOKU_REVEAL), Tool, ToolReveal, editable, false);
  }
  const fui::Rect lower = screen.takeBottom(height, gap);
  for (int i = 0; i < 4; ++i)
    place(lower, i, 5, DIGIT_LABELS[5 + i], Digit, static_cast<int16_t>(6 + i), editable, false);
  place(lower, 4, 5, tr(STR_SUDOKU_ERASE), Digit, 0, editable, false);
  const fui::Rect upper = screen.takeBottom(height, gap);
  for (int i = 0; i < 5; ++i) place(upper, i, 5, DIGIT_LABELS[i], Digit, static_cast<int16_t>(1 + i), editable, false);
}

void SudokuBoardActivity::buildSolved(UiScreen& screen) {
  const auto& theme = screen.theme();
  const int16_t height = std::max(theme.rowHeight, theme.listTouchMinRowHeight);
  fui::ButtonProps done;
  done.label = tr(STR_SUDOKU_DONE);
  done.action = Done;
  done.styles = themedSelectionStyles(theme);
  done.state = fui::StateSelected;
  done.text = theme.bodyText;
  done.text.bold = true;
  done.text.align = fui::TextAlign::Center;
  const int16_t inset = static_cast<int16_t>(theme.spaceLg * 2);
  screen.button(done, screen.takeBottom(height, 0).inset(fui::Insets{0, inset, 0, inset}));
  auto body = theme.bodyText;
  body.align = fui::TextAlign::Center;
  body.maxLines = 2;
  uiTarget.text(screen.takeBottom(static_cast<int16_t>(uiTarget.lineHeight(body.font) * 2), theme.spaceMd), solvedText,
                body);
  auto heading = theme.titleText;
  heading.align = fui::TextAlign::Center;
  heading.bold = true;
  uiTarget.text(screen.takeBottom(uiTarget.lineHeight(heading.font), theme.spaceSm), tr(STR_SUDOKU_SOLVED), heading);
}

void SudokuBoardActivity::drawBoard(const fui::ThemeTokens& theme) {
  const auto& game = store.game();
  const fui::Paint black = fui::Paint::solid(fui::Color::Black);
  const fui::StyleSet selection = themedSelectionStyles(theme);
  const bool markConflicts = SETTINGS.sudokuMarkConflicts;

  auto digitStyle = theme.titleText;
  digitStyle.align = fui::TextAlign::Center;
  uiTarget.setFont(fui::GfxRendererTarget::FONT_TITLE, NOTOSANS_18_FONT_ID);
  const int16_t digitHeight = uiTarget.lineHeight(digitStyle.font);
  auto noteStyle = theme.smallText;
  noteStyle.align = fui::TextAlign::Center;
  const int16_t noteHeight = uiTarget.lineHeight(noteStyle.font);
  const int16_t third = static_cast<int16_t>(cellSize / 3);
  char text[2] = {0, 0};

  for (int cell = 0; cell < CELLS; ++cell) {
    const fui::Rect rect{static_cast<int16_t>(board.x + (cell % 9) * cellSize),
                         static_cast<int16_t>(board.y + (cell / 9) * cellSize), cellSize, cellSize};
    fui::Color ink = fui::Color::Black;
    if (cell == selected) {
      uiTarget.fill(rect, selection.selected.background, 0);
      if (selection.selected.foreground.color != fui::Color::Transparent) ink = selection.selected.foreground.color;
    }
    const uint8_t value = game.value(cell);
    if (value) {
      digitStyle.bold = game.isGiven(cell);
      digitStyle.color = ink;
      text[0] = static_cast<char>('0' + value);
      uiTarget.text(
          fui::Rect{rect.x, static_cast<int16_t>(rect.y + (cellSize - digitHeight) / 2), cellSize, digitHeight}, text,
          digitStyle);
      const fui::Paint mark = fui::Paint::solid(ink);
      if (markConflicts && !game.isGiven(cell) && game.hasConflict(cell)) {
        const int16_t width = static_cast<int16_t>(cellSize / 2);
        uiTarget.fill(fui::Rect{static_cast<int16_t>(rect.x + (cellSize - width) / 2),
                                static_cast<int16_t>(rect.y + cellSize - 7), width, 3},
                      mark, 0);
      }
      if (checkShown && game.isWrong(cell)) uiTarget.stroke(rect.inset(fui::Insets{4, 4, 4, 4}), mark, 2, 0);
      continue;
    }
    const uint16_t notes = game.notes(cell);
    noteStyle.color = ink;
    for (int digit = 1; digit <= 9 && notes; ++digit) {
      if (!(notes & (1U << (digit - 1)))) continue;
      text[0] = static_cast<char>('0' + digit);
      const int16_t subX = static_cast<int16_t>(rect.x + ((digit - 1) % 3) * third);
      const int16_t subY = static_cast<int16_t>(rect.y + ((digit - 1) / 3) * third + (third - noteHeight) / 2);
      uiTarget.text(fui::Rect{subX, subY, third, noteHeight}, text, noteStyle);
    }
  }
  uiTarget.setFont(fui::GfxRendererTarget::FONT_TITLE, uiScaleSpec().titleFontId);

  const int16_t size = static_cast<int16_t>(cellSize * 9);
  for (int i = 0; i <= 9; ++i) {
    const int16_t width = i % 3 == 0 ? BOX_LINE : THIN_LINE;
    const int16_t offset = static_cast<int16_t>(i * cellSize - (i == 9 ? width : (i == 0 ? 0 : width / 2)));
    uiTarget.fill(fui::Rect{static_cast<int16_t>(board.x + offset), board.y, width, size}, black, 0);
    uiTarget.fill(fui::Rect{board.x, static_cast<int16_t>(board.y + offset), size, width}, black, 0);
  }
}

void SudokuBoardActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto& theme = screen.theme();
  buildHeader(screen);
  screen.setContentMarginFromScreen(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0, 0, 0});
  screen.insetContent(fui::Insets{theme.spaceMd, theme.spaceLg, theme.spaceLg, theme.spaceLg});

  if (saveFailed) {
    auto note = theme.smallText;
    note.maxLines = 2;
    uiTarget.text(screen.takeTop(static_cast<int16_t>(uiTarget.lineHeight(note.font) * 2), theme.spaceSm),
                  tr(STR_SUDOKU_SAVE_ERROR), note);
  }
  if (solved) {
    buildSolved(screen);
  } else {
    buildPad(screen);
  }
  const fui::Rect space = screen.body();
  const int16_t side = static_cast<int16_t>(std::min<int>(space.width, space.height - theme.spaceMd));
  cellSize = static_cast<int16_t>(std::max(0, side / 9));
  const int16_t size = static_cast<int16_t>(cellSize * 9);
  board = fui::Rect{static_cast<int16_t>(space.x + (space.width - size) / 2),
                    static_cast<int16_t>(space.y + std::max(0, (space.height - theme.spaceMd - size) / 2)), size, size};
  if (cellSize <= 0) return;
  screen.frame().hit(board, Board, 0, fui::InputTouch);
  drawBoard(theme);
}

void SudokuBoardActivity::render(RenderLock&&) {
  renderer.clearScreen();
  renderUi();
  // Clear ghosting at the reader's configured full-refresh cadence.
  if (--rendersUntilHalfRefresh <= 0) {
    rendersUntilHalfRefresh = SETTINGS.getRefreshFrequency();
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
  } else {
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  }
}
