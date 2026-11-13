#include <HalStorage.h>
#include <Memory.h>
#include <gtest/gtest.h>

#include <limits>
#include <string>

#include "ReadingStats.h"
#include "ReadingStatsStore.h"

using readingstats::Collector;
using readingstats::Store;
using readingstats::Totals;
using Departure = Collector::Departure;
using Status = Store::Status;

TEST(ReadingCollector, FixedSizeAndNoClockRequired) {
  EXPECT_LE(sizeof(Collector), 32u);
  Collector collector;
  collector.pageShown(0);
  collector.depart(2000, Departure::ForwardTurn);
  EXPECT_EQ(collector.totals(), (Totals{2000, 1}));
}

TEST(ReadingCollector, ExactDwellBoundaries) {
  for (const uint32_t duration : {0U, 1U, 1999U, 2000U, 2001U, 299999U, 300000U, 300001U, 1800000U}) {
    SCOPED_TRACE(duration);
    Collector collector;
    collector.pageShown(100);
    collector.depart(100 + duration, Departure::ForwardTurn);
    const bool qualifies = duration >= 2000 && duration <= 300000;
    EXPECT_EQ(collector.totals(), (Totals{qualifies ? duration : 0, qualifies ? 1U : 0U}));
  }
}

TEST(ReadingCollector, BackwardAndJumpCountTimeWithoutForwardTurns) {
  Collector collector;
  collector.pageShown(10);
  collector.depart(5010, Departure::Navigation);
  collector.pageShown(8000);
  collector.depart(15000, Departure::Navigation);
  EXPECT_EQ(collector.totals(), (Totals{12000, 0}));
}

TEST(ReadingCollector, PauseExcludesMenusAndResumeStartsFresh) {
  Collector collector;
  collector.pageShown(0);
  collector.depart(5000, Departure::Pause);
  EXPECT_FALSE(collector.isTiming());
  EXPECT_EQ(collector.preview(100000), (Totals{5000, 0}));
  collector.pageShown(100000);
  collector.depart(103000, Departure::ForwardTurn);
  EXPECT_EQ(collector.totals(), (Totals{8000, 1}));
}

TEST(ReadingCollector, LastPageAndSleepExitAreIdempotent) {
  Collector collector;
  collector.pageShown(100);
  collector.depart(5100, Departure::Pause);
  collector.depart(5200, Departure::Pause);
  collector.depart(5300, Departure::ForwardTurn);
  EXPECT_EQ(collector.totals(), (Totals{5000, 0}));
}

TEST(ReadingCollector, RepaintAndFailedTurnDoNotSplitDwell) {
  Collector collector;
  collector.pageShown(100);
  collector.pageShown(1500);
  collector.pageShown(2000);
  collector.depart(2600, Departure::ForwardTurn);
  EXPECT_EQ(collector.totals(), (Totals{2500, 1}));
}

TEST(ReadingCollector, PreviewNeverCommitsAndCannotBypassIdleFiltering) {
  Collector collector;
  collector.pageShown(0);
  EXPECT_EQ(collector.preview(60000), (Totals{60000, 0}));
  EXPECT_EQ(collector.preview(300000), (Totals{300000, 0}));
  EXPECT_EQ(collector.preview(300001), Totals{});
  EXPECT_EQ(collector.totals(), Totals{});
  collector.depart(400000, Departure::ForwardTurn);
  EXPECT_EQ(collector.totals(), Totals{});
}

TEST(ReadingCollector, AccumulatesFractionalSecondsWithoutRoundingEachPage) {
  Collector collector;
  for (uint32_t i = 0; i < 10; ++i) {
    collector.pageShown(i * 4000);
    collector.depart(i * 4000 + 2500, Departure::ForwardTurn);
  }
  EXPECT_EQ(collector.totals(), (Totals{25000, 10}));
}

