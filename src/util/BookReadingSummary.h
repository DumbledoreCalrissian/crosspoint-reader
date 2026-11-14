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
  uint64_t forwardTurns = 0;  // valid when timeKnown (same record)
};

// Fills out[i] for books[i], i < count; the rest stay unknown. Read-only; one
// stats workspace serves every book.
void loadBookReadingSummaries(const std::vector<RecentBook>& books, BookReadingSummary* out, size_t count);

// "3 h 12 min", "12 min" or "< 1 min"; empty when the time is unknown.
void formatReadingTime(const BookReadingSummary& summary, char* out, size_t size);
// Compact form for narrow layouts: "3h 12m", "12m" or "<1m"; hoursOnly drops
// the minutes once there is at least an hour ("123h"). Empty when unknown.
void formatShortReadingTime(const BookReadingSummary& summary, char* out, size_t size, bool hoursOnly = false);
