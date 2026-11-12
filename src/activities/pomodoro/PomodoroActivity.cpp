#include "PomodoroActivity.h"

#include <FreeInkUIIcon.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "PomodoroSettingsActivity.h"
#include "components/UIScale.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "components/icons/headerIcons.h"
#include "components/icons/listIcons.h"
#include "fontIds.h"
#include "util/ButtonNavigator.h"

namespace fui = freeink::ui;

namespace {
constexpr int16_t PROGRESS_HEIGHT = 12;

constexpr int16_t DOT_SIZE = 18;
constexpr int16_t DOT_GAP = 12;
}  // namespace

PomodoroTimer::Config PomodoroActivity::configFromSettings() {
  PomodoroTimer::Config config;
  config.focusMs = SETTINGS.pomodoroFocusMinutes * 60000U;
  config.shortBreakMs = SETTINGS.pomodoroShortBreakMinutes * 60000U;
  config.longBreakMs = SETTINGS.pomodoroLongBreakMinutes * 60000U;
  config.focusPerCycle = SETTINGS.pomodoroSessionsPerCycle;
  return config;
}

void PomodoroActivity::onEnter() {
  Activity::onEnter();
  timer = PomodoroTimer{configFromSettings()};
  displayBucket = displayBucketNow();
  focus = Focus::Primary;
  rendersUntilHalfRefresh = SETTINGS.getRefreshFrequency();
  resetUi();
  for (const auto id : {Back, Start, Pause, Next, Reset, Settings}) app.on(id, action, this);
  app.setScreen(screenFn, this);
  requestUpdate();
}

void PomodoroActivity::onCovered() {
  timer.pause(millis());
  displayBucket = displayBucketNow();
  closeRouting();
}

void PomodoroActivity::screenFn(UiScreen& screen, void* context) {
  static_cast<PomodoroActivity*>(context)->buildScreen(screen);
}

void PomodoroActivity::action(const fui::ActionEvent& event, void* context) {
  static_cast<PomodoroActivity*>(context)->activate(event.action);
}

void PomodoroActivity::activate(const fui::ActionId id) {
  app.clearTapFlash();
  const uint32_t now = millis();
  switch (id) {
    case Back:
      activityManager.goHome(HomeMenuItem::POMODORO);
      return;
    case Settings:
      focus = Focus::Settings;
      openSettings();
      return;
    case Start:
      focus = Focus::Primary;
      timer.start(now);
      break;
    case Pause:
      focus = Focus::Primary;
      timer.pause(now);
      break;
    case Next:
      focus = Focus::Primary;
      timer.startNextPhase(now);
      break;
    case Reset:
      focus = Focus::Reset;
      timer.resetPhase();
      break;
  }
  displayBucket = displayBucketNow();
  requestUpdate();
}

void PomodoroActivity::openSettings() {
  auto settings = makeUniqueNoThrow<PomodoroSettingsActivity>(renderer, mappedInput);
  if (!settings) {
    LOG_ERR("POMO", "Allocation failed: %u bytes", static_cast<unsigned>(sizeof(PomodoroSettingsActivity)));
    return;
  }
  startActivityForResult(std::move(settings), [this](const ActivityResult&) {
    RenderLock lock(*this);
    timer.configure(configFromSettings());
    displayBucket = displayBucketNow();
    requestUpdate();
  });
}

void PomodoroActivity::loop() {
  RenderLock lock(*this);
  const auto previousState = timer.state();
  timer.update(millis());
  const uint32_t bucket = displayBucketNow();
  if (bucket != displayBucket || previousState != timer.state()) {
    displayBucket = bucket;
    requestUpdate();
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    activate(Back);
    return;
  }
  const auto touch = routeTouch(mappedInput);
  if (touch.event) return;
  if (touch.routed && app.invalidated()) requestUpdate();
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    switch (visibleFocus()) {
      case Focus::Settings:
        activate(Settings);
        break;
      case Focus::Primary:
        activate(primaryAction());
        break;
      case Focus::Reset:
        activate(Reset);
        break;
    }
    return;
  }
  ButtonNavigator::onNextPress([this] { moveFocus(1); });
  ButtonNavigator::onPreviousPress([this] { moveFocus(-1); });
}