TEST(ReadingCollector, HandlesMonotonicWrap) {
  Collector collector;
  collector.pageShown(UINT32_MAX - 999);
  collector.depart(2000, Departure::ForwardTurn);
  EXPECT_EQ(collector.totals(), (Totals{3000, 1}));
}

TEST(ReadingCollector, SaturatesCounters) {
  const auto limit = std::numeric_limits<uint64_t>::max();
  Collector collector({limit - 1000, limit});
  collector.pageShown(0);
  collector.depart(5000, Departure::ForwardTurn);
  EXPECT_EQ(collector.totals(), (Totals{limit, limit}));
}

class ReadingStore : public testing::Test {
 protected:
  static constexpr const char* BOOK = "/books/日本語.epub";
  Store store;
  void SetUp() override {
    Storage = FakeStorage{};
    failStatsAllocation = false;
    statsAllocations = 0;
    ASSERT_EQ(store.load(BOOK), Status::Missing);
  }
  std::string slot(const char which) const {
    for (const auto& [path, data] : Storage.files) {
      if (path.ends_with(std::string("-") + which + ".bin")) return path;
    }
    return {};
  }
  void twoSnapshots() {
    ASSERT_EQ(store.save({10000, 2}), Status::Ok);
    ASSERT_EQ(store.save({20000, 4}), Status::Ok);
  }
  static std::string unhex(const char* text) {
    const std::string hex(text);
    std::string result;
    for (size_t offset = 0; offset < hex.size(); offset += 2) {
      result.push_back(static_cast<char>(std::stoul(hex.substr(offset, 2), nullptr, 16)));
    }
    return result;
  }
};

TEST_F(ReadingStore, AllocationFailureDoesNotTouchHistoryAndCanRetry) {
  Store other;
  failStatsAllocation = true;
  EXPECT_EQ(other.load(BOOK), Status::OutOfMemory);
  EXPECT_FALSE(other.canSave());
  EXPECT_FALSE(other.canReset());
  EXPECT_TRUE(Storage.files.empty());
  failStatsAllocation = false;
  EXPECT_EQ(other.load(BOOK), Status::Missing);
  EXPECT_EQ(other.save({10000, 2}), Status::Ok);
}

TEST_F(ReadingStore, WorkspaceIsReusedAcrossLoadsAndSaves) {
  const auto allocated = statsAllocations;
  failStatsAllocation = true;
  EXPECT_EQ(store.save({10000, 2}), Status::Ok);
  EXPECT_EQ(store.load(BOOK), Status::Ok);
  EXPECT_EQ(store.reset(), Status::Ok);
  EXPECT_EQ(statsAllocations, allocated);
  failStatsAllocation = false;
}

TEST_F(ReadingStore, EmptyLoadDoesNotCreateFiles) {
  EXPECT_TRUE(store.canSave());
  EXPECT_TRUE(store.canReset());
  EXPECT_EQ(store.totals(), Totals{});
  EXPECT_TRUE(Storage.files.empty());
  EXPECT_EQ(store.save({}), Status::Ok);
  EXPECT_EQ(Storage.writes, 0u);
}

TEST_F(ReadingStore, RoundTripUsesNewestGenerationAndWideCounters) {
  twoSnapshots();
  EXPECT_NE(slot('a'), "");
  EXPECT_NE(slot('b'), "");
  Store reopened;
  ASSERT_EQ(reopened.load(BOOK), Status::Ok);
  EXPECT_EQ(reopened.totals(), (Totals{20000, 4}));
  const Totals wide{UINT64_MAX - 1, UINT64_MAX};
  ASSERT_EQ(reopened.save(wide), Status::Ok);
  ASSERT_EQ(store.load(BOOK), Status::Ok);
  EXPECT_EQ(store.totals(), wide);
}

TEST_F(ReadingStore, NoOpDoesNotWrite) {
  ASSERT_EQ(store.save({10000, 2}), Status::Ok);
  const auto before = Storage.files;
  const auto writes = Storage.writes;
  EXPECT_EQ(store.save({10000, 2}), Status::Ok);
  EXPECT_EQ(Storage.files, before);
  EXPECT_EQ(Storage.writes, writes);
}

