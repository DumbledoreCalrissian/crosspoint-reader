#include "WeatherClient.h"

#include <HalMemory.h>
#include <Logging.h>
#include <Memory.h>
#include <TrustedTime.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "WeatherParser.h"
#include "network/HttpDownloader.h"
namespace weather {
namespace {
constexpr size_t MAX_BYTES = 8192;
struct Response {
  char bytes[MAX_BYTES + 1]{};
  size_t size = 0;
};
std::string encode(std::string_view value) {
  std::string out;
  out.reserve(value.size() * 3);
  static constexpr char HEX_DIGITS[] = "0123456789ABCDEF";
  for (unsigned char c : value) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == '~')
      out += c;
    else {
      out += '%';
      out += HEX_DIGITS[c >> 4];
      out += HEX_DIGITS[c & 15];
    }
  }
  return out;
}
bool tlsMemory() {
  const auto heap = HalMemory::getInternalHeap();
  if (heap.freeBytes < HttpDownloader::MIN_TLS_FREE_HEAP ||
      heap.largestBlockBytes < HttpDownloader::MIN_TLS_MAX_ALLOC) {
    LOG_ERR("WEATHER", "TLS internal heap floor not met (%u free, %u largest)", unsigned(heap.freeBytes),
            unsigned(heap.largestBlockBytes));
    return false;
  }
  return true;
}
Fetch fetch(const std::string& url, Response& response, const std::function<bool()>& abort) {
  if (!tlsMemory()) return Fetch::OutOfMemory;
  bool overflow = false;
  const bool success = HttpDownloader::fetchOnce(
      url,
      [&](const uint8_t* p, size_t n) {
        if (n > MAX_BYTES - response.size) {
          overflow = true;
          return false;
        }
        memcpy(response.bytes + response.size, p, n);
        response.size += n;
        return true;
      },
      abort);
  if (abort && abort()) return Fetch::Cancelled;
  if (overflow) return Fetch::InvalidData;
  if (!success) return Fetch::Network;
  return Fetch::Ok;
}
std::string endpoint(bool geo) {
#ifdef SIMULATOR
  if (const char* base = std::getenv("CROSSPOINT_SIM_WEATHER_BASE"))
    return std::string(base) + (geo ? "/search" : "/forecast");
#endif
  return geo ? "https://geocoding-api.open-meteo.com/v1/search" : "https://api.open-meteo.com/v1/forecast";
}
}  // namespace
Fetch search(std::string_view query, Location* places, unsigned& count, const std::function<bool()>& abort) {
  count = 0;
  if (!validQuery(query)) return Fetch::InvalidData;
  auto response = makeUniqueNoThrow<Response>();
  if (!response) {
    LOG_ERR("WEATHER", "OOM: HTTP response (%u bytes)", unsigned(sizeof(Response)));
    return Fetch::OutOfMemory;
  }
  const auto result =
      fetch(endpoint(true) + "?count=5&language=en&format=json&name=" + encode(query), *response, abort);
  if (result != Fetch::Ok) return result;
  return parseLocations(response->bytes, response->size, places, count) ? Fetch::Ok : Fetch::InvalidData;
}
Fetch refresh(const Location& place, Forecast& forecast, const std::function<bool()>& abort) {
  if (!valid(place)) return Fetch::InvalidData;
  auto response = makeUniqueNoThrow<Response>();
  if (!response) {
    LOG_ERR("WEATHER", "OOM: HTTP response (%u bytes)", unsigned(sizeof(Response)));
    return Fetch::OutOfMemory;
  }
  char coordinates[96];
  snprintf(coordinates, sizeof(coordinates), "?latitude=%.6f&longitude=%.6f", place.latitude / 1000000.0,
           place.longitude / 1000000.0);
  if (!tlsMemory()) return Fetch::OutOfMemory;
  const auto result = fetch(endpoint(false) + coordinates +
                                "&forecast_days=3&timezone=auto&temperature_unit=celsius&precipitation_unit=mm&current="
                                "temperature_2m,relative_humidity_2m,weather_code,is_day&daily=weather_code,"
                                "temperature_2m_max,temperature_2m_min,precipitation_sum,precipitation_probability_max",
                            *response, abort);
  if (result != Fetch::Ok) return result;
  if (!parseForecast(response->bytes, response->size, forecast)) return Fetch::InvalidData;
  const auto now = trustedtime::trustedNow();
  if (now > 0 && uint64_t(now) + 3600 >= forecast.sampleEpoch && uint64_t(now) <= forecast.sampleEpoch + 86400)
    forecast.fetchedEpoch = now;
  return Fetch::Ok;
}
}  // namespace weather
