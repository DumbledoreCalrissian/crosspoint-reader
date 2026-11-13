#include "SudokuActivity.h"

#include <FreeInkUIIcon.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>

#include "CrossPointSettings.h"
#include "SudokuBoardActivity.h"
#include "SudokuSettingsActivity.h"
#include "activities/util/ConfirmationActivity.h"
#include "components/UITheme.h"
#include "components/UiScreenHeader.h"
#include "components/icons/headerIcons.h"

namespace fui = freeink::ui;
using sudoku::Difficulty;
using sudoku::PackStatus;

void SudokuActivity::onEnter() {
  UiListActivity::onEnter();
  {
    RenderLock lock(*this);
    switch (store.load()) {
      case sudoku::SudokuStore::LoadResult::Ok:
        notice = nullptr;
        break;
      case sudoku::SudokuStore::LoadResult::Recovered:
        notice = tr(STR_SUDOKU_DATA_ERROR);
        break;
      case sudoku::SudokuStore::LoadResult::Unreadable:
        notice = tr(STR_SUDOKU_READ_ERROR);
        break;
    }
  }
  refresh();
  app.on(ACTION_BACK, action, this);
  app.on(ACTION_SETTINGS, action, this);
  if (openSettingsOnEnter) openSettings();
}

void SudokuActivity::refresh() {
  RenderLock lock(*this);
  closeRouting();
  rowCount = 0;
  if (store.hasGame()) rows[rowCount++] = Row{true, store.game().difficulty()};
  for (int d = 0; d < sudoku::DIFFICULTIES; ++d) {
    const auto difficulty = static_cast<Difficulty>(d);
    packStatus[d] = sudoku::readPackInfo(difficulty, packInfo[d]);
    if (difficulty == Difficulty::Easy && !SETTINGS.sudokuShowEasy) continue;
    rows[rowCount++] = Row{false, difficulty};
  }
  for (int i = 0; i < rowCount; ++i) {
    const Row& row = rows[i];
    const int d = static_cast<int>(row.difficulty);
    if (row.isContinue) {
      const unsigned percent = static_cast<unsigned>(store.game().filledCount() * 100 / sudoku::CELLS);
      std::snprintf(values[i], sizeof(values[i]), tr(STR_SUDOKU_PROGRESS), difficultyLabel(row.difficulty), percent);
    } else if (packStatus[d] == PackStatus::Valid) {
      std::snprintf(values[i], sizeof(values[i]), tr(STR_SUDOKU_PUZZLES),
                    static_cast<unsigned long>(packInfo[d].count));
    } else {
      std::snprintf(values[i], sizeof(values[i]), "%s",
                    packStatus[d] == PackStatus::Missing ? tr(STR_SUDOKU_NOT_INSTALLED) : tr(STR_SUDOKU_DAMAGED));
    }
  }
  if (nav.selected >= rowCount) nav.reset();
  requestUpdate();
}

bool SudokuActivity::anyInstalled() const {
  for (const PackStatus status : packStatus) {
    if (status != PackStatus::Missing) return true;
  }
  return false;
}

const char* SudokuActivity::difficultyLabel(const Difficulty difficulty) {
  switch (difficulty) {
    case Difficulty::Easy:
      return tr(STR_SUDOKU_EASY);
    case Difficulty::Medium:
      return tr(STR_SUDOKU_MEDIUM);
    case Difficulty::Hard:
      return tr(STR_SUDOKU_HARD);
    case Difficulty::Diabolical:
      return tr(STR_SUDOKU_DIABOLICAL);
  }
  return "";
}

void SudokuActivity::action(const fui::ActionEvent& event, void* context) {
  auto* self = static_cast<SudokuActivity*>(context);
  self->app.clearTapFlash();
  switch (event.action) {
    case ACTION_BACK:
      self->onBackButton();
      break;
    case ACTION_SETTINGS:
      self->openSettings();
      break;
  }
}

void SudokuActivity::onBackButton() { activityManager.goHome(HomeMenuItem::SUDOKU); }

void SudokuActivity::activateIndex(const int index) {
  if (index < 0 || index >= rowCount) return;
  nav.selected = index;
  app.clearTapFlash();
  const Row row = rows[index];
  if (row.isContinue) {
    openBoard();
    return;
  }
  if (packStatus[static_cast<int>(row.difficulty)] != PackStatus::Valid) return;
  if (store.hasGame()) {
    confirmReplace(row.difficulty);
  } else {
    startPuzzle(row.difficulty);
  }
}

