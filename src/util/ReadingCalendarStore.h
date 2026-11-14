#pragma once
#include <memory>

#include "ReadingCalendar.h"

namespace readingcalendar {
class Store {
 public:
  enum class Status { Ok, Missing, Recovered, Invalid, StorageError, OutOfMemory };
  Store();
  ~Store();
  Status load();
  bool save();
  uint8_t goalIndex() const { return selectedGoal; }
  bool setGoalIndex(uint8_t value) {
    if (value > 6) return false;
    selectedGoal = value;
    return true;
  }
  bool canSave() const { return writable; }
  Model& model() { return data; }
  const Model& model() const { return data; }

 private:
  enum class Read { Missing, Valid, Corrupt, Unsupported, Unreadable };
  struct Workspace;
  Read read(const char* path, Model& model, uint64_t& generation, uint8_t& goal);
  std::unique_ptr<Workspace> workspace;
  Model data;
  uint64_t generation = 0;
  int active = -1;
  bool writable = false;
  uint8_t selectedGoal = 4;
};
}  // namespace readingcalendar
