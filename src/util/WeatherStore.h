#pragma once
#include <memory>

#include "WeatherModel.h"
namespace weather {
class Store {
 public:
  enum class Status { Ok, Missing, Recovered, Invalid, StorageError, OutOfMemory };
  Store();
  ~Store();
  Status load();
  bool save();
  bool available() const { return loaded; }
  bool canSave() const { return loaded && generation != UINT64_MAX; }
  Snapshot& model() { return data; }
  const Snapshot& model() const { return data; }

 private:
  enum class Read { Missing, Valid, Corrupt, Unsupported, Unreadable };
  struct Workspace;
  Read read(const char* path, Snapshot& value, uint64_t& revision);
  Snapshot data;
  std::unique_ptr<Workspace> workspace;
  uint64_t generation = 0;
  int active = -1;
  bool loaded = false;
};
}  // namespace weather
