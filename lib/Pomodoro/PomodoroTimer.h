#pragma once

#include <cstdint>

class PomodoroTimer {
 public:
  enum class Phase : uint8_t { Focus, ShortBreak, LongBreak };
  enum class State : uint8_t { Ready, Running, Paused, Complete };

  // Defaults mirror CrossPointSettings::DEFAULT_POMODORO_*; keep them in step.
  struct Config {
    uint32_t focusMs = 25U * 60U * 1000U;
    uint32_t shortBreakMs = 5U * 60U * 1000U;
    uint32_t longBreakMs = 15U * 60U * 1000U;
    uint8_t focusPerCycle = 4;
  };

  PomodoroTimer() = default;
  explicit PomodoroTimer(const Config& config) { configure(config); }

  // New durations apply now only when the timer is Ready; otherwise at the next
  // phase or reset, so an unfinished phase keeps its length.
  void configure(const Config& config);
  void update(uint32_t now);
  void start(uint32_t now);
  void pause(uint32_t now);
  void resetPhase();
  void startNextPhase(uint32_t now);

  Phase phase() const { return currentPhase; }
  State state() const { return currentState; }
  uint32_t remainingMs() const { return remaining; }
  uint32_t phaseDurationMs() const { return phaseDuration; }
  uint8_t completedFocus() const { return completed; }
  uint8_t focusPerCycle() const { return cfg.focusPerCycle; }

 private:
  uint32_t durationOf(Phase phase) const;
  void loadPhase(Phase phase);

  Config cfg{};
  Phase currentPhase = Phase::Focus;
  State currentState = State::Ready;
  uint8_t completed = 0;
  uint32_t phaseDuration = Config{}.focusMs;
  uint32_t remaining = Config{}.focusMs;
  uint32_t lastTick = 0;
};
