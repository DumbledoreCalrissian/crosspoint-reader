#pragma once
// In-memory stand-in for HalStorage with failure injection (host tests only).
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <utility>
constexpr int O_RDONLY = 0, O_WRONLY = 1, O_CREAT = 2, O_TRUNC = 4;
class HalFile {
 public:
  std::string* data = nullptr;
  size_t position = 0;
  explicit operator bool() const { return data != nullptr; }
  size_t fileSize() const { return data ? data->size() : 0; }
  bool seek(size_t pos);
  int read(void* buffer, size_t n) {
    if (!data) return -1;
    n = std::min(n, data->size() - std::min(position, data->size()));
    std::memcpy(buffer, data->data() + position, n);
    position += n;
    return static_cast<int>(n);
  }
  size_t write(const uint8_t* buffer, size_t n);
  bool close();
};
struct FakeStorage {
  std::map<std::string, std::string> files;
  int writeBudget = -1;
  bool failOpen = false, failClose = false, failRename = false, failRemove = false, failDirectory = false,
       failSeek = false;
  unsigned writes = 0;
  bool exists(const char* path) { return files.count(path); }
  bool ensureDirectoryExists(const char*) { return !failDirectory; }
  HalFile open(const char* path, int flags = O_RDONLY) {
    if (failOpen || (!flags && !exists(path))) return {};
    if (flags) {
      files[path].clear();
      ++writes;
    }
    return {&files[path], 0};
  }
  bool remove(const char* path) { return !failRemove && files.erase(path); }
  bool rename(const char* from, const char* to) {
    if (failRename || exists(to) || !exists(from)) return false;
    files[to] = std::move(files[from]);
    files.erase(from);
    return true;
  }
};
inline FakeStorage Storage;
inline bool HalFile::seek(size_t pos) {
  if (!data || Storage.failSeek || pos > data->size()) return false;
  position = pos;
  return true;
}
inline size_t HalFile::write(const uint8_t* buffer, size_t n) {
  if (!data) return 0;
  if (Storage.writeBudget >= 0) {
    n = std::min(n, static_cast<size_t>(Storage.writeBudget));
    Storage.writeBudget -= static_cast<int>(n);
  }
  data->append(reinterpret_cast<const char*>(buffer), n);
  return n;
}
inline bool HalFile::close() {
  data = nullptr;
  return !Storage.failClose;
}
