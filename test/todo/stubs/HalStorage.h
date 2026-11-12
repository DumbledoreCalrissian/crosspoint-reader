#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <utility>
constexpr int O_WRONLY = 1, O_CREAT = 2, O_TRUNC = 4;
struct FakeStorage;
class HalFile {
 public:
  std::string* data = nullptr;
  size_t position = 0;
  bool writing = false;
  explicit operator bool() const { return data != nullptr; }
  bool isDirectory() const { return false; }
  size_t size() const { return data ? data->size() : 0; }
  int read() { return data && position < data->size() ? static_cast<unsigned char>((*data)[position++]) : -1; }
  int read(void* buffer, size_t n) {
    if (!data) return -1;
    n = std::min(n, data->size() - position);
    std::memcpy(buffer, data->data() + position, n);
    position += n;
    return n;
  }
  size_t write(const uint8_t* buffer, size_t n);
  size_t write(uint8_t c) { return write(&c, 1); }
  bool close();
};
struct FakeStorage {
  std::map<std::string, std::string> files;
  int writeBudget = -1;
  bool failOpen = false, failClose = false, failRename = false, failRemove = false, failDirectory = false;
  unsigned writes = 0;
  bool exists(const char* path) { return files.count(path); }
  bool ensureDirectoryExists(const char*) { return !failDirectory; }
  HalFile open(const char* path, int flags = 0) {
    if (failOpen || (!flags && !exists(path))) return {};
    if (flags) {
      files[path].clear();
      ++writes;
    }
    return {&files[path], 0, flags != 0};
  }
  bool remove(const char* path) {
    if (failRemove) return false;
    return files.erase(path);
  }
  bool rename(const char* from, const char* to) {
    if (failRename || exists(to) || !exists(from)) return false;
    files[to] = std::move(files[from]);
    files.erase(from);
    return true;
  }
};
inline FakeStorage Storage;
inline size_t HalFile::write(const uint8_t* buffer, size_t n) {
  if (!data) return 0;
  if (Storage.writeBudget >= 0) {
    n = std::min(n, static_cast<size_t>(Storage.writeBudget));
    Storage.writeBudget -= n;
  }
  data->append(reinterpret_cast<const char*>(buffer), n);
  return n;
}
inline bool HalFile::close() {
  data = nullptr;
  return !Storage.failClose;
}
