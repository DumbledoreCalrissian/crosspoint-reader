#include "ReadingStatsStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>
#include <limits>

namespace readingstats {
namespace {
constexpr uint8_t MAGIC[] = {'C', 'P', 'R', 'S'};
constexpr uint16_t VERSION = 1;
constexpr size_t HEADER_BYTES = 32;
constexpr size_t FILE_PATH_BYTES = 88;

uint64_t getLe(const uint8_t* data, const size_t bytes) {
  uint64_t result = 0;
  for (size_t i = 0; i < bytes; ++i) result |= uint64_t(data[i]) << (8 * i);
  return result;
}

void putLe(uint8_t* data, const uint64_t value, const size_t bytes) {
  for (size_t i = 0; i < bytes; ++i) data[i] = static_cast<uint8_t>(value >> (8 * i));
}

uint32_t checksum(const uint8_t* data, const size_t length) {
  uint32_t crc = UINT32_MAX;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
  }
  return ~crc;
}

uint64_t pathHash(const char* path) {
  uint64_t hash = 14695981039346656037ULL;
  for (; *path; ++path) {
    hash ^= static_cast<unsigned char>(*path);
    hash *= 1099511628211ULL;
  }
  return hash;
}
}  // namespace

struct Store::Workspace {
  Record records[2];
  char normalized[MAX_PATH_BYTES + 1];
  uint8_t bytes[MAX_RECORD_BYTES];
};

Store::Store() = default;
Store::~Store() = default;

bool Store::normalize(const std::string_view source, char* destination) {
  destination[0] = '\0';
  if (source.empty() || source.front() != '/' || source.back() == '/' || source.size() > MAX_PATH_BYTES ||
      source.find('\0') != std::string_view::npos) {
    return false;
  }
  size_t used = 0;
  for (size_t start = 1; start < source.size();) {
    size_t end = source.find('/', start);
    if (end == std::string_view::npos) end = source.size();
    const auto part = source.substr(start, end - start);
    if (part == "..") {
      if (!used) return false;
      while (used && destination[used - 1] != '/') --used;
      if (used) --used;
    } else if (!part.empty() && part != ".") {
      destination[used++] = '/';
      std::memcpy(destination + used, part.data(), part.size());
      used += part.size();
    }
    start = end + 1;
  }
  destination[used] = '\0';
  return used > 1;
}

void Store::filename(const char slot, char* destination) const {
  snprintf(destination, FILE_PATH_BYTES, "%s-%c.bin", stem, slot);
}

Store::Read Store::read(const char* filename, Record& record) {
  if (!Storage.ready()) return Read::Unreadable;
  if (!Storage.exists(filename)) return Read::Missing;
  auto file = Storage.open(filename);
  if (!file) return Read::Unreadable;
  const uint64_t size = file.fileSize64();
  if (file.isDirectory() || size < 6) return Read::Corrupt;
  auto* data = workspace->bytes;
  const size_t requested = size > MAX_RECORD_BYTES ? MAX_RECORD_BYTES : static_cast<size_t>(size);
  if (file.read(data, requested) != static_cast<int>(requested)) return Read::Unreadable;
  if (std::memcmp(data, MAGIC, sizeof(MAGIC)) != 0) return Read::Corrupt;
  if (getLe(data + 4, 2) != VERSION) return Read::Unsupported;
  if (size < HEADER_BYTES + 5 || size > MAX_RECORD_BYTES) return Read::Corrupt;
  const size_t pathLength = static_cast<size_t>(getLe(data + 6, 2));
  if (pathLength > MAX_PATH_BYTES || size != HEADER_BYTES + pathLength + 4 ||
      getLe(data + size - 4, 4) != checksum(data, size - 4)) {
    return Read::Corrupt;
  }
  const std::string_view storedPath(reinterpret_cast<const char*>(data + HEADER_BYTES), pathLength);
  if (!normalize(storedPath, record.path) || storedPath != record.path) return Read::Corrupt;
  record.generation = getLe(data + 8, 8);
  if (!record.generation) return Read::Corrupt;
  record.totals = {getLe(data + 16, 8), getLe(data + 24, 8)};
  return Read::Valid;
}

