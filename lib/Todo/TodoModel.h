#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

class TodoModel {
 public:
  static constexpr size_t MAX_TASKS = 32;
  static constexpr size_t MAX_TEXT_BYTES = 192;
  struct Task {
    uint32_t id = 0;
    bool completed = false;
    char text[MAX_TEXT_BYTES + 1]{};
  };
  enum class Result { Ok, Unchanged, InvalidText, Full, NotFound, IdExhausted };

  void clear() {
    count = 0;
    next = 1;
    for (auto& task : tasks) task = {};
  }
  size_t size() const { return count; }
  const Task* at(size_t index) const { return index < count ? &tasks[index] : nullptr; }
  const Task* find(uint32_t id) const;
  uint32_t nextId() const { return next; }
  Result add(std::string_view text);
  Result edit(uint32_t id, std::string_view text);
  Result toggle(uint32_t id);
  Result erase(uint32_t id);
  bool appendLoaded(uint32_t id, bool completed, std::string_view text);
  bool setNextId(uint32_t id);
  static bool validText(std::string_view text);
  uint32_t checksum(uint32_t generation) const;

 private:
  std::array<Task, MAX_TASKS> tasks{};
  size_t count = 0;
  uint32_t next = 1;
  static std::string_view trimmed(std::string_view text);
};
