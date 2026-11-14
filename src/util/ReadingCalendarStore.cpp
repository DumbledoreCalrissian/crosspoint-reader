#include "ReadingCalendarStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstring>

namespace readingcalendar {
namespace {
constexpr const char* PATHS[] = {"/.crosspoint/reading-calendar-a.bin", "/.crosspoint/reading-calendar-b.bin"};
constexpr const char* TEMP = "/.crosspoint/reading-calendar-t.bin";
constexpr size_t MAX_BYTES = 28 + Model::CAPACITY * 12;
uint64_t get(const uint8_t* p, unsigned bytes) {
  uint64_t value = 0;
  for (unsigned i = 0; i < bytes; ++i) value |= uint64_t(p[i]) << (8 * i);
  return value;
}
void put(uint8_t* p, uint64_t value, unsigned bytes) {
  for (unsigned i = 0; i < bytes; ++i) p[i] = uint8_t(value >> (8 * i));
}
uint32_t crc(const uint8_t* p, size_t bytes) {
  uint32_t value = UINT32_MAX;
  for (size_t i = 0; i < bytes; ++i) {
    value ^= p[i];
    for (unsigned bit = 0; bit < 8; ++bit) value = (value >> 1) ^ (0xedb88320u & (0u - (value & 1)));
  }
  return ~value;
}
}  // namespace
struct Store::Workspace {
  Model candidate;
  uint8_t bytes[MAX_BYTES];
};
Store::Store() = default;
Store::~Store() = default;

Store::Read Store::read(const char* path, Model& model, uint64_t& revision, uint8_t& goal) {
  if (!Storage.ready()) return Read::Unreadable;
  if (!Storage.exists(path)) return Read::Missing;
  auto file = Storage.open(path);
  if (!file) return Read::Unreadable;
  const auto size = file.fileSize64();
  if (file.isDirectory() || size < 6) return Read::Corrupt;
  auto* p = workspace->bytes;
  const size_t requested = size > MAX_BYTES ? MAX_BYTES : size;
  if (file.read(p, requested) != int(requested)) return Read::Unreadable;
  if (std::memcmp(p, "RDAY", 4)) return Read::Corrupt;
  if (get(p + 4, 2) != 1) return Read::Unsupported;
  if (size < 28 || size > MAX_BYTES) return Read::Corrupt;
  const unsigned count = get(p + 6, 2);
  if (count > Model::CAPACITY || size != 28 + count * 12 || get(p + size - 4, 4) != crc(p, size - 4))
    return Read::Corrupt;
  revision = get(p + 8, 8);
  if (!revision) return Read::Corrupt;
  goal = p[20];
  if (goal > 6 || get(p + 21, 3)) return Read::Corrupt;
  model.clear();
  int32_t previous = 0;
  int32_t firstDate = 0;
  for (unsigned i = 0; i < count; ++i) {
    const auto* entry = p + 24 + i * 12;
    const auto date = int32_t(get(entry, 4));
    const auto milliseconds = uint32_t(get(entry + 4, 4));
    const auto goal = uint16_t(get(entry + 8, 2));
    if (!firstDate) firstDate = date;
    if (int64_t(date) - firstDate >= int(Model::CAPACITY) || date <= previous || date > civilDay(2100, 12, 31) ||
        milliseconds > Model::MAX_READING_MS || get(entry + 10, 2) || !model.add(date, milliseconds, goal))
      return Read::Corrupt;
    previous = date;
  }
  const uint32_t prior = get(p + 16, 4);
  if ((!count && prior) || prior > unsigned(civilDay(2100, 12, 31))) return Read::Corrupt;
  model.restorePreviousStreak(prior);
  return Read::Valid;
}

Store::Status Store::load() {
  writable = false;
  data.clear();
  generation = 0;
  selectedGoal = 4;
  active = -1;
  if (!workspace) workspace = makeUniqueNoThrow<Workspace>();
  if (!workspace) {
    LOG_ERR("CAL", "OOM: calendar workspace (%u bytes)", unsigned(sizeof(Workspace)));
    return Status::OutOfMemory;
  }
  bool corrupt = false;
  bool blocked = false;
  for (int slot = 0; slot < 2; ++slot) {
    uint64_t revision = 0;
    uint8_t goal = 4;
    const auto result = read(PATHS[slot], workspace->candidate, revision, goal);
    corrupt |= result == Read::Corrupt;
    blocked |= result == Read::Unsupported || result == Read::Unreadable;
    if (result == Read::Valid && revision > generation) {
      data = workspace->candidate;
      selectedGoal = goal;
      generation = revision;
      active = slot;
    }
  }
  if (blocked || (corrupt && active < 0)) {
    LOG_ERR("CAL", "Calendar history unavailable; files preserved");
    return Status::Invalid;
  }
  writable = true;
  return active < 0 ? Status::Missing : corrupt ? Status::Recovered : Status::Ok;
}

bool Store::save() {
  if (!writable || generation == UINT64_MAX || !Storage.ensureDirectoryExists("/.crosspoint")) return false;
  auto* p = workspace->bytes;
  std::memcpy(p, "RDAY", 4);
  put(p + 4, 1, 2);
  put(p + 8, generation + 1, 8);
  put(p + 16, data.previousStreak(), 4);
  put(p + 20, selectedGoal, 1);
  put(p + 21, 0, 3);
  unsigned count = 0;
  for (int32_t date = data.latestDay() - int(Model::CAPACITY) + 1; date <= data.latestDay(); ++date) {
    const auto* day = data.find(date);
    if (!day) continue;
    auto* entry = p + 24 + count++ * 12;
    put(entry, day->date, 4);
    put(entry + 4, day->milliseconds, 4);
    put(entry + 8, day->goalMinutes, 2);
    put(entry + 10, 0, 2);
  }
  put(p + 6, count, 2);
  const size_t size = 28 + count * 12;
  put(p + size - 4, crc(p, size - 4), 4);
  auto file = Storage.open(TEMP, O_WRONLY | O_CREAT | O_TRUNC);
  if (!file) return false;
  const bool complete = file.write(p, size) == size;
  if (!file.close() || !complete) return false;
  uint64_t revision = 0;
  uint8_t goal = 4;
  if (read(TEMP, workspace->candidate, revision, goal) != Read::Valid || revision != generation + 1 ||
      goal != selectedGoal ||
      (workspace->candidate.records() != data.records() ||
       workspace->candidate.previousStreak() != data.previousStreak()))
    return false;
  const int target = active == 0 ? 1 : 0;
  if ((Storage.exists(PATHS[target]) && !Storage.remove(PATHS[target])) || !Storage.rename(TEMP, PATHS[target]))
    return false;
  active = target;
  ++generation;
  return true;
}
}  // namespace readingcalendar
