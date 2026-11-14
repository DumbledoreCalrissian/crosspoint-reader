add_executable(ReadingStatsTest
  ReadingStatsTest.cpp
  stubs/CalendarClock.cpp
  ${REPO_ROOT}/src/util/ReadingCalendar.cpp
  ${REPO_ROOT}/src/util/ReadingCalendarStore.cpp
  ${REPO_ROOT}/src/util/ReadingCalendarTracker.cpp
  ${REPO_ROOT}/src/activities/reader/EpubReadingStats.cpp
  ${REPO_ROOT}/src/util/ReadingStats.cpp
  ${REPO_ROOT}/src/util/ReadingStatsStore.cpp
)
target_include_directories(ReadingStatsTest PRIVATE
  stubs
  ${REPO_ROOT}/src/util
  ${REPO_ROOT}/src
  ${REPO_ROOT}/src/activities/reader
)
target_link_libraries(ReadingStatsTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(ReadingStatsTest)
