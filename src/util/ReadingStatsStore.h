#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include "ReadingStats.h"

namespace readingstats {

// One writer per book. Own with the reader; views must not save stale copies.
class Store {
 public:
  enum class Status {
    Ok,
    Missing,
    Recovered,
    InvalidPath,
    Corrupt,
    Unsupported,
    PathConflict,
    StorageError,
    Exhausted,
    OutOfMemory
  };
  static constexpr size_t MAX_PATH_BYTES = 511;
  static constexpr size_t MAX_RECORD_BYTES = 36 + MAX_PATH_BYTES;
  static constexpr const char* DIRECTORY = "/.crosspoint/reading-stats";

  Store();
  ~Store();

  Status load(std::string_view path);
  Status save(Totals totals);
  // Explicit user reset; never replaces an unknown format or another path.
  Status reset();
  Totals totals() const { return persisted; }
  bool canSave() const { return writable; }
  bool canReset() const { return resettable; }
  const char* bookPath() const { return path; }

 private:
  struct Record {
    char path[MAX_PATH_BYTES + 1]{};
    Totals totals;
    uint64_t generation = 0;
  };
  enum class Read { Missing, Valid, Corrupt, Unsupported, Unreadable };
  static bool normalize(std::string_view source, char* destination);
  Read read(const char* filename, Record& record);
  Status write(Totals totals, bool resetting);
  void filename(char slot, char* destination) const;

  struct Workspace;
  std::unique_ptr<Workspace> workspace;

  char path[MAX_PATH_BYTES + 1]{};
  char stem[80]{};
  Totals persisted;
  uint64_t generation = 0;
  int activeSlot = -1;
  bool writable = false;
  bool resettable = false;
};

}  // namespace readingstats
