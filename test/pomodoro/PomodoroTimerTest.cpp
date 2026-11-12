#include <gtest/gtest.h>

#include "PomodoroTimer.h"
using Phase = PomodoroTimer::Phase;
using State = PomodoroTimer::State;

TEST(PomodoroTimer, ReadyIgnoresUnstartedTime) {
  PomodoroTimer t;
  t.update(2000000);
  EXPECT_EQ(t.state(), State::Ready);
  EXPECT_EQ(t.phase(), Phase::Focus);
  EXPECT_EQ(t.remainingMs(), 1500000U);
  EXPECT_EQ(t.completedFocus(), 0);
}
TEST(PomodoroTimer, PauseAndResumeExcludePausedTime) {
  PomodoroTimer t;
  t.start(500);
  t.pause(12345);
  const auto remaining = 1500000U - 11845U;
  EXPECT_EQ(t.state(), State::Paused);
  EXPECT_EQ(t.remainingMs(), remaining);
  t.update(99999999);
  EXPECT_EQ(t.remainingMs(), remaining);
  t.start(99999999);
  t.update(100000999);
  EXPECT_EQ(t.remainingMs(), remaining - 1000U);
}
TEST(PomodoroTimer, HandlesMillisecondRollover) {
  PomodoroTimer t;
  t.start(UINT32_MAX - 499);
  t.update(500);
  EXPECT_EQ(t.remainingMs(), 1499000U);
  t.pause(1500);
  EXPECT_EQ(t.remainingMs(), 1498000U);
}
TEST(PomodoroTimer, CompletionLatchesAndCountsExactlyOnce) {
  PomodoroTimer t;
  t.start(7);
  t.update(1500006);
  EXPECT_EQ(t.state(), State::Running);
  EXPECT_EQ(t.remainingMs(), 1U);
  t.update(1500007);
  EXPECT_EQ(t.state(), State::Complete);
  EXPECT_EQ(t.remainingMs(), 0U);
  EXPECT_EQ(t.completedFocus(), 1);
  t.update(999999999);
  t.pause(1000000000);
  t.start(1000000000);
  t.resetPhase();
  EXPECT_EQ(t.state(), State::Complete);
  EXPECT_EQ(t.completedFocus(), 1);
}
TEST(PomodoroTimer, DelayedUpdateDoesNotAutoAdvance) {
  PomodoroTimer t;
  t.start(100);
  t.update(7200100);
  EXPECT_EQ(t.state(), State::Complete);
  EXPECT_EQ(t.phase(), Phase::Focus);
  EXPECT_EQ(t.completedFocus(), 1);
}
TEST(PomodoroTimer, StalePauseAtDeadlineCannotStartNextPhase) {
  PomodoroTimer t;
  t.start(100);
  t.pause(1500100);
  EXPECT_EQ(t.state(), State::Complete);
  EXPECT_EQ(t.phase(), Phase::Focus);
}
TEST(PomodoroTimer, ResetStopsPhaseAndKeepsCount) {
  PomodoroTimer t;
  t.start(0);
  t.update(1500000);
  t.startNextPhase(1500100);
  t.pause(1510100);
  t.resetPhase();
  EXPECT_EQ(t.state(), State::Ready);
  EXPECT_EQ(t.phase(), Phase::ShortBreak);
  EXPECT_EQ(t.remainingMs(), 300000U);
  EXPECT_EQ(t.completedFocus(), 1);
  t.start(2000000);
  t.update(2000001);
  EXPECT_EQ(t.remainingMs(), 299999U);
}
TEST(PomodoroTimer, NextCannotSkipUnfinishedPhase) {
  PomodoroTimer t;
  t.startNextPhase(100);
  EXPECT_EQ(t.state(), State::Ready);
  t.start(200);
  t.startNextPhase(999);
  t.update(1200);
  EXPECT_EQ(t.phase(), Phase::Focus);
  EXPECT_EQ(t.remainingMs(), 1499000U);
}
TEST(PomodoroTimer, FullCycleUsesLongBreakAndStartsFresh) {
  PomodoroTimer t;
  uint32_t now = 100;
  t.start(now);
  for (unsigned focus = 1; focus <= 4; ++focus) {
    now += 1500000;
    t.update(now);
    ASSERT_EQ(t.state(), State::Complete);
    ASSERT_EQ(t.completedFocus(), focus);
    t.startNextPhase(++now);
    EXPECT_EQ(t.phase(), focus == 4 ? Phase::LongBreak : Phase::ShortBreak);
    const uint32_t rest = focus == 4 ? 900000 : 300000;
    EXPECT_EQ(t.remainingMs(), rest);
    now += rest;
    t.update(now);
    EXPECT_EQ(t.state(), State::Complete);
    EXPECT_EQ(t.completedFocus(), focus);
    t.startNextPhase(++now);
    EXPECT_EQ(t.phase(), Phase::Focus);
    EXPECT_EQ(t.remainingMs(), 1500000U);
  }
  EXPECT_EQ(t.completedFocus(), 0);
  EXPECT_EQ(t.state(), State::Running);
}
TEST(PomodoroTimer, UpdateFrequencyDoesNotChangeDuration) {
  PomodoroTimer sparse, frequent;
  sparse.start(42);
  frequent.start(42);
  for (uint32_t now = 43; now <= 600042; now += 137) frequent.update(now);
  frequent.update(600042);
  sparse.update(600042);
  EXPECT_EQ(frequent.remainingMs(), sparse.remainingMs());
  EXPECT_EQ(sparse.remainingMs(), 900000U);
}
namespace {
PomodoroTimer::Config custom(uint32_t focusMin, uint32_t shortMin, uint32_t longMin, uint8_t cycle) {
  PomodoroTimer::Config c;
  c.focusMs = focusMin * 60000U;
  c.shortBreakMs = shortMin * 60000U;
  c.longBreakMs = longMin * 60000U;
  c.focusPerCycle = cycle;
  return c;
}
}  // namespace
TEST(PomodoroTimer, CustomCycleUsesConfiguredDurations) {
  PomodoroTimer t(custom(50, 10, 30, 2));
  EXPECT_EQ(t.remainingMs(), 3000000U);
  EXPECT_EQ(t.phaseDurationMs(), 3000000U);
  EXPECT_EQ(t.focusPerCycle(), 2);
  uint32_t now = 0;
  t.start(now);
  for (unsigned focus = 1; focus <= 2; ++focus) {
    now += 3000000;
    t.update(now);
    ASSERT_EQ(t.state(), State::Complete);
    t.startNextPhase(now);
    const uint32_t rest = focus == 2 ? 1800000U : 600000U;
    EXPECT_EQ(t.phase(), focus == 2 ? Phase::LongBreak : Phase::ShortBreak);
    EXPECT_EQ(t.phaseDurationMs(), rest);
    now += rest;
    t.update(now);
    t.startNextPhase(now);
  }
  EXPECT_EQ(t.phase(), Phase::Focus);
  EXPECT_EQ(t.completedFocus(), 0);
}
TEST(PomodoroTimer, ConfigureWhileReadyAppliesImmediately) {
  PomodoroTimer t;
  t.configure(custom(40, 5, 15, 4));
  EXPECT_EQ(t.state(), State::Ready);
  EXPECT_EQ(t.remainingMs(), 2400000U);
  EXPECT_EQ(t.phaseDurationMs(), 2400000U);
}
TEST(PomodoroTimer, ConfigureKeepsUnfinishedPhaseUntilReset) {
  PomodoroTimer t;
  t.start(0);
  t.pause(60000);
  t.configure(custom(10, 2, 15, 4));
  EXPECT_EQ(t.state(), State::Paused);
  EXPECT_EQ(t.remainingMs(), 1440000U);
  EXPECT_EQ(t.phaseDurationMs(), 1500000U);
  t.resetPhase();
  EXPECT_EQ(t.remainingMs(), 600000U);
  t.start(0);
  t.update(600000);
  t.startNextPhase(600000);
  EXPECT_EQ(t.phaseDurationMs(), 120000U);
}
TEST(PomodoroTimer, ShorterCycleTakesLongBreakAtNextBoundary) {
  uint32_t now = 0;
  // Third focus completes after the cycle shrinks to two: long break follows.
  PomodoroTimer u;
  u.start(now);
  for (int focus = 0; focus < 2; ++focus) {
    now += 1500000;
    u.update(now);
    u.startNextPhase(now);
    now += 300000;
    u.update(now);
    u.startNextPhase(now);
  }
  u.configure(custom(25, 5, 15, 2));
  now += 1500000;
  u.update(now);
  EXPECT_EQ(u.completedFocus(), 3);
  u.startNextPhase(now);
  EXPECT_EQ(u.phase(), Phase::LongBreak);
}
TEST(PomodoroTimer, ZeroConfigIsClampedToRunnableValues) {
  PomodoroTimer t(custom(0, 0, 0, 0));
  EXPECT_EQ(t.remainingMs(), 1U);
  EXPECT_EQ(t.focusPerCycle(), 1);
  t.start(0);
  t.update(1);
  EXPECT_EQ(t.state(), State::Complete);
  t.startNextPhase(1);
  EXPECT_EQ(t.phase(), Phase::LongBreak);
}