void SudokuActivity::confirmReplace(const Difficulty difficulty) {
  auto confirmation =
      makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, tr(STR_SUDOKU_REPLACE_TITLE),
                                              tr(STR_SUDOKU_REPLACE_BODY), tr(STR_CANCEL), tr(STR_SUDOKU_START));
  if (!confirmation) {
    LOG_ERR("SUDOKU", "Allocation failed: %u bytes", static_cast<unsigned>(sizeof(ConfirmationActivity)));
    notice = tr(STR_SUDOKU_MEMORY_ERROR);
    refresh();
    return;
  }
  startActivityForResult(std::move(confirmation), [this, difficulty](const ActivityResult& result) {
    if (result.isCancelled) {
      requestUpdate();
      return;
    }
    startPuzzle(difficulty);
  });
}

void SudokuActivity::startPuzzle(const Difficulty difficulty) {
  sudoku::SudokuStore::StartResult result;
  {
    RenderLock lock(*this);
    result = store.startNext(difficulty);
  }
  switch (result) {
    case sudoku::SudokuStore::StartResult::Ok:
    case sudoku::SudokuStore::StartResult::SaveFailed:
      openBoard();
      return;
    case sudoku::SudokuStore::StartResult::Missing:
    case sudoku::SudokuStore::StartResult::Damaged:
      notice = tr(STR_SUDOKU_OPEN_ERROR);
      refresh();
      return;
  }
}

void SudokuActivity::openBoard() {
  auto board = makeUniqueNoThrow<SudokuBoardActivity>(renderer, mappedInput, store);
  if (!board) {
    LOG_ERR("SUDOKU", "Allocation failed: %u bytes", static_cast<unsigned>(sizeof(SudokuBoardActivity)));
    notice = tr(STR_SUDOKU_MEMORY_ERROR);
    refresh();
    return;
  }
  startActivityForResult(std::move(board), [this](const ActivityResult&) {
    notice = store.unsaved() ? tr(STR_SUDOKU_SAVE_ERROR) : nullptr;
    refresh();
  });
}

void SudokuActivity::openSettings() {
  auto settings = makeUniqueNoThrow<SudokuSettingsActivity>(renderer, mappedInput, !store.unsaved());
  if (!settings) {
    LOG_ERR("SUDOKU", "Allocation failed: %u bytes", static_cast<unsigned>(sizeof(SudokuSettingsActivity)));
    notice = tr(STR_SUDOKU_MEMORY_ERROR);
    refresh();
    return;
  }
  startActivityForResult(std::move(settings), [this](const ActivityResult&) { refresh(); });
}

void SudokuActivity::provideRow(void* context, const uint16_t index, fui::ListItem& item) {
  const auto& self = *static_cast<SudokuActivity*>(context);
  if (index >= self.rowCount) return;
  const Row& row = self.rows[index];
  item.actionValue = static_cast<int16_t>(index);
  item.label = row.isContinue ? tr(STR_SUDOKU_CONTINUE) : difficultyLabel(row.difficulty);
  item.value = self.values[index];
  item.enabled = row.isContinue || self.packStatus[static_cast<int>(row.difficulty)] == PackStatus::Valid;
}

void SudokuActivity::buildHeader(UiScreen& screen) {
  fui::HeaderProps header;
  const auto band = prepareUiHeader(screen, renderer, header, tr(STR_SUDOKU), ACTION_BACK);
  header.trailingIcon = fui::bitmapFromIcon(icon_settings_32);
  header.trailingAction = ACTION_SETTINGS;
  fui::header(screen.frame(), band, header);
}

void SudokuActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto& theme = screen.theme();
  buildHeader(screen);
  screen.setContentMarginFromScreen(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0, 0, 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  const char* message = notice ? notice : (anyInstalled() ? nullptr : tr(STR_SUDOKU_NO_PUZZLES));
  if (message) {
    fui::TextAreaProps text;
    text.text = message;
    text.showCaret = false;
    text.style = theme.smallText;
    text.style.maxLines = 3;
    const auto inset = static_cast<int16_t>(theme.listInset + theme.listSidePadding);
    const auto band = screen.takeTop(static_cast<int16_t>(uiTarget.lineHeight(text.style.font) * 3));
    fui::textArea(screen.frame(), band.inset(fui::Insets{0, inset, 0, inset}), text);
  }

  fui::ListProps props;
  props.count = static_cast<uint16_t>(rowCount);
  props.rowProvider = provideRow;
  props.rowProviderCtx = this;
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  syncListViewport(screen, props);
  screen.list(props);
}
