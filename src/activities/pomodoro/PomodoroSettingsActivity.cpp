#include "PomodoroSettingsActivity.h"

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <memory>

#include "CrossPointSettings.h"
#include "activities/util/ConfirmationActivity.h"
#include "activities/util/IntervalSelectionActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

namespace {
struct Row {
  StrId name;
  uint8_t CrossPointSettings::* field;
  uint8_t min;
  uint8_t max;
  int largeStep;
  StrId format;
  uint8_t defaultValue;
};

constexpr Row ROWS[PomodoroSettingsActivity::VALUE_COUNT] = {
    {StrId::STR_POMODORO_FOCUS, &CrossPointSettings::pomodoroFocusMinutes,
     CrossPointSettings::MIN_POMODORO_FOCUS_MINUTES, CrossPointSettings::MAX_POMODORO_FOCUS_MINUTES, 5,
     StrId::STR_POMODORO_MINUTES, CrossPointSettings::DEFAULT_POMODORO_FOCUS_MINUTES},
    {StrId::STR_POMODORO_SHORT_BREAK, &CrossPointSettings::pomodoroShortBreakMinutes,
     CrossPointSettings::MIN_POMODORO_SHORT_BREAK_MINUTES, CrossPointSettings::MAX_POMODORO_SHORT_BREAK_MINUTES, 5,
     StrId::STR_POMODORO_MINUTES, CrossPointSettings::DEFAULT_POMODORO_SHORT_BREAK_MINUTES},
    {StrId::STR_POMODORO_LONG_BREAK, &CrossPointSettings::pomodoroLongBreakMinutes,
     CrossPointSettings::MIN_POMODORO_LONG_BREAK_MINUTES, CrossPointSettings::MAX_POMODORO_LONG_BREAK_MINUTES, 5,
     StrId::STR_POMODORO_MINUTES, CrossPointSettings::DEFAULT_POMODORO_LONG_BREAK_MINUTES},
    {StrId::STR_POMODORO_SESSIONS, &CrossPointSettings::pomodoroSessionsPerCycle,
     CrossPointSettings::MIN_POMODORO_SESSIONS, CrossPointSettings::MAX_POMODORO_SESSIONS, 2, StrId::STR_NONE_OPT,
     CrossPointSettings::DEFAULT_POMODORO_SESSIONS},
};
}  // namespace

PomodoroSettingsActivity::PomodoroSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("PomodoroSettings", renderer, mappedInput) {}

void PomodoroSettingsActivity::onEnter() {
  UiListActivity::onEnter();
  for (int i = 0; i < ITEM_COUNT; i++) {
    rowItems_[i].label = i < VALUE_COUNT ? I18N.get(ROWS[i].name) : tr(STR_POMODORO_RESET_DEFAULTS);
    rowItems_[i].actionValue = static_cast<int16_t>(i);
  }
}

const char* PomodoroSettingsActivity::headerTitle() const { return tr(STR_POMODORO_SETTINGS); }

void PomodoroSettingsActivity::activateIndex(const int index) {
  if (index < 0 || index >= ITEM_COUNT) return;
  nav.selected = index;
  app.clearTapFlash();
  if (index == VALUE_COUNT) {
    confirmResetDefaults();
    return;
  }
  const Row& row = ROWS[index];
  auto picker = makeUniqueNoThrow<IntervalSelectionActivity>(renderer, mappedInput, "PomodoroInterval", row.name,
                                                             SETTINGS.*(row.field), row.min, row.max, 1, row.largeStep,
                                                             row.format);
  if (!picker) {
    LOG_ERR("POMO", "OOM: IntervalSelectionActivity");
    return;
  }
  startActivityForResult(std::move(picker), [this, index](const ActivityResult& result) {
    if (!result.isCancelled) {
      SETTINGS.*(ROWS[index].field) = static_cast<uint8_t>(std::get<IntervalResult>(result.data).value);
      SETTINGS.saveToFile();
    }
    requestUpdate();
  });
}

void PomodoroSettingsActivity::confirmResetDefaults() {
  char body[160];
  std::snprintf(body, sizeof(body), tr(STR_POMODORO_RESET_DEFAULTS_BODY),
                static_cast<unsigned>(CrossPointSettings::DEFAULT_POMODORO_FOCUS_MINUTES),
                static_cast<unsigned>(CrossPointSettings::DEFAULT_POMODORO_SHORT_BREAK_MINUTES),
                static_cast<unsigned>(CrossPointSettings::DEFAULT_POMODORO_LONG_BREAK_MINUTES),
                static_cast<unsigned>(CrossPointSettings::DEFAULT_POMODORO_SESSIONS));
  auto confirmation =
      makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, tr(STR_POMODORO_RESET_DEFAULTS_CONFIRM), body,
                                              tr(STR_CANCEL), tr(STR_POMODORO_RESET_ACTION));
  if (!confirmation) {
    LOG_ERR("POMO", "OOM: ConfirmationActivity");
    return;
  }
  startActivityForResult(std::move(confirmation), [this](const ActivityResult& result) {
    if (!result.isCancelled) {
      for (const Row& row : ROWS) SETTINGS.*(row.field) = row.defaultValue;
      SETTINGS.saveToFile();
    }
    requestUpdate();
  });
}

void PomodoroSettingsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  for (int i = 0; i < VALUE_COUNT; i++) {
    const unsigned value = SETTINGS.*(ROWS[i].field);
    if (ROWS[i].format != StrId::STR_NONE_OPT) {
      std::snprintf(values_[i], sizeof(values_[i]), I18N.get(ROWS[i].format), value);
    } else {
      std::snprintf(values_[i], sizeof(values_[i]), "%u", value);
    }
    rowItems_[i].value = values_[i];
  }

  fui::ListProps props;
  props.items = rowItems_;
  props.count = ITEM_COUNT;
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // physical buttons stay in loop()
  props.valueInset = 8;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}
