#pragma once
#include <Stream.h>
#include <fcntl.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <string>
class HalFile {
 public:
  std::string* data = nullptr;
  size_t cursor = 0;
  explicit operator bool() const { return data; }
  bool isOpen() const { return data; }
  size_t fileSize() const { return data ? data->size() : 0; }
  bool seek(size_t pos) {
    cursor = pos;
    return data && pos <= data->size();
  }
  int read(void* out, size_t n) {
    if (!data) return -1;
    n = std::min(n, data->size() - cursor);
    std::memcpy(out, data->data() + cursor, n);
    cursor += n;
    return static_cast<int>(n);
  }
  size_t write(const uint8_t*, size_t);
  bool close();
};
struct TestStorage {
  std::map<std::string, std::string> files;
  int writeBudget = -1;
  int renameBudget = -1;
  bool failOpen = false, failClose = false, failRemove = false;
  bool exists(const char* p) { return files.count(p); }
  bool remove(const char* p) { return !failRemove && files.erase(p); }
  bool rename(const char* a, const char* b) {
    if (renameBudget == 0 || !exists(a) || exists(b)) return false;
    if (renameBudget > 0) --renameBudget;
    files[b] = std::move(files[a]);
    files.erase(a);
    return true;
  }
  HalFile open(const char* p, int flags = O_RDONLY) {
    if (failOpen || (!exists(p) && !(flags & O_CREAT))) return {};
    if (flags & O_TRUNC) files[p].clear();
    return {&files[p], 0};
  }
  bool openFileForRead(const char*, const char* p, HalFile& f) {
    f = open(p);
    return bool(f);
  }
  bool openFileForWrite(const char*, const char* p, HalFile& f) {
    f = open(p, O_WRONLY | O_CREAT | O_TRUNC);
    return bool(f);
  }
  bool replaceFile(const char* a, const char* b) {
    if (exists(b) && !remove(b)) return false;
    return rename(a, b);
  }
};
inline TestStorage Storage;
inline size_t HalFile::write(const uint8_t* bytes, size_t n) {
  if (!data) return 0;
  if (Storage.writeBudget >= 0) {
    n = std::min(n, static_cast<size_t>(Storage.writeBudget));
    Storage.writeBudget -= n;
  }
  data->replace(cursor, n, reinterpret_cast<const char*>(bytes), n);
  cursor += n;
  return n;
}
inline bool HalFile::close() {
  data = nullptr;
  return !Storage.failClose;
}
