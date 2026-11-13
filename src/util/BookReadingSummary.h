#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Saved progress and reading time for one EPUB, read once for Home. Other
// formats report nothing here; their existing displays are unchanged.
struct BookReadingSummary {
  int percent = -1;  // saved position (see loadBookProgress), -1 when unavailable
  bool timeKnown = false;
  uint64_t readingMilliseconds = 0;
};

BookReadingSummary loadBookReadingSummary(const std::string& path);

// "3 h 12 min", "12 min" or "< 1 min"; empty when the time is unknown.
void formatReadingTime(const BookReadingSummary& summary, char* out, size_t size);
