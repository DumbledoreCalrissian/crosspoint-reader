#include "SudokuSettingsActivity.h"

#include <I18n.h>

#include "CrossPointSettings.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

namespace {
struct Row {
  StrId name;
  uint8_t CrossPointSettings::* field;
};

constexpr Row ROWS[SudokuSettingsActivity::ITEM_COUNT] = {
    {StrId::STR_SUDOKU_SHOW_EASY, &CrossPointSettings::sudokuShowEasy},
    {StrId::STR_SUDOKU_MARK_CONFLICTS, &CrossPointSettings::sudokuMarkConflicts},
    {StrId::STR_SUDOKU_CHECK_BUTTON, &CrossPointSettings::sudokuCheckButton},
    {StrId::STR_SUDOKU_REVEAL_BUTTON, &CrossPointSettings::sudokuRevealButton},
};
}  // namespace

void SudokuSettingsActivity::onEnter() {
  UiListActivity::onEnter();
  for (int i = 0; i < ITEM_COUNT; i++) {
    rowItems_[i].label = I18N.get(ROWS[i].name);
    rowItems_[i].actionValue = static_cast<int16_t>(i);
  }
}

const char* SudokuSettingsActivity::headerTitle() const { return tr(STR_SUDOKU_SETTINGS); }

void SudokuSettingsActivity::activateIndex(const int index) {
  if (index < 0 || index >= ITEM_COUNT) return;
  nav.selected = index;
  uint8_t& value = SETTINGS.*(ROWS[index].field);
  value = value ? 0 : 1;
  SETTINGS.saveToFile();
  requestUpdate();
}

void SudokuSettingsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  for (int i = 0; i < ITEM_COUNT; i++) {
    rowItems_[i].value = SETTINGS.*(ROWS[i].field) ? tr(STR_STATE_ON) : tr(STR_STATE_OFF);
  }
  fui::ListProps props;
  props.items = rowItems_;
  props.count = ITEM_COUNT;
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // physical buttons stay in loop()
  props.valueInset = 8;
  syncListViewport(screen, props);
  screen.list(props);
}
