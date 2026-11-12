#pragma once
#include "TodoModel.h"

class TodoStore {
 public:
  enum class Status { Ok, InvalidText, Full, StorageError, InvalidData, OutOfMemory };
  enum class Change { Add, Edit, Toggle, Delete };
  Status load();
  Status change(Change change, uint32_t id = 0, std::string_view text = {});
  bool canEdit() const { return writable; }
  Status eraseTasks(const uint32_t* ids, size_t count);
  const TodoModel& model() const { return tasks; }

 private:
  TodoModel tasks;
  uint32_t generation = 0;
  int activeSlot = -1;
  bool writable = false;
  static constexpr const char* PATHS[] = {"/.crosspoint/todo-a.json", "/.crosspoint/todo-b.json"};
  static constexpr const char* TEMP = "/.crosspoint/todo.tmp";
  static constexpr size_t MAX_FILE_BYTES = 32768;
  enum class Read { Missing, Empty, Valid, Invalid };
  static Read read(const char* path, TodoModel& model, uint32_t& generation);
  bool save(const TodoModel& model);
};
