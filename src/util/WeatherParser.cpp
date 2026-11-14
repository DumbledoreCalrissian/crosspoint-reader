#include "WeatherParser.h"

#include <ArduinoJson.h>
#include <Logging.h>
#include <Memory.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
namespace weather {
namespace {
// One parser-owned arena bounds JSON expansion and avoids per-node heap churn.
class Budget final : public ArduinoJson::Allocator {
  struct alignas(std::max_align_t) Block {
    size_t size;
  };
  static constexpr size_t LIMIT = 16384;
  alignas(std::max_align_t) uint8_t bytes[LIMIT]{};
  size_t used = 0;
  static size_t aligned(size_t n) { return (n + sizeof(Block) - 1) / sizeof(Block) * sizeof(Block); }

 public:
  void* allocate(size_t n) override {
    if (n > LIMIT - sizeof(Block) || aligned(sizeof(Block) + n) > LIMIT - used) return nullptr;
    auto* block = reinterpret_cast<Block*>(bytes + used);
    block->size = n;
    used += aligned(sizeof(Block) + n);
    return block + 1;
  }
  void deallocate(void*) override {}  // The owning Parser frees the entire arena.
  void* reallocate(void* p, size_t n) override {
    if (!p) return allocate(n);
    auto* block = static_cast<Block*>(p) - 1;
    const size_t old = block->size;
    if (n <= old) return p;
    const size_t before = aligned(sizeof(Block) + old);
    if (n <= LIMIT - sizeof(Block) && reinterpret_cast<uint8_t*>(block) + before == bytes + used) {
      const size_t after = aligned(sizeof(Block) + n);
      if (after - before <= LIMIT - used) {
        used += after - before;
        block->size = n;
        return p;
      }
    }
    void* next = allocate(n);
    if (next) memcpy(next, p, old);
    return next;
  }
};
struct Parser {
  Budget budget;
  JsonDocument doc{&budget};
};
struct Reader {
  const char* bytes;
  size_t left;
  int read() {
    if (!left) return -1;
    --left;
    return static_cast<unsigned char>(*bytes++);
  }
  size_t readBytes(char* out, size_t n) {
    if (n > left) n = left;
    memcpy(out, bytes, n);
    bytes += n;
    left -= n;
    return n;
  }
};
bool number(JsonVariantConst v, double lo, double hi, double& result) {
  if (!v.is<double>()) return false;
  result = v.as<double>();
  return std::isfinite(result) && result >= lo && result <= hi;
}
bool text(JsonVariantConst v, char* out, size_t cap, bool empty = false) {
  if (v.isNull() && empty) {
    out[0] = 0;
    return true;
  }
  if (!v.is<const char*>()) return false;
  const auto s = v.as<JsonString>();
  if (s.size() >= cap || !validText({s.c_str(), s.size()}, empty)) return false;
  memcpy(out, s.c_str(), s.size());
  out[s.size()] = 0;
  return true;
}
bool integer(JsonVariantConst v, int lo, int hi, int& result) {
  double value;
  if (!number(v, lo, hi, value) || std::floor(value) != value) return false;
  result = int(value);
  return true;
}
bool percent(JsonVariantConst v, uint8_t& out) {
  if (v.isNull()) {
    out = UNKNOWN_PERCENT;
    return true;
  }
  int value;
  if (!integer(v, 0, 100, value)) return false;
  out = value;
  return true;
}
bool code(JsonVariantConst v, uint8_t& out) {
  if (v.isNull()) {
    out = 255;
    return true;
  }
  int value;
  if (!integer(v, 0, 99, value)) return false;
  out = value;
  return true;
}
std::unique_ptr<Parser> document(const char* bytes, size_t size) {
  if (!bytes || !size || size > 8192) return nullptr;
  auto p = makeUniqueNoThrow<Parser>();
  if (!p) {
    LOG_ERR("WEATHER", "OOM: parser (%u bytes)", unsigned(sizeof(Parser)));
    return nullptr;
  }
  Reader reader{bytes, size};
  if (deserializeJson(p->doc, reader, DeserializationOption::NestingLimit(5)) || !p->doc.is<JsonObject>() ||
      p->doc["error"].as<bool>())
    return nullptr;
  for (int c = reader.read(); c != -1; c = reader.read())
    if (c != ' ' && c != '\n' && c != '\r' && c != '\t') return nullptr;
  return p;
}
}  // namespace
bool parseLocations(const char* bytes, size_t size, Location* out, unsigned& count) {
  count = 0;
  if (!out) return false;
  auto p = document(bytes, size);
  if (!p) return false;
  auto results = p->doc["results"];
  if (results.isNull()) return true;
  if (!results.is<JsonArray>() || results.size() > MAX_PLACES) return false;
  for (JsonObjectConst result : results.as<JsonArrayConst>()) {
    auto& location = out[count];
    double latitude, longitude;
    if (!text(result["name"], location.name, sizeof(location.name)) ||
        !text(result["admin1"], location.region, sizeof(location.region), true) ||
        !text(result["country_code"], location.country, sizeof(location.country)) ||
        !number(result["latitude"], -90, 90, latitude) || !number(result["longitude"], -180, 180, longitude))
      return false;
    location.latitude = std::lround(latitude * 1000000);
    location.longitude = std::lround(longitude * 1000000);
    if (!valid(location)) return false;
    ++count;
  }
  return true;
}
bool parseForecast(const char* bytes, size_t size, Forecast& f) {
  auto p = document(bytes, size);
  if (!p) return false;
  const auto current = p->doc["current"].as<JsonObjectConst>();
  const auto daily = p->doc["daily"].as<JsonObjectConst>();
  if (current.isNull() || daily.isNull()) return false;
  auto same = [](JsonVariantConst v, const char* expected) {
    const char* unit = v.as<const char*>();
    return unit && strcmp(unit, expected) == 0;
  };
  if (!same(p->doc["current_units"]["temperature_2m"], "°C") ||
      !same(p->doc["current_units"]["relative_humidity_2m"], "%") ||
      !same(p->doc["daily_units"]["temperature_2m_max"], "°C") ||
      !same(p->doc["daily_units"]["temperature_2m_min"], "°C") ||
      !same(p->doc["daily_units"]["precipitation_sum"], "mm") ||
      !same(p->doc["daily_units"]["precipitation_probability_max"], "%"))
    return false;
  const auto timestamp = current["time"].as<JsonString>();
  const char* stamp = timestamp.c_str();
  if (!stamp || timestamp.size() != 16 || stamp[10] != 'T' || stamp[13] != ':') return false;
  f.currentDate = isoDay({stamp, 10});
  int hour = 0, minute = 0;
  for (int i = 11; i < 16; ++i) {
    if (i == 13) continue;
    if (stamp[i] < '0' || stamp[i] > '9') return false;
    auto& v = i < 13 ? hour : minute;
    v = v * 10 + stamp[i] - '0';
  }
  if (!f.currentDate || hour > 23 || minute > 59) return false;
  f.currentMinute = hour * 60 + minute;
  int offset = 0, daylight = 0;
  double value;
  if (!integer(current["is_day"], 0, 1, daylight)) return false;
  f.isDay = daylight;
  if (!integer(p->doc["utc_offset_seconds"], -64800, 64800, offset) ||
      !number(current["temperature_2m"], -100, 70, value) || !percent(current["relative_humidity_2m"], f.humidity) ||
      !code(current["weather_code"], f.code))
    return false;
  f.utcOffset = offset;
  f.temperature = std::lround(value * 10);
  f.sampleEpoch = int64_t(f.currentDate) * 86400 + f.currentMinute * 60 - int64_t(offset);
  f.fetchedEpoch = 0;
  for (const char* key : {"time", "temperature_2m_max", "temperature_2m_min", "precipitation_sum",
                          "precipitation_probability_max", "weather_code"})
    if (!daily[key].is<JsonArrayConst>() || daily[key].size() != DAYS) return false;
  for (unsigned i = 0; i < DAYS; ++i) {
    auto& d = f.days[i];
    const auto date = daily["time"][i].as<JsonString>();
    if (!date.c_str()) return false;
    d.date = isoDay({date.c_str(), date.size()});
    if (!number(daily["temperature_2m_max"][i], -100, 70, value)) return false;
    d.high = std::lround(value * 10);
    if (!number(daily["temperature_2m_min"][i], -100, 70, value)) return false;
    d.low = std::lround(value * 10);
    const auto rain = daily["precipitation_sum"][i];
    if (rain.isNull())
      d.rain = UNKNOWN_RAIN;
    else {
      if (!number(rain, 0, 3000, value)) return false;
      d.rain = std::lround(value * 10);
    }
    if (!percent(daily["precipitation_probability_max"][i], d.chance) || !code(daily["weather_code"][i], d.code))
      return false;
  }
  return valid(f);
}
}  // namespace weather
