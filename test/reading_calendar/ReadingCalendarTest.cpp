#include <HalStorage.h>
#include <Memory.h>
#include <gtest/gtest.h>

#include "ReadingCalendar.h"
#include "ReadingCalendarStore.h"
using namespace readingcalendar;
TEST(Calendar, DatesIncludeLeapDays) {
  EXPECT_EQ(civilDay(2024, 3, 1) - civilDay(2024, 2, 28), 2);
  EXPECT_EQ(civilDay(2025, 3, 1) - civilDay(2025, 2, 28), 1);
}
TEST(Calendar, BurstsAndMonthlyTotalsAreIndependentOfGoal) {
  Model m;
  m.add(20000, 90000, 30);
  m.add(20000, 90000, 30);
  EXPECT_FALSE(m.achieved(20000));
  EXPECT_EQ(m.minutes(20000, 20000), 3);
  m.add(20000, 27 * 60000, 30);
  EXPECT_TRUE(m.achieved(20000));
  EXPECT_EQ(m.minutes(20000, 20000), 30);
}
TEST(Calendar, ChangingTodayPreservesHistory) {
  Model m;
  m.add(20000, 10 * 60000, 10);
  m.add(20001, 10 * 60000, 30);
  EXPECT_TRUE(m.changeGoal(20001, 5));
  EXPECT_TRUE(m.achieved(20000));
  EXPECT_TRUE(m.achieved(20001));
  EXPECT_FALSE(m.changeGoal(20000, 60));
  EXPECT_TRUE(m.changeGoal(20001, 60));
  EXPECT_FALSE(m.achieved(20001));
  EXPECT_TRUE(m.achieved(20000));
  EXPECT_EQ(m.minutes(20000, 20001), 20);
}
TEST(Calendar, StreakSurvivesMorningAndCrossesMonth) {
  Model m;
  for (int i = 0; i < 5; ++i) m.add(20000 + i, 30 * 60000, 30);
  EXPECT_EQ(m.streak(20004), 5);
  EXPECT_EQ(m.streak(20005), 5);
  EXPECT_EQ(m.streak(20006), 0);
}
TEST(Calendar, BoundedRetentionAndSaturation) {
  Model m;
  m.add(20000, UINT32_MAX, 30);
  EXPECT_EQ(m.find(20000)->milliseconds, Model::MAX_READING_MS);
  m.add(20365, 1, 30);
  EXPECT_NE(m.find(20000), nullptr);
  m.add(20366, 1, 30);
  EXPECT_EQ(m.find(20000), nullptr);
  EXPECT_FALSE(m.add(20000, 10, 30));
}
class CalendarStore : public testing::Test {
 protected:
  void SetUp() override {
    Storage = {};
    failStatsAllocation = false;
  }
};
TEST_F(CalendarStore, RoundTripAndReadOnlyLoad) {
  Store s;
  EXPECT_EQ(s.load(), Store::Status::Missing);
  s.model().add(20000, 100000, 30);
  ASSERT_TRUE(s.save());
  const auto files = Storage.files;
  const auto writes = Storage.writes;
  Store loaded;
  EXPECT_EQ(loaded.load(), Store::Status::Ok);
  EXPECT_EQ(loaded.model().minutes(20000, 20000), 1);
  EXPECT_EQ(Storage.files, files);
  EXPECT_EQ(Storage.writes, writes);
}
TEST_F(CalendarStore, AlternatingSnapshotsAndCorruptionRecovery) {
  Store s;
  s.load();
  s.model().add(20000, 60000, 30);
  ASSERT_TRUE(s.save());
  s.model().add(20000, 60000, 30);
  ASSERT_TRUE(s.save());
  Storage.files["/.crosspoint/reading-calendar-b.bin"][20] ^= 1;
  Store recovered;
  EXPECT_EQ(recovered.load(), Store::Status::Recovered);
  EXPECT_EQ(recovered.model().minutes(20000, 20000), 1);
}
TEST_F(CalendarStore, FailedPromotionKeepsActiveSnapshot) {
  Store s;
  s.load();
  s.model().add(20000, 60000, 30);
  ASSERT_TRUE(s.save());
  s.model().add(20000, 60000, 30);
  Storage.failRename = true;
  EXPECT_FALSE(s.save());
  Storage.failRename = false;
  Store loaded;
  ASSERT_EQ(loaded.load(), Store::Status::Ok);
  EXPECT_EQ(loaded.model().minutes(20000, 20000), 1);
}
TEST_F(CalendarStore, PartialWritesKeepSnapshot) {
  Store s;
  s.load();
  s.model().add(20000, 60000, 30);
  ASSERT_TRUE(s.save());
  const auto active = Storage.files["/.crosspoint/reading-calendar-a.bin"];
  for (int budget = 0; budget < 40; ++budget) {
    Storage.writeBudget = budget;
    EXPECT_FALSE(s.save());
    EXPECT_EQ(Storage.files["/.crosspoint/reading-calendar-a.bin"], active);
  }
}
TEST_F(CalendarStore, CorruptHistoryBlocksWrites) {
  Storage.files["/.crosspoint/reading-calendar-a.bin"] = "broken";
  const auto files = Storage.files;
  Store s;
  EXPECT_EQ(s.load(), Store::Status::Invalid);
  EXPECT_FALSE(s.save());
  EXPECT_EQ(Storage.files, files);
}
TEST(Calendar, StreakContinuesBeyondRetention) {
  Model m;
  for (int i = 0; i < 800; ++i) m.add(20000 + i, 30 * 60000, 30);
  EXPECT_EQ(m.streak(20799), 800);
  EXPECT_EQ(m.streak(20800), 800);
  m.add(20802, 30 * 60000, 30);
  EXPECT_EQ(m.streak(20802), 1);
}
TEST_F(CalendarStore, LongStreakSurvivesRoundTrip) {
  Store s;
  s.load();
  for (int i = 0; i < 800; ++i) s.model().add(20000 + i, 30 * 60000, 30);
  ASSERT_TRUE(s.save());
  Store loaded;
  ASSERT_EQ(loaded.load(), Store::Status::Ok);
  EXPECT_EQ(loaded.model().streak(20799), 800);
}
TEST(Calendar, ThresholdIsExactAndTotalsRoundOnlyAfterSumming) {
  Model m;
  m.add(20000, 30 * 60000 - 1, 30);
  EXPECT_FALSE(m.achieved(20000));
  m.add(20000, 1, 30);
  EXPECT_TRUE(m.achieved(20000));
  m.add(20001, 30000, 30);
  m.add(20002, 30000, 30);
  EXPECT_EQ(m.minutes(20001, 20002), 1);
}
TEST(Calendar, MissingOldestDayBreaksCarriedStreak) {
  Model m;
  for (int i = 0; i < 400; ++i) m.add(20000 + i, 30 * 60000, 30);
  m.add(20400, 0, 30);
  for (int i = 401; i < 767; ++i) m.add(20000 + i, 30 * 60000, 30);
  EXPECT_EQ(m.streak(20766), 366);
}
TEST_F(CalendarStore, GoalAndHistoryCommitTogether) {
  Store s;
  s.load();
  s.model().add(20000, 15 * 60000, 30);
  ASSERT_TRUE(s.save());
  s.setGoalIndex(2);
  s.model().changeGoal(20000, 15);
  Storage.failRename = true;
  EXPECT_FALSE(s.save());
  Storage.failRename = false;
  Store old;
  ASSERT_EQ(old.load(), Store::Status::Ok);
  EXPECT_EQ(old.goalIndex(), 4);
  EXPECT_FALSE(old.model().achieved(20000));
  ASSERT_TRUE(s.save());
  Store updated;
  ASSERT_EQ(updated.load(), Store::Status::Ok);
  EXPECT_EQ(updated.goalIndex(), 2);
  EXPECT_TRUE(updated.model().achieved(20000));
}
TEST_F(CalendarStore, AllocationAndUnsupportedDataPreserveFiles) {
  Store s;
  s.load();
  s.model().add(20000, 1000, 30);
  ASSERT_TRUE(s.save());
  auto& bytes = Storage.files["/.crosspoint/reading-calendar-a.bin"];
  bytes[4] = 2;
  const auto original = Storage.files;
  Store future;
  EXPECT_EQ(future.load(), Store::Status::Invalid);
  EXPECT_FALSE(future.save());
  EXPECT_EQ(Storage.files, original);
}

TEST_F(CalendarStore, CheckedAllocationFailureIsReadOnlyAndRetryable) {
  failStatsAllocation = true;
  Store s;
  EXPECT_EQ(s.load(), Store::Status::OutOfMemory);
  EXPECT_EQ(Storage.writes, 0);
  failStatsAllocation = false;
  EXPECT_EQ(s.load(), Store::Status::Missing);
}
TEST_F(CalendarStore, CloseAndReadbackFailuresKeepActiveSnapshot) {
  Store s;
  s.load();
  s.model().add(20000, 60000, 30);
  ASSERT_TRUE(s.save());
  const auto active = Storage.files["/.crosspoint/reading-calendar-a.bin"];
  s.model().add(20000, 60000, 30);
  Storage.failClose = true;
  EXPECT_FALSE(s.save());
  Storage.failClose = false;
  Storage.readBudget = 1;
  EXPECT_FALSE(s.save());
  Storage.readBudget = -1;
  EXPECT_EQ(Storage.files["/.crosspoint/reading-calendar-a.bin"], active);
  Store loaded;
  ASSERT_EQ(loaded.load(), Store::Status::Ok);
  EXPECT_EQ(loaded.model().minutes(20000, 20000), 1);
}
