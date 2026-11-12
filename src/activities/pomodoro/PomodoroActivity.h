#pragma once

#include <PomodoroTimer.h>

#include "activities/Activity.h"
#include "components/UiAppHost.h"

class PomodoroActivity final : public Activity, private UiAppHost {
 public:
  PomodoroActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Pomodoro", renderer, mappedInput), UiAppHost(renderer) {}

  void onEnter() override;
  // A covered timer is not counted: pause it so auto-sleep follows the child screen.
  void onCovered() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return timer.state() == PomodoroTimer::State::Running; }
  // Timing uses elapsed milliseconds, so a slower idle loop is safe.
  bool allowIdlePowerSaving() const override { return true; }

 private:
  // UiAppHost registers at most six handlers; Start also resumes a paused phase.
  enum Action : freeink::ui::ActionId { Back = 1, Start, Pause, Next, Reset, Settings };
  // Button focus order, top to bottom and left to right.
  enum class Focus : uint8_t { Settings, Primary, Reset };
  static PomodoroTimer::Config configFromSettings();
  static void screenFn(UiScreen& screen, void* context);
  static void action(const freeink::ui::ActionEvent& event, void* context);
  void buildScreen(UiScreen& screen);
  void buildHeader(UiScreen& screen);
  void buildControls(UiScreen& screen);
  void drawSessionDots(freeink::ui::Rect band);
  void activate(freeink::ui::ActionId action);
  void moveFocus(int delta);
  Focus visibleFocus() const;
  void openSettings();
  Action primaryAction() const;
  const char* primaryLabel() const;
  const char* phaseLabel() const;
  const char* statusLabel() const;
  bool canReset() const;
  uint32_t secondsRemaining() const { return (timer.remainingMs() + 999U) / 1000U; }
  uint32_t displayBucketNow() const { return (secondsRemaining() + 9U) / 10U; }

  Focus focus = Focus::Primary;
  PomodoroTimer timer;
  uint32_t displayBucket = 0;
  int rendersUntilHalfRefresh = 0;
  char timeText[6]{};
  char countText[48]{};
};