TEST_F(ReadingStore, EqualGenerationsWithEqualTotalsAreUnambiguous) {
  twoSnapshots();
  Storage.files[slot('b')] = Storage.files.at(slot('a'));
  EXPECT_EQ(store.load(BOOK), Status::Ok);
  EXPECT_EQ(store.totals(), (Totals{10000, 2}));
  EXPECT_TRUE(store.canSave());
}

TEST_F(ReadingStore, UsesExplicitLittleEndianVersionAndCrc) {
  ASSERT_EQ(store.save({0x0102030405060708ULL, 9}), Status::Ok);
  const auto& bytes = Storage.files.at(slot('a'));
  EXPECT_EQ(bytes.substr(0, 4), "CPRS");
  EXPECT_EQ(static_cast<unsigned char>(bytes[4]), 1);
  EXPECT_EQ(bytes[5], 0);
  EXPECT_EQ(static_cast<unsigned char>(bytes[8]), 1);
  for (int i = 0; i < 8; ++i) EXPECT_EQ(static_cast<unsigned char>(bytes[16 + i]), 8 - i);
  EXPECT_EQ(bytes.substr(32, std::strlen(BOOK)), BOOK);
  EXPECT_EQ(bytes.size(), 36 + std::strlen(BOOK));
}

TEST_F(ReadingStore, MatchesIndependentPythonStructAndZlibFixture) {
  ASSERT_EQ(store.load("/book.epub"), Status::Missing);
  ASSERT_EQ(store.save({10000, 2}), Status::Ok);
  EXPECT_EQ(Storage.files.at(slot('a')),
            unhex("4350525301000a000100000000000000102700000000000002000000000000002f626f6f6b2e65707562d73654b4"));
}

TEST_F(ReadingStore, ZeroGenerationIsInvalidEvenWithCorrectChecksum) {
  ASSERT_EQ(store.load("/book.epub"), Status::Missing);
  ASSERT_EQ(store.save({10000, 2}), Status::Ok);
  Storage.files[slot('a')] =
      unhex("4350525301000a000000000000000000102700000000000002000000000000002f626f6f6b2e65707562e167d690");
  EXPECT_EQ(store.load("/book.epub"), Status::Corrupt);
  EXPECT_FALSE(store.canSave());
}

TEST_F(ReadingStore, ExhaustedGenerationNeverWrapsOrDestroysHistory) {
  ASSERT_EQ(store.load("/book.epub"), Status::Missing);
  ASSERT_EQ(store.save({10000, 2}), Status::Ok);
  Storage.files[slot('a')] =
      unhex("4350525301000a00ffffffffffffffff102700000000000002000000000000002f626f6f6b2e657075627e35a3d1");
  ASSERT_EQ(store.load("/book.epub"), Status::Ok);
  const auto original = Storage.files;
  EXPECT_EQ(store.save({20000, 4}), Status::Exhausted);
  EXPECT_EQ(store.reset(), Status::Exhausted);
  EXPECT_EQ(Storage.files, original);
}

