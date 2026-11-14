#include <HalMemory.h>
#include <HalStorage.h>
#include <Memory.h>
#include <TrustedTime.h>
#include <gtest/gtest.h>
#include <network/HttpDownloader.h>

#include <cstring>
#include <fstream>
#include <iterator>

#include "util/WeatherClient.h"
#include "util/WeatherModel.h"
#include "util/WeatherParser.h"
#include "util/WeatherStore.h"
using namespace weather;
namespace {
constexpr auto FORECAST =
    R"({"utc_offset_seconds":32400,"current_units":{"temperature_2m":"°C","relative_humidity_2m":"%"},"daily_units":{"temperature_2m_max":"°C","temperature_2m_min":"°C","precipitation_sum":"mm","precipitation_probability_max":"%"},"current":{"time":"2026-11-15T12:00","temperature_2m":14.0,"relative_humidity_2m":64,"weather_code":61,"is_day":1},"daily":{"time":["2026-11-15","2026-11-16","2026-11-17"],"temperature_2m_max":[16,15,14],"temperature_2m_min":[9,8,8],"precipitation_sum":[2.4,0.8,0],"precipitation_probability_max":[70,40,10],"weather_code":[61,51,3]}})";
constexpr auto PLACES =
    R"({"results":[{"name":"Tokyo","admin1":"Tokyo","country_code":"JP","latitude":35.681236,"longitude":139.767125}]})";
Snapshot example() {
  Snapshot s;
  s.configured = s.cached = true;
  strcpy(s.location.name, "Tokyo");
  strcpy(s.location.region, "Tokyo");
  strcpy(s.location.country, "JP");
  s.location.latitude = 35681236;
  s.location.longitude = 139767125;
  EXPECT_TRUE(parseForecast(FORECAST, strlen(FORECAST), s.forecast));
  return s;
}
std::string changed(std::string source, const std::string& from, const std::string& to) {
  auto at = source.find(from);
  EXPECT_NE(at, std::string::npos);
  source.replace(at, from.size(), to);
  return source;
}
}  // namespace
TEST(WeatherModel, DatesAndLeapYears) {
  EXPECT_EQ(isoDay("2024-03-01") - isoDay("2024-02-28"), 2);
  EXPECT_EQ(isoDay("2025-02-29"), 0);
  EXPECT_EQ(isoDay("2026-13-01"), 0);
  EXPECT_EQ(isoDay("2026-11-31"), 0);
  EXPECT_EQ(isoDay("2026-11-15x"), 0);
  unsigned y, m, d;
  for (int day = isoDay("2020-01-01"); day <= isoDay("2100-12-31"); ++day) {
    civilDate(day, y, m, d);
    char text[16];
    snprintf(text, sizeof(text), "%04u-%02u-%02u", y, m, d);
    ASSERT_EQ(isoDay(text), day);
  }
}
TEST(WeatherModel, UnitsAndUtf8) {
  EXPECT_EQ(temperature(140, false), 14);
  EXPECT_EQ(temperature(140, true), 57);
  EXPECT_EQ(temperature(-55, false), -6);
  EXPECT_EQ(temperature(-1000, true), -148);
  EXPECT_TRUE(validText("東京"));
  EXPECT_FALSE(validText("\xc0\xaf"));
  EXPECT_FALSE(validText("City\n"));
  EXPECT_FALSE(validText(std::string_view("a\0b", 3)));
}
TEST(WeatherParser, ValidForecastAndLocation) {
  auto s = example();
  EXPECT_TRUE(valid(s));
  EXPECT_EQ(s.forecast.humidity, 64);
  EXPECT_EQ(s.forecast.days[0].rain, 24);
  Location p[MAX_PLACES];
  unsigned count;
  ASSERT_TRUE(parseLocations(PLACES, strlen(PLACES), p, count));
  ASSERT_EQ(count, 1);
  EXPECT_EQ(p[0], s.location);
}
TEST(WeatherParser, UnknownValuesRemainUnknown) {
  auto body = changed(FORECAST, "\"relative_humidity_2m\":64", "\"relative_humidity_2m\":null");
  body = changed(body, "\"precipitation_sum\":[2.4", "\"precipitation_sum\":[null");
  body = changed(body, "\"precipitation_probability_max\":[70", "\"precipitation_probability_max\":[null");
  Forecast f;
  ASSERT_TRUE(parseForecast(body.data(), body.size(), f));
  EXPECT_EQ(f.humidity, UNKNOWN_PERCENT);
  EXPECT_EQ(f.days[0].rain, UNKNOWN_RAIN);
  EXPECT_EQ(f.days[0].chance, UNKNOWN_PERCENT);
}
TEST(WeatherParser, RejectsWrongUnitsShapeBoundsAndDates) {
  for (auto body : {changed(FORECAST, "\"mm\"", "\"inch\""),
                    changed(FORECAST, "\"temperature_2m\":14.0", "\"temperature_2m\":null"),
                    changed(FORECAST, "\"relative_humidity_2m\":64", "\"relative_humidity_2m\":101"),
                    changed(FORECAST, "2026-11-16", "2026-11-18"), changed(FORECAST, "[70,40,10]", "[70]"),
                    changed(FORECAST, "[2.4,0.8,0]", "[-1,0,0]")}) {
    Forecast f;
    EXPECT_FALSE(parseForecast(body.data(), body.size(), f)) << body;
  }
}
TEST(WeatherParser, RejectsInvalidNamesCoordinatesAndTypes) {
  Location p[MAX_PLACES];
  unsigned count;
  for (auto body : {changed(PLACES, "Tokyo", "Tokyo\\u0000hidden"), changed(PLACES, "35.681236", "91"),
                    std::string("[]"), std::string("{\"results\":[4]}")})
    EXPECT_FALSE(parseLocations(body.data(), body.size(), p, count));
  EXPECT_TRUE(parseLocations("{}", 2, p, count));
  EXPECT_EQ(count, 0);
}
class WeatherStore : public testing::Test {
 protected:
  void SetUp() override {
    Storage = {};
    failStatsAllocation = false;
    HalMemory::stats = {1048576, 1048576, 1048576, 1048576};
    HttpDownloader::success = true;
    HttpDownloader::url.clear();
    HttpDownloader::response.clear();
    trustedtime::weatherTestTime = 0;
  }
};
TEST_F(WeatherStore, GoldenWireFormatAndReadOnlyLoad) {
  Store s;
  ASSERT_EQ(s.load(), Store::Status::Missing);
  s.model() = example();
  ASSERT_TRUE(s.save());
  std::ifstream file(WEATHER_FIXTURE, std::ios::binary);
  std::string golden((std::istreambuf_iterator<char>(file)), {});
  ASSERT_EQ(golden.size(), 512);
  EXPECT_EQ(Storage.files["/.crosspoint/weather-a.bin"], golden);
  const auto before = Storage.files;
  const auto writes = Storage.writes;
  Store restored;
  ASSERT_EQ(restored.load(), Store::Status::Ok);
  EXPECT_EQ(restored.model(), s.model());
  EXPECT_EQ(Storage.files, before);
  EXPECT_EQ(Storage.writes, writes);
}
TEST_F(WeatherStore, RecoveryAndUnits) {
  Store s;
  s.load();
  s.model() = example();
  ASSERT_TRUE(s.save());
  s.model().fahrenheit = true;
  ASSERT_TRUE(s.save());
  Store restored;
  ASSERT_EQ(restored.load(), Store::Status::Ok);
  EXPECT_TRUE(restored.model().fahrenheit);
  Storage.files["/.crosspoint/weather-b.bin"][100] ^= 1;
  Store recovered;
  ASSERT_EQ(recovered.load(), Store::Status::Recovered);
  EXPECT_FALSE(recovered.model().fahrenheit);
}
TEST_F(WeatherStore, EveryShortWritePreservesActive) {
  Store s;
  s.load();
  s.model() = example();
  ASSERT_TRUE(s.save());
  const auto before = Storage.files["/.crosspoint/weather-a.bin"];
  s.model().fahrenheit = true;
  for (int n = 0; n < 512; ++n) {
    Storage.writeBudget = n;
    EXPECT_FALSE(s.save());
    EXPECT_EQ(Storage.files["/.crosspoint/weather-a.bin"], before);
  }
  Storage.writeBudget = -1;
  Store restored;
  ASSERT_EQ(restored.load(), Store::Status::Ok);
  EXPECT_FALSE(restored.model().fahrenheit);
}
TEST_F(WeatherStore, CloseReadbackRenameFailuresPreserveActive) {
  Store s;
  s.load();
  s.model() = example();
  ASSERT_TRUE(s.save());
  s.model().fahrenheit = true;
  Storage.failClose = true;
  EXPECT_FALSE(s.save());
  Storage.failClose = false;
  Storage.readBudget = 1;
  EXPECT_FALSE(s.save());
  Storage.readBudget = -1;
  Storage.failRename = true;
  EXPECT_FALSE(s.save());
  Storage.failRename = false;
  Store restored;
  ASSERT_EQ(restored.load(), Store::Status::Ok);
  EXPECT_FALSE(restored.model().fahrenheit);
}
TEST_F(WeatherStore, UnsupportedAndUnreadableBlockWrites) {
  Store s;
  s.load();
  s.model() = example();
  ASSERT_TRUE(s.save());
  Storage.files["/.crosspoint/weather-a.bin"][4] = 2;
  auto before = Storage.files;
  Store future;
  EXPECT_EQ(future.load(), Store::Status::Invalid);
  EXPECT_FALSE(future.save());
  EXPECT_EQ(before, Storage.files);
  Storage.failOpen = true;
  Store unreadable;
  EXPECT_EQ(unreadable.load(), Store::Status::Invalid);
}
TEST_F(WeatherStore, AllocationFailureAndRetry) {
  failStatsAllocation = true;
  Store s;
  EXPECT_EQ(s.load(), Store::Status::OutOfMemory);
  EXPECT_EQ(Storage.writes, 0);
  failStatsAllocation = false;
  EXPECT_EQ(s.load(), Store::Status::Missing);
}
TEST_F(WeatherStore, FetchesEncodedSearchWithoutDefaultLocation) {
  HttpDownloader::response = PLACES;
  Location p[MAX_PLACES];
  unsigned count;
  ASSERT_EQ(search("New York", p, count, [] { return false; }), Fetch::Ok);
  EXPECT_NE(HttpDownloader::url.find("name=New%20York"), std::string::npos);
  EXPECT_EQ(count, 1);
  EXPECT_EQ(Storage.writes, 0);
}
TEST_F(WeatherStore, FetchBoundsCancellationMemoryAndFailure) {
  Location p[MAX_PLACES];
  unsigned count;
  HttpDownloader::response = std::string(8193, 'x');
  EXPECT_EQ(search("Tokyo", p, count, [] { return false; }), Fetch::InvalidData);
  EXPECT_EQ(search("Tokyo", p, count, [] { return true; }), Fetch::Cancelled);
  HalMemory::stats.freeBytes = 39999;
  EXPECT_EQ(search("Tokyo", p, count, [] { return false; }), Fetch::OutOfMemory);
  HalMemory::stats = {1048576, 1048576, 1048576, 1048576};
  HttpDownloader::response = PLACES;
  HttpDownloader::success = false;
  EXPECT_EQ(search("Tokyo", p, count, [] { return false; }), Fetch::Network);
}
TEST_F(WeatherStore, RefreshRequestsMetricUnitsAndKeepsLocation) {
  auto s = example();
  HttpDownloader::response = FORECAST;
  Forecast result;
  ASSERT_EQ(refresh(s.location, result, [] { return false; }), Fetch::Ok);
  EXPECT_NE(HttpDownloader::url.find("forecast_days=3"), std::string::npos);
  EXPECT_NE(HttpDownloader::url.find("temperature_unit=celsius"), std::string::npos);
  EXPECT_EQ(result, s.forecast);
  EXPECT_EQ(Storage.writes, 0);
}
TEST(WeatherParser, RejectsTrailingGarbageAndEmbeddedNul) {
  Location p[MAX_PLACES];
  unsigned count;
  for (const auto& body :
       {std::string(PLACES) + "x", std::string(PLACES) + "{}", std::string(PLACES) + std::string("\0", 1)})
    EXPECT_FALSE(parseLocations(body.data(), body.size(), p, count));
  const std::string body = std::string(PLACES) + " \n";
  EXPECT_TRUE(parseLocations(body.data(), body.size(), p, count));
}
TEST(WeatherModel, QueriesCountCharactersNotBytes) {
  EXPECT_TRUE(validQuery("東京"));
  EXPECT_FALSE(validQuery("東"));
  EXPECT_FALSE(validQuery(std::string(65, 'a')));
  EXPECT_FALSE(validQuery("a\n"));
}
TEST(WeatherModel, FahrenheitRoundsOnlyOnce) {
  EXPECT_EQ(temperature(-53, true), 22);
  EXPECT_EQ(temperature(-52, true), 23);
  EXPECT_EQ(temperature(-175, true), 1);
}
TEST_F(WeatherStore, FragmentedInternalHeapFailsBeforeHttp) {
  Location places[MAX_PLACES];
  unsigned count;
  HalMemory::stats.largestBlockBytes = 19999;
  EXPECT_EQ(search("Tokyo", places, count, [] { return false; }), Fetch::OutOfMemory);
  EXPECT_TRUE(HttpDownloader::url.empty());
}
TEST(WeatherParser, EmbeddedNullDatesAreInvalid) {
  for (auto payload : {changed(FORECAST, "2026-11-15T12:00", "2026-11-15T12:00\\u0000x"),
                       changed(FORECAST, "2026-11-16", "2026-11-16\\u0000x")}) {
    Forecast f;
    EXPECT_FALSE(parseForecast(payload.data(), payload.size(), f));
  }
}
