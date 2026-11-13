add_executable(ReadingStatsTest
  ReadingStatsTest.cpp
  ${REPO_ROOT}/src/util/ReadingStats.cpp
  ${REPO_ROOT}/src/util/ReadingStatsStore.cpp
)
target_include_directories(ReadingStatsTest PRIVATE
  stubs
  ${REPO_ROOT}/src/util
)
target_link_libraries(ReadingStatsTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(ReadingStatsTest)
