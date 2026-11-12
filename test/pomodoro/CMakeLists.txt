add_executable(PomodoroTimerTest
  PomodoroTimerTest.cpp
  ${REPO_ROOT}/lib/Pomodoro/PomodoroTimer.cpp
)
target_include_directories(PomodoroTimerTest PRIVATE ${REPO_ROOT}/lib/Pomodoro)
target_link_libraries(PomodoroTimerTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(PomodoroTimerTest)
