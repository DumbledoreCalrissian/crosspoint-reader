#pragma once
#include <cstddef>
class HalMemory {
 public:
  struct HeapStats {
    size_t freeBytes, totalBytes, minFreeBytes, largestBlockBytes;
  };
  inline static HeapStats stats{1048576, 1048576, 1048576, 1048576};
  static HeapStats getInternalHeap() { return stats; }
};
