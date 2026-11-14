#include "WeatherStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstring>
namespace weather {
namespace {
constexpr const char* PATHS[] = {"/.crosspoint/weather-a.bin", "/.crosspoint/weather-b.bin"};
constexpr const char* TEMP = "/.crosspoint/weather-t.bin";
constexpr size_t BYTES = 512;
static_assert(sizeof(Snapshot) + 16 <= BYTES - 4, "Weather record must fit the canonical slot");
uint32_t crc(const uint8_t* p, size_t n) {
  uint32_t c = UINT32_MAX;
  for (size_t i = 0; i < n; ++i) {
    c ^= p[i];
    for (int b = 0; b < 8; ++b) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
  }
  return ~c;
}
struct Codec {
  uint8_t* bytes;
  size_t at = 0;
  void put(uint64_t v, unsigned n) {
    for (unsigned i = 0; i < n; ++i) bytes[at++] = v >> (8 * i);
  }
  uint64_t get(unsigned n) {
    uint64_t v = 0;
    for (unsigned i = 0; i < n; ++i) v |= uint64_t(bytes[at++]) << (8 * i);
    return v;
  }
  void putText(const char* p, size_t n) {
    memcpy(bytes + at, p, n);
    at += n;
  }
  void getText(char* p, size_t n) {
    memcpy(p, bytes + at, n);
    at += n;
  }
};
void encode(Codec& c, const Snapshot& s) {
  c.put(s.configured, 1);
  c.put(s.cached, 1);
  c.put(s.fahrenheit, 1);
  c.put(s.location.latitude, 4);
  c.put(s.location.longitude, 4);
  c.putText(s.location.name, sizeof(s.location.name));
  c.putText(s.location.region, sizeof(s.location.region));
  c.putText(s.location.country, sizeof(s.location.country));
  const auto& f = s.forecast;
  c.put(f.currentDate, 4);
  c.put(f.currentMinute, 2);
  c.put(f.temperature, 2);
  c.put(f.utcOffset, 4);
  c.put(f.sampleEpoch, 8);
  c.put(f.fetchedEpoch, 8);
  c.put(f.humidity, 1);
  c.put(f.code, 1);
  c.put(f.isDay, 1);
  for (const auto& d : f.days) {
    c.put(d.date, 4);
    c.put(d.high, 2);
    c.put(d.low, 2);
    c.put(d.rain, 2);
    c.put(d.chance, 1);
    c.put(d.code, 1);
  }
}
bool decode(Codec& c, Snapshot& s) {
  const auto configured = c.get(1), cached = c.get(1), fahrenheit = c.get(1);
  if (configured > 1 || cached > 1 || fahrenheit > 1) return false;
  s.configured = configured;
  s.cached = cached;
  s.fahrenheit = fahrenheit;
  s.location.latitude = int32_t(c.get(4));
  s.location.longitude = int32_t(c.get(4));
  c.getText(s.location.name, sizeof(s.location.name));
  c.getText(s.location.region, sizeof(s.location.region));
  c.getText(s.location.country, sizeof(s.location.country));
  auto& f = s.forecast;
  f.currentDate = int32_t(c.get(4));
  f.currentMinute = c.get(2);
  f.temperature = int16_t(c.get(2));
  f.utcOffset = int32_t(c.get(4));
  f.sampleEpoch = c.get(8);
  f.fetchedEpoch = c.get(8);
  f.humidity = c.get(1);
  f.code = c.get(1);
  f.isDay = c.get(1);
  for (auto& d : f.days) {
    d.date = int32_t(c.get(4));
    d.high = int16_t(c.get(2));
    d.low = int16_t(c.get(2));
    d.rain = c.get(2);
    d.chance = c.get(1);
    d.code = c.get(1);
  }
  return valid(s);
}
}  // namespace
struct Store::Workspace {
  Snapshot candidate;
  uint8_t bytes[BYTES];
};
Store::Store() = default;
Store::~Store() = default;
Store::Read Store::read(const char* path, Snapshot& value, uint64_t& revision) {
  if (!Storage.ready()) return Read::Unreadable;
  if (!Storage.exists(path)) return Read::Missing;
  auto file = Storage.open(path);
  if (!file) return Read::Unreadable;
  const auto size = file.fileSize64();
  if (file.isDirectory() || size < 6) return Read::Corrupt;
  const size_t want = size > BYTES ? BYTES : size;
  auto* p = workspace->bytes;
  if (file.read(p, want) != int(want)) return Read::Unreadable;
  if (memcmp(p, "WTHR", 4)) return Read::Corrupt;
  Codec c{p, 4};
  if (c.get(2) != 1) return Read::Unsupported;
  if (size != BYTES || c.get(2) != 0) return Read::Corrupt;
  revision = c.get(8);
  if (!revision) return Read::Corrupt;
  Codec tail{p, BYTES - 4};
  if (tail.get(4) != crc(p, BYTES - 4) || !decode(c, value)) return Read::Corrupt;
  for (size_t i = c.at; i < BYTES - 4; ++i)
    if (p[i]) return Read::Corrupt;
  return Read::Valid;
}
Store::Status Store::load() {
  loaded = false;
  generation = 0;
  active = -1;
  data.configured = data.cached = data.fahrenheit = false;
  if (!workspace) workspace = makeUniqueNoThrow<Workspace>();
  if (!workspace) {
    LOG_ERR("WEATHER", "OOM: cache workspace (%u bytes)", unsigned(sizeof(Workspace)));
    return Status::OutOfMemory;
  }
  bool corrupt = false, blocked = false;
  for (int slot = 0; slot < 2; ++slot) {
    uint64_t revision = 0;
    const auto r = read(PATHS[slot], workspace->candidate, revision);
    corrupt |= r == Read::Corrupt;
    blocked |= r == Read::Unsupported || r == Read::Unreadable;
    if (r == Read::Valid && revision == generation && generation && data != workspace->candidate) blocked = true;
    if (r == Read::Valid && revision > generation) {
      data = workspace->candidate;
      generation = revision;
      active = slot;
    }
  }
  if (blocked || (corrupt && active < 0)) {
    LOG_ERR("WEATHER", "Weather cache unavailable; files preserved");
    return Status::Invalid;
  }
  loaded = true;
  return active < 0 ? Status::Missing : corrupt ? Status::Recovered : Status::Ok;
}
bool Store::save() {
  const auto fail = [] {
    LOG_ERR("WEATHER", "Weather save failed; active snapshot retained");
    return false;
  };
  if (!canSave() || !valid(data) || !Storage.ensureDirectoryExists("/.crosspoint")) return fail();
  auto* p = workspace->bytes;
  memset(p, 0, BYTES);
  memcpy(p, "WTHR", 4);
  Codec c{p, 4};
  c.put(1, 2);
  c.put(0, 2);
  c.put(generation + 1, 8);
  encode(c, data);
  Codec tail{p, BYTES - 4};
  tail.put(crc(p, BYTES - 4), 4);
  auto file = Storage.open(TEMP, O_WRONLY | O_CREAT | O_TRUNC);
  if (!file) return fail();
  const bool complete = file.write(p, BYTES) == BYTES;
  if (!file.close() || !complete) return fail();
  uint64_t revision = 0;
  if (read(TEMP, workspace->candidate, revision) != Read::Valid || revision != generation + 1 ||
      workspace->candidate != data)
    return fail();
  // read() validated the entire canonical payload and CRC before promotion.
  const int target = active == 0 ? 1 : 0;
  if ((Storage.exists(PATHS[target]) && !Storage.remove(PATHS[target])) || !Storage.rename(TEMP, PATHS[target]))
    return fail();
  active = target;
  ++generation;
  return true;
}
}  // namespace weather
