#include "BookReadingSummary.h"

#include <FsHelpers.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "BookProgress.h"
#include "ReadingStatsStore.h"
#include "RecentBooksStore.h"

namespace {
BookReadingSummary loadSummary(const std::string& path, readingstats::Store* store) {
  BookReadingSummary summary;
  if (!FsHelpers::hasEpubExtension(path)) return summary;
  summary.percent = loadBookProgress(path);
  if (!store) return summary;
  // Read-only: the reader owns the only writer, and it is closed while Home runs.
  const auto status = store->load(path);
  if (status == readingstats::Store::Status::Ok || status == readingstats::Store::Status::Recovered) {
    summary.timeKnown = true;
    summary.readingMilliseconds = store->totals().readingMilliseconds;
  }
  return summary;
}
}  // namespace

void loadBookReadingSummaries(const std::vector<RecentBook>& books, BookReadingSummary* out, size_t count) {
  count = std::min(count, books.size());
  if (count == 0) return;
  auto store = makeUniqueNoThrow<readingstats::Store>();
  if (!store) LOG_ERR("HOME", "OOM: reading stats");
  for (size_t i = 0; i < count; ++i) out[i] = loadSummary(books[i].path, store.get());
}

void formatReadingTime(const BookReadingSummary& summary, char* out, size_t size) {
  if (size == 0) return;
  out[0] = '\0';
  if (!summary.timeKnown) return;
  const auto minutes = static_cast<unsigned long long>(summary.readingMilliseconds / 60000);
  if (minutes == 0)
    snprintf(out, size, "%s", tr(STR_STATS_TIME_UNDER_MINUTE));
  else if (minutes >= 60)
    snprintf(out, size, tr(STR_STATS_TIME_HOURS), minutes / 60, minutes % 60);
  else
    snprintf(out, size, tr(STR_STATS_TIME_MINUTES), minutes);
}
