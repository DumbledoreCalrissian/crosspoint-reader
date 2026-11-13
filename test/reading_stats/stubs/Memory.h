#pragma once

#include <memory>
#include <new>
#include <utility>

inline bool failStatsAllocation = false;
inline unsigned statsAllocations = 0;

template <typename T, typename... Args>
std::unique_ptr<T> makeUniqueNoThrow(Args&&... args) {
  ++statsAllocations;
  if (failStatsAllocation) return nullptr;
  return std::unique_ptr<T>(new (std::nothrow) T(std::forward<Args>(args)...));
}
