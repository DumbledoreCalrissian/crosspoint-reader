#include "SudokuStore.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstring>
#include <string>

namespace sudoku {
namespace {

constexpr uint8_t MAGIC[4] = {'C', 'P', 'S', 'G'};
constexpr uint16_t VERSION = 1;
constexpr int MAX_TRIES = 8;

uint32_t getU32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

void putU32(uint8_t* p, const uint32_t v) {
  for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}

}  // namespace

bool SudokuStore::decodeSnapshot(const uint8_t* data, uint32_t& revision) {
  if (std::memcmp(data, MAGIC, 4) != 0 || (data[4] | (data[5] << 8)) != VERSION ||
      getU32(data + FILE_SIZE - 4) != crc32(data, FILE_SIZE - 4) || data[10] > 1) {
    return false;
  }
  Progress progress;
  const uint8_t* p = data + 11;
  for (int i = 0; i < DIFFICULTIES; ++i, p += 4) progress.nextIndex[i] = getU32(p);
  for (int i = 0; i < DIFFICULTIES; ++i, p += 4) progress.packCrc[i] = getU32(p);
  SudokuGame game;
  const bool active = data[10] == 1;
  if (active && !game.decode(p)) return false;
  revision = getU32(data + 6);
  saved = progress;
  current = game;
  gameActive = active;
  return true;
}

SudokuStore::Read SudokuStore::read(const char* path, uint32_t& revision) {
  if (!Storage.exists(path)) return Read::Missing;
  auto file = Storage.open(path);
  if (!file) return Read::Unreadable;
  if (file.fileSize() != FILE_SIZE) {
    file.close();
    return Read::Invalid;
  }
  uint8_t data[FILE_SIZE];
  const bool complete = file.read(data, FILE_SIZE) == static_cast<int>(FILE_SIZE);
  file.close();
  if (!complete) return Read::Unreadable;
  return decodeSnapshot(data, revision) ? Read::Valid : Read::Invalid;
}

SudokuStore::LoadResult SudokuStore::load() {
  *this = SudokuStore{};
  LoadResult outcome = LoadResult::Ok;
  for (int slot = 0; slot < 2; ++slot) {
    SudokuStore candidate;
    uint32_t revision = 0;
    const Read result = candidate.read(PATHS[slot], revision);
    if (result == Read::Unreadable) {
      LOG_ERR("SUDOKU", "Save unreadable, saving off: %s", PATHS[slot]);
      writable = false;
      outcome = LoadResult::Unreadable;
      continue;
    }
    if (result == Read::Invalid) {
      if (outcome == LoadResult::Ok) outcome = LoadResult::Recovered;
      const std::string aside = std::string(PATHS[slot]) + ".bad";
      if (Storage.exists(aside.c_str())) Storage.remove(aside.c_str());
      if (Storage.rename(PATHS[slot], aside.c_str())) {
        LOG_ERR("SUDOKU", "Damaged save moved aside: %s", PATHS[slot]);
      } else {
        LOG_ERR("SUDOKU", "Damaged save could not be moved: %s", PATHS[slot]);
      }
      continue;
    }
    if (result == Read::Valid && (activeSlot < 0 || revision > generation)) {
      saved = candidate.saved;
      current = candidate.current;
      gameActive = candidate.gameActive;
      generation = revision;
      activeSlot = slot;
    }
  }
  return outcome;
}

bool SudokuStore::save() {
  pendingSave = !writeSnapshot();
  return !pendingSave;
}

bool SudokuStore::writeSnapshot() {
  if (!writable || generation == UINT32_MAX || !Storage.ensureDirectoryExists("/.crosspoint")) return false;
  uint8_t data[FILE_SIZE]{};
  std::memcpy(data, MAGIC, 4);
  data[4] = static_cast<uint8_t>(VERSION);
  data[5] = static_cast<uint8_t>(VERSION >> 8);
  putU32(data + 6, generation + 1);
  data[10] = gameActive ? 1 : 0;
  uint8_t* p = data + 11;
  for (int i = 0; i < DIFFICULTIES; ++i, p += 4) putU32(p, saved.nextIndex[i]);
  for (int i = 0; i < DIFFICULTIES; ++i, p += 4) putU32(p, saved.packCrc[i]);
  if (gameActive) current.encode(p);
  putU32(data + FILE_SIZE - 4, crc32(data, FILE_SIZE - 4));

  auto file = Storage.open(TEMP, O_WRONLY | O_CREAT | O_TRUNC);
  if (!file) return false;
  const bool complete = file.write(data, FILE_SIZE) == FILE_SIZE;
  if (!file.close() || !complete) return false;
  // FAT cannot replace a file atomically: only the inactive snapshot is
  // removed, so the active one survives every failure before the rename.
  const int target = activeSlot == 0 ? 1 : 0;
  if (Storage.exists(PATHS[target]) && !Storage.remove(PATHS[target])) return false;
  if (!Storage.rename(TEMP, PATHS[target])) return false;
  activeSlot = target;
  ++generation;
  return true;
}

SudokuStore::StartResult SudokuStore::startNext(const Difficulty difficulty) {
  PackInfo info;
  const PackStatus status = readPackInfo(difficulty, info);
  if (status == PackStatus::Missing) return StartResult::Missing;
  if (status != PackStatus::Valid) return StartResult::Damaged;
  const int d = static_cast<int>(difficulty);
  if (saved.packCrc[d] != info.recordsCrc) {
    // A different puzzle set: start again from its first puzzle.
    saved.packCrc[d] = info.recordsCrc;
    saved.nextIndex[d] = 0;
  }
  uint32_t index = saved.nextIndex[d] % info.count;
  Puzzle puzzle;
  for (int tries = 0; tries < MAX_TRIES; ++tries, index = (index + 1) % info.count) {
    if (!readPuzzle(difficulty, info, index, puzzle)) {
      LOG_ERR("SUDOKU", "Skipping damaged puzzle %u in %s", static_cast<unsigned>(index),
              difficultyFileName(difficulty));
      continue;
    }
    current.start(difficulty, index, info.recordsCrc, puzzle);
    gameActive = true;
    saved.nextIndex[d] = (index + 1) % info.count;
    return save() ? StartResult::Ok : StartResult::SaveFailed;
  }
  // Move past the damaged run so the next attempt reaches later puzzles.
  saved.nextIndex[d] = index;
  save();
  return StartResult::Damaged;
}

bool SudokuStore::finishGame() {
  // The finished board stays readable in memory; it is no longer saved.
  gameActive = false;
  return save();
}

}  // namespace sudoku