PomodoroActivity::Focus PomodoroActivity::visibleFocus() const {
  // Reset is hidden on Ready and completed phases.
  return focus == Focus::Reset && !canReset() ? Focus::Primary : focus;
}

void PomodoroActivity::moveFocus(const int delta) {
  const int count = canReset() ? 3 : 2;
  const int current = static_cast<int>(visibleFocus());
  const int next =
      delta > 0 ? ButtonNavigator::nextIndex(current, count) : ButtonNavigator::previousIndex(current, count);
  focus = static_cast<Focus>(next);
  requestUpdate();
}

bool PomodoroActivity::canReset() const {
  return timer.state() == PomodoroTimer::State::Running || timer.state() == PomodoroTimer::State::Paused;
}

PomodoroActivity::Action PomodoroActivity::primaryAction() const {
  switch (timer.state()) {
    case PomodoroTimer::State::Ready:
      return Start;
    case PomodoroTimer::State::Running:
      return Pause;
    case PomodoroTimer::State::Paused:
      return Start;
    case PomodoroTimer::State::Complete:
      return Next;
  }
  return Start;
}

const char* PomodoroActivity::primaryLabel() const {
  switch (timer.state()) {
    case PomodoroTimer::State::Ready:
      return tr(STR_POMODORO_START);
    case PomodoroTimer::State::Running:
      return tr(STR_POMODORO_PAUSE);
    case PomodoroTimer::State::Paused:
      return tr(STR_RESUME);
    case PomodoroTimer::State::Complete:
      return tr(STR_POMODORO_NEXT);
  }
  return "";
}

const char* PomodoroActivity::phaseLabel() const {
  switch (timer.phase()) {
    case PomodoroTimer::Phase::Focus:
      return tr(STR_POMODORO_FOCUS);
    case PomodoroTimer::Phase::ShortBreak:
      return tr(STR_POMODORO_SHORT_BREAK);
    case PomodoroTimer::Phase::LongBreak:
      return tr(STR_POMODORO_LONG_BREAK);
  }
  return "";
}

const char* PomodoroActivity::statusLabel() const {
  switch (timer.state()) {
    case PomodoroTimer::State::Ready:
      return tr(STR_POMODORO_READY);
    case PomodoroTimer::State::Running:
      return tr(STR_POMODORO_RUNNING);
    case PomodoroTimer::State::Paused:
      return tr(STR_POMODORO_PAUSED);
    case PomodoroTimer::State::Complete:
      return timer.phase() == PomodoroTimer::Phase::Focus ? tr(STR_POMODORO_FOCUS_COMPLETE)
                                                          : tr(STR_POMODORO_BREAK_COMPLETE);
  }
  return "";
}

void PomodoroActivity::buildHeader(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto& theme = screen.theme();
  fui::HeaderProps header;
  header.title = tr(STR_POMODORO);
  header.titleText = theme.titleText;
  header.titleText.align = theme.headerTitleAlign;
  header.sidePadding = theme.headerSidePadding;
  header.minTouchSize = theme.minTouchSize;
  header.styles = theme.popup;
  if (header.styles.normal.border.kind == fui::PaintKind::None && theme.headerUnderline > 0) {
    header.styles.normal.border = fui::Paint::solid(fui::Color::Black);
    header.styles.normal.borderWidth = theme.headerUnderline;
  }
  header.borderEdges = fui::EdgeBottom;
  header.trailingStyles = fui::plainStyles(fui::Paint::solid(fui::Color::Black));
  if (visibleFocus() == Focus::Settings) {
    // The header button has no selected state; paint its normal style as the selection.
    header.trailingStyles = themedSelectionStyles(theme);
    header.trailingStyles.normal = header.trailingStyles.selected;
  }
  GUI.applyHeaderStatus(renderer, header);
  header.leadingIcon = fui::bitmapFromIcon(icon_header_back_32);
  header.leadingAction = Back;
  header.trailingIcon = fui::bitmapFromIcon(icon_settings_32);
  header.trailingAction = Settings;
  const auto frame = screen.frame().screen();
  const auto band = BaseTheme::headerBand(renderer,
                                          fui::Rect{frame.x, static_cast<int16_t>(metrics.topPadding), frame.width,
                                                    static_cast<int16_t>(metrics.headerHeight)},
                                          header);
  fui::header(screen.frame(), band, header);
}

