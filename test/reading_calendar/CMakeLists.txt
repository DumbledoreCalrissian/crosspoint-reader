add_executable(ReadingCalendarTest ReadingCalendarTest.cpp
  ${REPO_ROOT}/src/util/ReadingCalendar.cpp
  ${REPO_ROOT}/src/util/ReadingCalendarStore.cpp)
target_include_directories(ReadingCalendarTest PRIVATE
  ${REPO_ROOT}/test/reading_stats/stubs ${REPO_ROOT}/src/util)
target_link_libraries(ReadingCalendarTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(ReadingCalendarTest)
