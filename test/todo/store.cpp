#include <HalStorage.h>
#include <TodoStore.h>

#include <cassert>
#include <iostream>
#include <memory>

using S = TodoStore::Status;
using C = TodoStore::Change;
static constexpr const char* A = "/.crosspoint/todo-a.json";
static constexpr const char* B = "/.crosspoint/todo-b.json";
int main() {
  auto store = std::make_unique<TodoStore>();
  assert(store->load() == S::Ok && !store->model().size());
  assert(store->change(C::Add, 0, "") == S::InvalidText && Storage.files.empty());
  assert(store->change(C::Add, 0, "Café 日本語 😀 & \"quote\"") == S::Ok);
  const auto first = Storage.files[A];
  auto restarted = std::make_unique<TodoStore>();
  assert(restarted->load() == S::Ok && restarted->model().find(1));
  assert(restarted->change(C::Edit, 1, "edited") == S::Ok);
  assert(restarted->change(C::Toggle, 1) == S::Ok);
  const auto writes = Storage.writes;
  assert(restarted->change(C::Edit, 1, "edited") == S::Ok && Storage.writes == writes);
  auto savedFiles = Storage.files;
  for (int failure = 0; failure < 6; ++failure) {
    Storage.files = savedFiles;
    auto retry = std::make_unique<TodoStore>();
    assert(retry->load() == S::Ok);
    Storage.writeBudget = failure == 0 ? 12 : -1;
    Storage.failOpen = failure == 1;
    Storage.failClose = failure == 2;
    Storage.failRename = failure == 3;
    Storage.failRemove = failure == 4;
    Storage.failDirectory = failure == 5;
    assert(retry->change(C::Edit, 1, "lost") == S::StorageError);
    assert(std::string(retry->model().find(1)->text) == "edited");
    Storage.writeBudget = -1;
    Storage.failOpen = Storage.failClose = Storage.failRename = Storage.failRemove = Storage.failDirectory = false;
    auto recovered = std::make_unique<TodoStore>();
    assert(recovered->load() == S::Ok);
    assert(std::string(recovered->model().find(1)->text) == "edited" && recovered->model().find(1)->completed);
  }
  Storage.files = savedFiles;
  auto deleted = std::make_unique<TodoStore>();
  assert(deleted->load() == S::Ok && deleted->change(C::Delete, 1) == S::Ok);
  auto afterDelete = std::make_unique<TodoStore>();
  assert(afterDelete->load() == S::Ok && !afterDelete->model().size());
  assert(afterDelete->change(C::Add, 0, "new") == S::Ok && afterDelete->model().find(2));
  Storage.files.clear();
  Storage.files[A] = "";
  auto empty = std::make_unique<TodoStore>();
  assert(empty->load() == S::Ok);
  Storage.files[A] = "{broken";
  auto corrupt = std::make_unique<TodoStore>();
  assert(corrupt->load() == S::InvalidData && corrupt->change(C::Add, 0, "test") == S::InvalidData);
  assert(Storage.files[A] == "{broken");
  Storage.files[B] = first;
  assert(corrupt->load() == S::Ok && corrupt->model().size() == 1);
  Storage.files[A] = first;
  Storage.files[B] = first;
  Storage.files[B].replace(Storage.files[B].find("Café"), 4, "bad!");
  assert(corrupt->load() == S::Ok && std::string(corrupt->model().find(1)->text).find("Café") == 0);
  Storage.files[A] = "{\"version\":99}";
  Storage.files.erase(B);
  assert(corrupt->load() == S::InvalidData);
  Storage.files[A] = std::string(32769, ' ');
  assert(corrupt->load() == S::InvalidData);
  Storage.files.clear();
  auto full = std::make_unique<TodoStore>();
  assert(full->load() == S::Ok);
  for (size_t i = 0; i < TodoModel::MAX_TASKS; ++i)
    assert(full->change(C::Add, 0, std::string(TodoModel::MAX_TEXT_BYTES, 'a')) == S::Ok);
  assert(full->change(C::Add, 0, "overflow") == S::Full);
  auto fullRestart = std::make_unique<TodoStore>();
  assert(fullRestart->load() == S::Ok && fullRestart->model().size() == 32);
  Storage.files.clear();
  Storage.files[A] = first + "garbage";
  assert(fullRestart->load() == S::InvalidData);
  Storage.files[A] = "{\"version\":1,\"tasks\":[]}";
  assert(fullRestart->load() == S::InvalidData);
  Storage.files[A] = "[" + std::string(32700, '0') + "]";
  assert(fullRestart->load() == S::InvalidData);
  Storage.files.clear();
  auto batch = std::make_unique<TodoStore>();
  assert(batch->load() == S::Ok);
  for (const char* text : {"first", "second", "third"}) assert(batch->change(C::Add, 0, text) == S::Ok);
  uint32_t ids[] = {1, 3};
  const auto batchSnapshot = Storage.files;
  for (int failure = 0; failure < 6; ++failure) {
    Storage.files = batchSnapshot;
    assert(batch->load() == S::Ok);
    Storage.writeBudget = failure == 0 ? 12 : -1;
    Storage.failOpen = failure == 1;
    Storage.failClose = failure == 2;
    Storage.failRename = failure == 3;
    Storage.failRemove = failure == 4;
    Storage.failDirectory = failure == 5;
    assert(batch->eraseTasks(ids, 2) == S::StorageError && batch->model().size() == 3);
    Storage.writeBudget = -1;
    Storage.failOpen = Storage.failClose = Storage.failRename = Storage.failRemove = Storage.failDirectory = false;
    auto recovered = std::make_unique<TodoStore>();
    assert(recovered->load() == S::Ok && recovered->model().size() == 3);
  }
  Storage.files = batchSnapshot;
  assert(batch->load() == S::Ok);
  const auto beforeDelete = Storage.writes;
  assert(batch->eraseTasks(nullptr, 0) == S::Ok && Storage.writes == beforeDelete);
  assert(batch->eraseTasks(nullptr, 2) == S::InvalidData);
  uint32_t bad[] = {1, 99};
  assert(batch->eraseTasks(bad, 2) == S::InvalidData && batch->model().size() == 3 && Storage.writes == beforeDelete);
  uint32_t duplicate[] = {1, 1};
  assert(batch->eraseTasks(duplicate, 2) == S::InvalidData && batch->model().size() == 3);
  assert(batch->eraseTasks(ids, 2) == S::Ok && Storage.writes == beforeDelete + 1);
  auto afterBatch = std::make_unique<TodoStore>();
  assert(afterBatch->load() == S::Ok && afterBatch->model().size() == 1 && afterBatch->model().find(2));
  assert(afterBatch->change(C::Add, 0, "fourth") == S::Ok && afterBatch->model().find(4));
  std::cout << "Todo persistence checks passed, including failure recovery and full-capacity restart\n";
}