TEST_F(ReadingStore, AllWriteFailuresPreserveActiveSnapshotAndPermitRetry) {
  twoSnapshots();
  const auto original = Storage.files;
  const auto active = slot('b');
  for (int fault = 0; fault < 9; ++fault) {
    SCOPED_TRACE(fault);
    Storage = FakeStorage{};
    Storage.files = original;
    ASSERT_EQ(store.load(BOOK), Status::Ok);
    Storage.writeBudget = fault == 0 ? 8 : -1;
    Storage.failOpen = fault == 1;
    Storage.failClose = fault == 2;
    Storage.failRename = fault == 3;
    Storage.failRemove = fault == 4;
    Storage.failDirectory = fault == 5;
    Storage.corruptWrite = fault == 6;
    Storage.readBudget = fault == 7 ? 4 : -1;
    Storage.mounted = fault != 8;
    EXPECT_EQ(store.save({30000, 6}), Status::StorageError);
    EXPECT_EQ(store.totals(), (Totals{20000, 4}));
    EXPECT_EQ(Storage.files.at(active), original.at(active));
    const auto interrupted = Storage.files;
    Storage = FakeStorage{};
    Storage.files = interrupted;
    Store afterPowerLoss;
    ASSERT_EQ(afterPowerLoss.load(BOOK), Status::Ok);
    EXPECT_EQ(afterPowerLoss.totals(), (Totals{20000, 4}));
    EXPECT_EQ(store.save({30000, 6}), Status::Ok);
    ASSERT_EQ(afterPowerLoss.load(BOOK), Status::Ok);
    EXPECT_EQ(afterPowerLoss.totals(), (Totals{30000, 6}));
  }
}

TEST_F(ReadingStore, EveryShortWriteLeavesPreviousStatsReadable) {
  ASSERT_EQ(store.save({10000, 2}), Status::Ok);
  const auto original = Storage.files;
  const auto size = Storage.files.at(slot('a')).size();
  for (size_t limit = 0; limit < size; ++limit) {
    SCOPED_TRACE(limit);
    Storage.files = original;
    ASSERT_EQ(store.load(BOOK), Status::Ok);
    Storage.writeBudget = static_cast<int>(limit);
    EXPECT_EQ(store.save({20000, 4}), Status::StorageError);
    Storage.writeBudget = -1;
    Store reopened;
    ASSERT_EQ(reopened.load(BOOK), Status::Ok);
    EXPECT_EQ(reopened.totals(), (Totals{10000, 2}));
  }
}

TEST_F(ReadingStore, RecoversOlderValidSnapshotWithoutDestroyingDamage) {
  twoSnapshots();
  Storage.files[slot('b')][20] ^= 1;
  const auto damaged = Storage.files;
  EXPECT_EQ(store.load(BOOK), Status::Recovered);
  EXPECT_EQ(store.totals(), (Totals{10000, 2}));
  EXPECT_TRUE(store.canSave());
  EXPECT_EQ(Storage.files, damaged);
  ASSERT_EQ(store.save({15000, 3}), Status::Ok);
  Store reopened;
  EXPECT_EQ(reopened.load(BOOK), Status::Ok);
  EXPECT_EQ(reopened.totals(), (Totals{15000, 3}));
}

TEST_F(ReadingStore, EveryByteOfRecordIsIntegrityProtected) {
  ASSERT_EQ(store.save({10000, 2}), Status::Ok);
  const auto filename = slot('a');
  const auto original = Storage.files.at(filename);
  for (size_t offset = 0; offset < original.size(); ++offset) {
    SCOPED_TRACE(offset);
    Storage.files[filename] = original;
    Storage.files[filename][offset] ^= 1;
    const auto damaged = Storage.files;
    const auto result = store.load(BOOK);
    EXPECT_TRUE(result == Status::Corrupt || result == Status::Unsupported);
    EXPECT_FALSE(store.canSave());
    EXPECT_EQ(store.save({20000, 4}), Status::StorageError);
    EXPECT_EQ(Storage.files, damaged);
  }
}

TEST_F(ReadingStore, TruncatedAndOversizedRecordsNeverBecomeFreshHistory) {
  ASSERT_EQ(store.save({10000, 2}), Status::Ok);
  const auto filename = slot('a');
  const auto original = Storage.files.at(filename);
  for (size_t size = 0; size < original.size(); ++size) {
    Storage.files[filename] = original.substr(0, size);
    EXPECT_EQ(store.load(BOOK), Status::Corrupt);
    EXPECT_FALSE(store.canSave());
  }
  Storage.files[filename] = original + std::string(Store::MAX_RECORD_BYTES, 'x');
  EXPECT_EQ(store.load(BOOK), Status::Corrupt);
}

