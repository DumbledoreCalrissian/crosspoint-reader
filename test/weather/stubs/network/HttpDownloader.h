#pragma once
#include <cstdint>
#include <functional>
#include <string>
class HttpDownloader {
 public:
  static constexpr uint32_t MIN_TLS_FREE_HEAP = 40000, MIN_TLS_MAX_ALLOC = 20000;
  using DataCallback = std::function<bool(const uint8_t*, size_t)>;
  inline static std::string response, url;
  inline static bool success = true;
  static bool fetchOnce(const std::string& requested, const DataCallback& callback,
                        const std::function<bool()>& abort) {
    url = requested;
    if (abort && abort()) return false;
    for (size_t i = 0; i < response.size(); i += 64) {
      if (abort && abort()) return false;
      const size_t count = std::min<size_t>(64, response.size() - i);
      if (!callback(reinterpret_cast<const uint8_t*>(response.data() + i), count)) return false;
    }
    return success;
  }
};
