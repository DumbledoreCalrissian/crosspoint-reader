#pragma once
#include <cstdint>
namespace trustedtime {
inline int64_t weatherTestTime = 0;
inline int64_t trustedNow() { return weatherTestTime; }
}  // namespace trustedtime