TEST_F(ReadingStore, UnknownVersionBlocksEvenWhenAnotherSnapshotIsValid) {
  twoSnapshots();
  Storage.files[slot('b')][4] = 2;
  const auto original = Storage.files;
  EXPECT_EQ(store.load(BOOK), Status::Unsupported);
  EXPECT_FALSE(store.canSave());
  EXPECT_FALSE(store.canReset());
  EXPECT_EQ(store.reset(), Status::StorageError);
  EXPECT_EQ(Storage.files, original);
}

TEST_F(ReadingStore, OversizedFutureFormatCannotBeResetAsCorruption) {
  ASSERT_EQ(store.save({10000, 2}), Status::Ok);
  auto& future = Storage.files[slot('a')];
  future[4] = 2;
  future.append(Store::MAX_RECORD_BYTES, 'x');
  const auto original = Storage.files;
  EXPECT_EQ(store.load(BOOK), Status::Unsupported);
  EXPECT_FALSE(store.canReset());
  EXPECT_EQ(store.reset(), Status::StorageError);
  EXPECT_EQ(Storage.files, original);
}

TEST_F(ReadingStore, ReadFailuresDisableBothSavingAndReset) {
  twoSnapshots();
  const auto original = Storage.files;
  for (int fault = 0; fault < 3; ++fault) {
    Storage = FakeStorage{};
    Storage.files = original;
    Storage.failOpen = fault == 0;
    Storage.readBudget = fault == 1 ? 5 : -1;
    Storage.mounted = fault != 2;
    EXPECT_EQ(store.load(BOOK), Status::StorageError);
    EXPECT_FALSE(store.canSave());
    EXPECT_FALSE(store.canReset());
    EXPECT_EQ(store.save({20000, 4}), Status::StorageError);
    EXPECT_EQ(store.reset(), Status::StorageError);
    EXPECT_EQ(Storage.files, original);
  }
}

TEST_F(ReadingStore, ResetIsPersistedAndFailuresPreservePreviousTotals) {
  twoSnapshots();
  Storage.failRename = true;
  EXPECT_EQ(store.reset(), Status::StorageError);
  EXPECT_EQ(store.totals(), (Totals{20000, 4}));
  Storage.failRename = false;
  ASSERT_EQ(store.reset(), Status::Ok);
  Store reopened;
  ASSERT_EQ(reopened.load(BOOK), Status::Ok);
  EXPECT_EQ(reopened.totals(), Totals{});
  EXPECT_EQ(reopened.save({5000, 1}), Status::Ok);
}

TEST_F(ReadingStore, CorruptHistoryRequiresExplicitReset) {
  twoSnapshots();
  Storage.files[slot('a')] = "broken";
  Storage.files[slot('b')] = "broken";
  const auto original = Storage.files;
  EXPECT_EQ(store.load(BOOK), Status::Corrupt);
  EXPECT_TRUE(store.canReset());
  EXPECT_FALSE(store.canSave());
  EXPECT_EQ(store.save({10000, 2}), Status::StorageError);
  EXPECT_EQ(Storage.files, original);
  ASSERT_EQ(store.reset(), Status::Ok);
  Store reopened;
  EXPECT_EQ(reopened.load(BOOK), Status::Recovered);
  EXPECT_EQ(reopened.totals(), Totals{});
}

TEST_F(ReadingStore, NormalizesAliasesAndPreservesUtf8Identity) {
  ASSERT_EQ(store.save({10000, 2}), Status::Ok);
  EXPECT_EQ(store.load("//books/./other/../日本語.epub"), Status::Ok);
  EXPECT_STREQ(store.bookPath(), BOOK);
  EXPECT_EQ(store.totals(), (Totals{10000, 2}));
  EXPECT_EQ(store.load(store.bookPath()), Status::Ok);
  EXPECT_EQ(store.totals(), (Totals{10000, 2}));
}

