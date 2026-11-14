add_executable(WeatherTest WeatherTest.cpp
  ${REPO_ROOT}/src/util/WeatherModel.cpp
  ${REPO_ROOT}/src/util/WeatherStore.cpp
  ${REPO_ROOT}/src/util/WeatherParser.cpp
  ${REPO_ROOT}/src/util/WeatherClient.cpp)
target_include_directories(WeatherTest PRIVATE stubs ${REPO_ROOT}/test/reading_stats/stubs ${REPO_ROOT}/src)
find_path(WEATHER_JSON_INCLUDE ArduinoJson.h
  HINTS ${REPO_ROOT}/.pio/libdeps/simulator_picco/ArduinoJson/src ${REPO_ROOT}/.pio/libdeps/picco/ArduinoJson/src)
if(WEATHER_JSON_INCLUDE)
  target_include_directories(WeatherTest SYSTEM PRIVATE ${WEATHER_JSON_INCLUDE})
else()
  # The same header-only dependency/version already pinned by platformio.ini.
  FetchContent_Declare(weather_json GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git GIT_TAG v7.4.2 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(weather_json)
  target_link_libraries(WeatherTest PRIVATE ArduinoJson)
endif()
target_compile_definitions(WeatherTest PRIVATE WEATHER_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/golden-weather-v1.bin")
target_link_libraries(WeatherTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(WeatherTest)
