#include "BookProgress.h"

#include <Epub.h>
#include <Epub/BookMetadataCache.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <Xtc.h>

#include <algorithm>
#include <functional>

namespace {
uint32_t readLe32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
}  // namespace

namespace {
// progress.bin: spine, page, optional page count (the reader's layout).
int percentFromProgress(const uint8_t* data, const int size, const int spineCount, const bool hasSize,
                        const std::function<float(int, float)>& progress) {
  if (size != 4 && size != 6 && size != 10) return -1;
  const int spine = data[0] | (data[1] << 8);
  const int page = data[2] | (data[3] << 8);
  const int total = size >= 6 ? data[4] | (data[5] << 8) : 0;
  if (spineCount <= 0 || !hasSize) return -1;
  if (spine == spineCount) return 100;
  if (spine > spineCount) return -1;
  const float fraction = total > 0 && page != UINT16_MAX ? std::clamp(float(page) / total, 0.0f, 1.0f) : 0;
  return std::clamp(static_cast<int>(progress(spine, fraction) * 100 + 0.5f), 0, 100);
}
}  // namespace

int loadBookProgress(const std::string& path) {
  uint8_t data[10]{};
  if (FsHelpers::hasReflowableBookExtension(path)) {
    const std::string cachePath = Epub::cachePathFor("/.crosspoint", path);
    // A book never opened has no position, so skip its metadata.
    if (!Storage.exists((cachePath + "/progress.bin").c_str())) return -1;
    if (FsHelpers::hasEpubExtension(path)) {
      // EPUB needs only the cached spine sizes: no archive, protection or CSS work.
      auto metadata = makeUniqueNoThrow<BookMetadataCache>(cachePath);
      if (!metadata) {
        LOG_ERR("HOME", "OOM: progress metadata");
        return -1;
      }
      if (!metadata->load()) return -1;
      HalFile file;
      if (!Storage.openFileForRead("HOME", cachePath + "/progress.bin", file)) return -1;
      const int size = file.read(data, sizeof(data));
      const int spineCount = metadata->getSpineCount();
      return percentFromProgress(
          data, size, spineCount, metadata->getCumulativeSize(spineCount - 1) > 0,
          [&](int spine, float fraction) { return metadata->calculateProgress(spine, fraction); });
    }
    // TXT and Markdown validate their cache through Epub::load.
    auto epub = makeUniqueNoThrow<Epub>(path, "/.crosspoint");
    if (!epub) {
      LOG_ERR("HOME", "OOM: progress metadata");
      return -1;
    }
    if (!epub->load(false, true)) return -1;
    HalFile file;
    if (!Storage.openFileForRead("HOME", epub->getCachePath() + "/progress.bin", file)) return -1;
    const int size = file.read(data, sizeof(data));
    return percentFromProgress(data, size, epub->getSpineItemsCount(), epub->getBookSize() != 0,
                               [&](int spine, float fraction) { return epub->calculateProgress(spine, fraction); });
  }
  if (FsHelpers::hasXtcExtension(path)) {
    auto xtc = makeUniqueNoThrow<Xtc>(path, "/.crosspoint");
    if (!xtc) {
      LOG_ERR("HOME", "OOM: XTC progress metadata");
      return -1;
    }
    if (!xtc->load()) return -1;
    HalFile file;
    if (!Storage.openFileForRead("HOME", xtc->getCachePath() + "/progress.bin", file) || file.read(data, 4) != 4)
      return -1;
    const uint32_t page = readLe32(data);
    if (xtc->getPageCount() == 0) return -1;
    if (page >= xtc->getPageCount()) return 100;
    return xtc->calculateProgress(page);
  }
  return -1;
}