TEST_F(ReadingStore, RejectsInvalidAndOverlongPathsWithoutIo) {
  const std::string embeddedNul("/books/a\0.epub", 14);
  for (const std::string& path :
       {std::string{}, std::string("relative.epub"), std::string("/"), std::string("/../a.epub"),
        std::string("/books/"), embeddedNul, "/" + std::string(Store::MAX_PATH_BYTES, 'a')}) {
    SCOPED_TRACE(path);
    EXPECT_EQ(store.load(path), Status::InvalidPath);
    EXPECT_FALSE(store.canSave());
    EXPECT_FALSE(store.canReset());
    EXPECT_EQ(store.reset(), Status::StorageError);
    EXPECT_TRUE(Storage.files.empty());
  }
}

TEST_F(ReadingStore, MaximumPathFitsExactly) {
  const std::string path = "/" + std::string(Store::MAX_PATH_BYTES - 1, 'a');
  ASSERT_EQ(store.load(path), Status::Missing);
  ASSERT_EQ(store.save({10000, 2}), Status::Ok);
  EXPECT_EQ(Storage.files.at(slot('a')).size(), Store::MAX_RECORD_BYTES);
  EXPECT_EQ(store.load(path), Status::Ok);
}

TEST_F(ReadingStore, DuplicateFilenamesRemainIndependent) {
  ASSERT_EQ(store.save({10000, 2}), Status::Ok);
  Store other;
  ASSERT_EQ(other.load("/other/日本語.epub"), Status::Missing);
  ASSERT_EQ(other.save({20000, 3}), Status::Ok);
  ASSERT_EQ(store.load(BOOK), Status::Ok);
  EXPECT_EQ(store.totals(), (Totals{10000, 2}));
  EXPECT_EQ(Storage.files.size(), 2u);
}

TEST_F(ReadingStore, HashCollisionNeverOverwritesAnotherPath) {
  ASSERT_EQ(store.save({10000, 2}), Status::Ok);
  const auto foreign = Storage.files.at(slot('a'));
  Storage.files.clear();
  ASSERT_EQ(store.load("/other.epub"), Status::Missing);
  ASSERT_EQ(store.save({20000, 3}), Status::Ok);
  Storage.files[slot('a')] = foreign;
  const auto original = Storage.files;
  EXPECT_EQ(store.load("/other.epub"), Status::PathConflict);
  EXPECT_FALSE(store.canSave());
  EXPECT_FALSE(store.canReset());
  EXPECT_EQ(store.reset(), Status::StorageError);
  EXPECT_EQ(Storage.files, original);
}

TEST_F(ReadingStore, TemporarySnapshotIsNeverCountedAsCommitted) {
  ASSERT_EQ(store.save({10000, 2}), Status::Ok);
  Storage.failRename = true;
  ASSERT_EQ(store.save({20000, 4}), Status::StorageError);
  ASSERT_NE(slot('t'), "");
  Storage.failRename = false;
  Store reopened;
  ASSERT_EQ(reopened.load(BOOK), Status::Ok);
  EXPECT_EQ(reopened.totals(), (Totals{10000, 2}));
}

TEST_F(ReadingStore, CollectorCanRetryPendingTotalsWithoutDoubleCounting) {
  Collector collector;
  collector.pageShown(0);
  collector.depart(5000, Departure::ForwardTurn);
  Storage.failRename = true;
  EXPECT_EQ(store.save(collector.totals()), Status::StorageError);
  collector.pageShown(6000);
  collector.depart(11000, Departure::Pause);
  Storage.failRename = false;
  ASSERT_EQ(store.save(collector.totals()), Status::Ok);
  Store reopened;
  ASSERT_EQ(reopened.load(BOOK), Status::Ok);
  EXPECT_EQ(reopened.totals(), (Totals{10000, 1}));
}