Store::Status Store::load(const std::string_view source) {
  if (!workspace) {
    workspace = makeUniqueNoThrow<Workspace>();
    if (!workspace) {
      LOG_ERR("RSTATS", "Stats workspace allocation failed: %u bytes", static_cast<unsigned>(sizeof(Workspace)));
      return Status::OutOfMemory;
    }
  }
  const bool validPath = normalize(source, workspace->normalized);
  persisted = {};
  generation = 0;
  activeSlot = -1;
  writable = false;
  resettable = false;
  path[0] = '\0';
  stem[0] = '\0';
  if (!validPath) return Status::InvalidPath;
  std::memcpy(path, workspace->normalized, std::strlen(workspace->normalized) + 1);
  snprintf(stem, sizeof(stem), "%s/%016llx", DIRECTORY, static_cast<unsigned long long>(pathHash(path)));
  auto& records = workspace->records;
  Read outcomes[2];
  for (int slot = 0; slot < 2; ++slot) {
    char file[FILE_PATH_BYTES];
    filename(slot == 0 ? 'a' : 'b', file);
    outcomes[slot] = read(file, records[slot]);
  }
  bool corrupt = false;
  for (int slot = 0; slot < 2; ++slot) {
    switch (outcomes[slot]) {
      case Read::Unreadable:
        LOG_ERR("RSTATS", "Snapshot unreadable; saving disabled");
        return Status::StorageError;
      case Read::Unsupported:
        LOG_ERR("RSTATS", "Unknown stats version; saving disabled");
        return Status::Unsupported;
      case Read::Corrupt:
        corrupt = true;
        break;
      case Read::Missing:
        break;
      case Read::Valid:
        if (std::strcmp(records[slot].path, path) != 0) {
          LOG_ERR("RSTATS", "Path hash collision; saving disabled");
          return Status::PathConflict;
        }
        if (activeSlot >= 0 && records[slot].generation == generation && records[slot].totals != persisted) {
          LOG_ERR("RSTATS", "Conflicting stats generations; saving disabled");
          resettable = true;
          return Status::Corrupt;
        }
        if (activeSlot < 0 || records[slot].generation > generation) {
          activeSlot = slot;
          generation = records[slot].generation;
          persisted = records[slot].totals;
        }
        break;
    }
  }
  resettable = true;
  if (activeSlot < 0 && corrupt) {
    LOG_ERR("RSTATS", "No valid snapshot; explicit reset required");
    return Status::Corrupt;
  }
  writable = true;
  if (activeSlot < 0) return Status::Missing;
  return corrupt ? Status::Recovered : Status::Ok;
}

Store::Status Store::save(const Totals totals) { return write(totals, false); }

Store::Status Store::reset() { return write({}, true); }

Store::Status Store::write(const Totals totals, const bool resetting) {
  if (resetting ? !resettable : !writable) return Status::StorageError;
  if (generation == std::numeric_limits<uint64_t>::max()) return Status::Exhausted;
  if (!resetting && totals == persisted) return Status::Ok;
  if (!Storage.ready() || !Storage.ensureDirectoryExists(DIRECTORY)) {
    LOG_ERR("RSTATS", "Stats directory unavailable");
    return Status::StorageError;
  }
  auto* data = workspace->bytes;
  const size_t pathLength = std::strlen(path);
  const size_t size = HEADER_BYTES + pathLength + 4;
  std::memcpy(data, MAGIC, sizeof(MAGIC));
  putLe(data + 4, VERSION, 2);
  putLe(data + 6, pathLength, 2);
  putLe(data + 8, generation + 1, 8);
  putLe(data + 16, totals.readingMilliseconds, 8);
  putLe(data + 24, totals.forwardTurns, 8);
  std::memcpy(data + HEADER_BYTES, path, pathLength);
  putLe(data + size - 4, checksum(data, size - 4), 4);

  char temporary[FILE_PATH_BYTES];
  filename('t', temporary);
  auto file = Storage.open(temporary, O_WRONLY | O_CREAT | O_TRUNC);
  if (!file) {
    LOG_ERR("RSTATS", "Cannot open stats temporary file");
    return Status::StorageError;
  }
  const bool complete = file.write(data, size) == size;
  if (!file.close() || !complete) {
    LOG_ERR("RSTATS", "Stats write/close failed; previous snapshot retained");
    return Status::StorageError;
  }
  // Read back after close before discarding even the inactive snapshot.
  auto& verified = workspace->records[0];
  if (read(temporary, verified) != Read::Valid || verified.generation != generation + 1 || verified.totals != totals ||
      std::strcmp(verified.path, path) != 0) {
    LOG_ERR("RSTATS", "Stats verification failed");
    return Status::StorageError;
  }
  const int target = activeSlot == 0 ? 1 : 0;
  char destination[FILE_PATH_BYTES];
  filename(target == 0 ? 'a' : 'b', destination);
  if ((Storage.exists(destination) && !Storage.remove(destination)) || !Storage.rename(temporary, destination)) {
    LOG_ERR("RSTATS", "Stats promotion failed; active snapshot retained");
    return Status::StorageError;
  }
  persisted = totals;
  ++generation;
  activeSlot = target;
  writable = true;
  return Status::Ok;
}

}  // namespace readingstats
