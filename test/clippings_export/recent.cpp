#include <RecentBooksStore.h>

#include <cassert>
#include <cstdio>

namespace {
JsonDocument input;
JsonDocument saved;
unsigned writes = 0;
bool failWrite = false;
}  // namespace

bool PersistableStoreBase::readDocFromFile(const char*, JsonDocument& doc) {
  doc.set(input);
  return true;
}

bool PersistableStoreBase::writeDocToFile(const char*, const JsonDocument& doc) {
  ++writes;
  if (failWrite) return false;
  saved.set(doc);
  return true;
}

int main() {
  auto& store = RecentBooksStore::getInstance();
  auto books = input["books"].to<JsonArray>();
  books.add<JsonObject>()["path"] = "/MY CLIPPINGS.TXT";
  books.add<JsonObject>()["path"] = "/books/My Clippings.txt";
  books.add<JsonObject>()["path"] = "/normal.txt";
  assert(store.loadFromFile());
  assert(store.getCount() == 2 && writes == 1);
  assert(saved["books"].size() == 2);
  assert(store.getBooks()[0].path == "/books/My Clippings.txt");

  const auto before = writes;
  store.addBook("/./My Clippings.txt", "Export", "", "");
  assert(store.getCount() == 2 && writes == before);
  store.addBook("/books/another.txt", "Another", "", "");
  assert(store.getCount() == 3 && writes == before + 1);
  store.updatePath("/normal.txt", "/My Clippings.txt", "", "");
  assert(store.getCount() == 2 && writes == before + 2);
  for (const auto& book : store.getBooks()) assert(book.path != "/My Clippings.txt");

  failWrite = true;
  assert(store.loadFromFile());
  assert(store.getCount() == 2);
  assert(store.getBooks()[0].path == "/books/My Clippings.txt");
  std::puts("Recent-book export migration, add/rename exclusion and failed resave PASS");
}
