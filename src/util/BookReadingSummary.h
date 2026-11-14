#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

struct RecentBook;

// Saved progress and reading time for one EPUB, read once per Home visit.
// Other formats report nothing here; their existing displays are unchanged.
struct BookReadingSummary {
  int percent = -1;  // saved position (see loadBookProgress), -1 when unavailable
  bool timeKnown = false;
  uint64_t readingMilliseconds = 0;
};

// Fills out[i] for books[i], i < count; the rest stay unknown. Read-only; one
// stats workspace serves every book.
void loadBookReadingSummaries(const std::vector<RecentBook>& books, BookReadingSummary* out, size_t count);

// "3 h 12 min", "12 min" or "< 1 min"; empty when the time is unknown.
void formatReadingTime(const BookReadingSummary& summary, char* out, size_t size);
