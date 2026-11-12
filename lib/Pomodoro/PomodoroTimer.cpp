#include "PomodoroTimer.h"

void PomodoroTimer::configure(const Config& config) {
  cfg = config;
  if (cfg.focusMs == 0) cfg.focusMs = 1;
  if (cfg.shortBreakMs == 0) cfg.shortBreakMs = 1;
  if (cfg.longBreakMs == 0) cfg.longBreakMs = 1;
  if (cfg.focusPerCycle == 0) cfg.focusPerCycle = 1;
  if (currentState == State::Ready) loadPhase(currentPhase);
}

uint32_t PomodoroTimer::durationOf(const Phase phase) const {
  switch (phase) {
    case Phase::Focus:
      return cfg.focusMs;
    case Phase::ShortBreak:
      return cfg.shortBreakMs;
    case Phase::LongBreak:
      return cfg.longBreakMs;
  }
  return cfg.focusMs;
}

void PomodoroTimer::loadPhase(const Phase phase) {
  currentPhase = phase;
  phaseDuration = durationOf(phase);
  remaining = phaseDuration;
}

void PomodoroTimer::update(const uint32_t now) {
  if (currentState != State::Running) return;
  // Unsigned elapsed time handles the ESP32 millisecond counter wrapping.
  const uint32_t elapsed = now - lastTick;
  lastTick = now;
  if (elapsed < remaining) {
    remaining -= elapsed;
    return;
  }
  remaining = 0;
  currentState = State::Complete;
  if (currentPhase == Phase::Focus && completed < UINT8_MAX) ++completed;
}

void PomodoroTimer::start(const uint32_t now) {
  if (currentState != State::Ready && currentState != State::Paused) return;
  lastTick = now;
  currentState = State::Running;
}

void PomodoroTimer::pause(const uint32_t now) {
  update(now);
  if (currentState == State::Running) currentState = State::Paused;
}

void PomodoroTimer::resetPhase() {
  // A completed focus has already been counted; only Next can leave this state.
  if (currentState == State::Complete) return;
  loadPhase(currentPhase);
  currentState = State::Ready;
}

void PomodoroTimer::startNextPhase(const uint32_t now) {
  if (currentState != State::Complete) return;
  switch (currentPhase) {
    case Phase::Focus:
      loadPhase(completed >= cfg.focusPerCycle ? Phase::LongBreak : Phase::ShortBreak);
      break;
    case Phase::ShortBreak:
      loadPhase(Phase::Focus);
      break;
    case Phase::LongBreak:
      completed = 0;
      loadPhase(Phase::Focus);
      break;
  }
  currentState = State::Ready;
  start(now);
}