void PomodoroActivity::buildControls(UiScreen& screen) {
  const auto& theme = screen.theme();
  const auto stroke = static_cast<uint8_t>(std::max(2, UITheme::getInstance().getMetrics().popupFrameThickness));
  // Outlined buttons whose focus paint is the theme's list selection.
  fui::StyleSet styles = themedSelectionStyles(theme);
  for (fui::BoxStyle* box : {&styles.normal, &styles.selected, &styles.focused, &styles.active, &styles.disabled}) {
    box->border = fui::Paint::solid(fui::Color::Black);
    box->borderWidth = stroke;
  }

  const auto height = std::max(theme.rowHeight, theme.listTouchMinRowHeight);
  const fui::Rect row = screen.takeBottom(height, theme.spaceLg);
  const Focus focused = visibleFocus();

  fui::ButtonProps primary;
  primary.label = primaryLabel();
  primary.action = primaryAction();
  primary.styles = styles;
  primary.state = focused == Focus::Primary ? fui::StateSelected : fui::StateNormal;
  primary.text = theme.bodyText;
  primary.text.bold = true;
  primary.text.align = fui::TextAlign::Center;

  if (!canReset()) {
    screen.button(primary, row);
    return;
  }
  const int16_t half = static_cast<int16_t>((row.width - theme.spaceMd) / 2);
  fui::ButtonProps reset;
  reset.label = tr(STR_POMODORO_RESET);
  reset.action = Reset;
  reset.styles = styles;
  reset.state = focused == Focus::Reset ? fui::StateSelected : fui::StateNormal;
  reset.text = theme.bodyText;
  reset.text.align = fui::TextAlign::Center;
  screen.button(primary, fui::Rect{row.x, row.y, half, row.height});
  screen.button(reset, fui::Rect{static_cast<int16_t>(row.x + row.width - half), row.y, half, row.height});
}

void PomodoroActivity::drawSessionDots(const fui::Rect band) {
  const int count = timer.focusPerCycle();
  const int filled = std::min<int>(timer.completedFocus(), count);
  const int width = count * DOT_SIZE + (count - 1) * DOT_GAP;
  int x = band.x + (band.width - width) / 2;
  for (int i = 0; i < count; ++i, x += DOT_SIZE + DOT_GAP) {
    const fui::Rect dot{static_cast<int16_t>(x), band.y, DOT_SIZE, DOT_SIZE};
    if (i < filled) {
      uiTarget.fill(dot, fui::Paint::solid(fui::Color::Black), DOT_SIZE / 2);
    } else {
      uiTarget.stroke(dot, fui::Paint::solid(fui::Color::Black), 2, DOT_SIZE / 2);
    }
  }
}

void PomodoroActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto& theme = screen.theme();
  buildHeader(screen);
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.insetContent(fui::Insets{theme.spaceLg, theme.spaceLg, theme.spaceLg, theme.spaceLg});

  auto note = theme.smallText;
  note.align = fui::TextAlign::Center;
  note.maxLines = 2;
  uiTarget.text(screen.takeBottom(uiTarget.lineHeight(note.font) * 2, theme.spaceLg), tr(STR_POMODORO_FOREGROUND),
                note);
  buildControls(screen);

  const uint32_t seconds = secondsRemaining();
  std::snprintf(timeText, sizeof(timeText), "%02u:%02u", static_cast<unsigned>(seconds / 60U),
                static_cast<unsigned>(seconds % 60U));
  std::snprintf(countText, sizeof(countText), tr(STR_POMODORO_COUNT),
                static_cast<unsigned>(std::min(timer.completedFocus(), timer.focusPerCycle())),
                static_cast<unsigned>(timer.focusPerCycle()));

  // The countdown borrows the title slot for the large sans, like the slider readout.
  auto phaseStyle = theme.titleText;
  phaseStyle.align = fui::TextAlign::Center;
  auto statusStyle = theme.bodyText;
  statusStyle.align = fui::TextAlign::Center;
  auto countStyle = theme.smallText;
  countStyle.align = fui::TextAlign::Center;
  const int16_t titleLh = uiTarget.lineHeight(theme.titleText.font);
  uiTarget.setFont(fui::GfxRendererTarget::FONT_TITLE, NOTOSANS_18_FONT_ID);
  const int16_t timeLh = uiTarget.lineHeight(theme.titleText.font);
  uiTarget.setFont(fui::GfxRendererTarget::FONT_TITLE, uiScaleSpec().titleFontId);
  const int16_t bodyLh = uiTarget.lineHeight(theme.bodyText.font);
  const int16_t smallLh = uiTarget.lineHeight(theme.smallText.font);
  const int groupHeight =
      titleLh + timeLh + PROGRESS_HEIGHT + bodyLh + DOT_SIZE + smallLh + theme.spaceMd * 2 + theme.spaceLg * 3;
  screen.spacer(static_cast<int16_t>(std::max(0, (screen.body().height - groupHeight) / 2)));

  uiTarget.text(screen.takeTop(titleLh, theme.spaceMd), phaseLabel(), phaseStyle);

  uiTarget.setFont(fui::GfxRendererTarget::FONT_TITLE, NOTOSANS_18_FONT_ID);
  auto timeStyle = theme.titleText;
  timeStyle.bold = true;
  timeStyle.align = fui::TextAlign::Center;
  uiTarget.text(screen.takeTop(timeLh, theme.spaceLg), timeText, timeStyle);
  uiTarget.setFont(fui::GfxRendererTarget::FONT_TITLE, uiScaleSpec().titleFontId);

  const fui::Rect progressBand = screen.takeTop(PROGRESS_HEIGHT, theme.spaceLg);
  const int16_t progressInset = static_cast<int16_t>(progressBand.width / 8);
  fui::ProgressBarProps progress;
  progress.max = static_cast<int32_t>(std::min<uint32_t>(timer.phaseDurationMs() / 1000U, INT32_MAX));
  progress.value = static_cast<int32_t>(std::min<uint32_t>(
      (timer.phaseDurationMs() - std::min(timer.remainingMs(), timer.phaseDurationMs())) / 1000U, INT32_MAX));
  progress.border = fui::Paint::solid(fui::Color::Black);
  progress.borderWidth = 2;
  fui::progressBar(screen.frame(), progressBand.inset(fui::Insets{0, progressInset, 0, progressInset}), progress);

  uiTarget.text(screen.takeTop(bodyLh, theme.spaceLg), statusLabel(), statusStyle);
  drawSessionDots(screen.takeTop(DOT_SIZE, theme.spaceMd));
  uiTarget.text(screen.takeTop(smallLh, 0), countText, countStyle);
}

void PomodoroActivity::render(RenderLock&&) {
  renderer.clearScreen();
  renderUi();
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  // Clear countdown ghosting at the reader's configured full-refresh cadence.
  if (--rendersUntilHalfRefresh <= 0) {
    rendersUntilHalfRefresh = SETTINGS.getRefreshFrequency();
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
  } else {
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  }
}
