#include "SudokuCatalog.h"

#include <HalStorage.h>
#include <Logging.h>
#ifdef SIMULATOR
#include <cstdio>
#include <cstdlib>
#endif

namespace sudoku {
namespace {
constexpr PublishedPack PACKS[DIFFICULTIES] = {
    {100000, 9600512, "db6a334f69581b32325aaae5e3545f375c674604759150396496495958a92ed1"},
    {352643, 33854240, "ad00dcee6bf2e4c70b5bdeb82315087f6aa293beedc2e5be7041b697c1809934"},
    {321592, 30873344, "849c994c430aabeaef5de66f01a2ea7d3b5a549c422ff05f9dcf4dfa4400b8ad"},
    {119681, 11489888, "6798e1dfe8543f4fb6d47d68cf10502e6a9c87354704e1931cc6045634e366bf"},
};
constexpr const char* PARTS[DIFFICULTIES] = {"/games/sudoku/easy.part", "/games/sudoku/medium.part",
                                             "/games/sudoku/hard.part", "/games/sudoku/diabolical.part"};
constexpr const char* BACKUPS[DIFFICULTIES] = {"/games/sudoku/easy.cpsd.bak", "/games/sudoku/medium.cpsd.bak",
                                               "/games/sudoku/hard.cpsd.bak", "/games/sudoku/diabolical.cpsd.bak"};
bool removeIfPresent(const char* path) { return !Storage.exists(path) || Storage.remove(path); }
}  // namespace
const PublishedPack& publishedPack(Difficulty difficulty) {
#ifdef SIMULATOR
  // Local HTTP fixtures only; device builds always use the immutable release pins.
  static PublishedPack fixtures[DIFFICULTIES];
  static char hashes[DIFFICULTIES][65];
  const int d = static_cast<int>(difficulty);
  char key[48];
  std::snprintf(key, sizeof(key), "CROSSPOINT_SIM_SUDOKU_PACK_%d", d);
  if (const char* value = std::getenv(key)) {
    unsigned long bytes;
    if (std::sscanf(value, "%lu:%64[0-9a-f]", &bytes, hashes[d]) == 2 && bytes > 0 && bytes <= 33854240) {
      fixtures[d] = {PACKS[d].count, static_cast<uint32_t>(bytes), hashes[d]};
      return fixtures[d];
    }
  }
#endif
  return PACKS[static_cast<int>(difficulty)];
}
const char* partPath(Difficulty difficulty) { return PARTS[static_cast<int>(difficulty)]; }

bool recoverPack(Difficulty difficulty) {
  const char* backup = BACKUPS[static_cast<int>(difficulty)];
  if (!Storage.exists(backup)) return true;
  const char* path = packPath(difficulty);
  const bool ok = Storage.exists(path) ? Storage.remove(backup) : Storage.rename(backup, path);
  if (!ok) LOG_ERR("SUDOKU", "Cannot recover pack: %s", path);
  return ok;
}

bool installPack(Difficulty difficulty) {
  if (!recoverPack(difficulty)) return false;
  const char* path = packPath(difficulty);
  const char* backup = BACKUPS[static_cast<int>(difficulty)];
  const bool replacing = Storage.exists(path);
  if (replacing && !Storage.rename(path, backup)) return false;
  if (!Storage.rename(partPath(difficulty), path)) {
    if (replacing && !Storage.rename(backup, path)) LOG_ERR("SUDOKU", "Old pack retained at %s", backup);
    return false;
  }
  // A leftover backup is safe: the next open will remove it.
  if (replacing && !Storage.remove(backup)) LOG_ERR("SUDOKU", "Cannot remove backup: %s", backup);
  return true;
}

bool deletePack(Difficulty difficulty) {
  // Remove backup first so recovery cannot resurrect a deleted pack.
  const bool ok = removeIfPresent(BACKUPS[static_cast<int>(difficulty)]) && removeIfPresent(packPath(difficulty)) &&
                  removeIfPresent(partPath(difficulty));
  if (!ok) LOG_ERR("SUDOKU", "Cannot delete pack: %s", packPath(difficulty));
  return ok;
}
}  // namespace sudoku
