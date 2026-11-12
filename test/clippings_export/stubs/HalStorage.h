#pragma once

struct TestStorage {
  bool exists(const char*) const { return true; }
};
inline TestStorage Storage;
