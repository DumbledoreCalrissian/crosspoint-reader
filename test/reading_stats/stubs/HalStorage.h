#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <utility>

constexpr int O_WRONLY = 1, O_CREAT = 2, O_TRUNC = 4;

class HalFile {
 public:
  std::string* data = nullptr;
  explicit operator bool() const { return data != nullptr; }
  bool isDirectory() const { return false; }
  size_t fileSize() const { return data ? data->size() : 0; }
  uint64_t fileSize64() const { return fileSize(); }
  int read(void* destination, size_t size);
  size_t write(const uint8_t* source, size_t size);
  bool close();
};

struct FakeStorage {
  std::map<std::string, std::string> files;
  int writeBudget = -1;
  int readBudget = -1;
  bool mounted = true;
  bool failOpen = false;
  bool failClose = false;
  bool failRename = false;
  bool failRemove = false;
  bool failDirectory = false;
  bool corruptWrite = false;
  unsigned writes = 0;

  bool ready() const { return mounted; }
  bool exists(const char* path) const { return mounted && files.count(path); }
  bool ensureDirectoryExists(const char*) const { return mounted && !failDirectory; }
  HalFile open(const char* path, const int flags = 0) {
    if (!mounted || failOpen || (!flags && !exists(path))) return {};
    if (flags) {
      files[path].clear();
      ++writes;
    }
    return {&files[path]};
  }
  bool remove(const char* path) { return mounted && !failRemove && files.erase(path); }
  bool rename(const char* from, const char* to) {
    if (!mounted || failRename || !exists(from) || exists(to)) return false;
    files[to] = std::move(files[from]);
    files.erase(from);
    return true;
  }
};

inline FakeStorage Storage;

inline int HalFile::read(void* destination, size_t size) {
  if (!data) return -1;
  size = std::min(size, data->size());
  if (Storage.readBudget >= 0) size = std::min(size, static_cast<size_t>(Storage.readBudget));
  std::memcpy(destination, data->data(), size);
  return static_cast<int>(size);
}

inline size_t HalFile::write(const uint8_t* source, size_t size) {
  if (!data) return 0;
  if (Storage.writeBudget >= 0) size = std::min(size, static_cast<size_t>(Storage.writeBudget));
  data->assign(reinterpret_cast<const char*>(source), size);
  if (Storage.corruptWrite && size) (*data)[size - 1] ^= 1;
  return size;
}

inline bool HalFile::close() {
  data = nullptr;
  return !Storage.failClose;
}
