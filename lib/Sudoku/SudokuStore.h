#pragma once
#include "SudokuGame.h"

namespace sudoku {

// Saved Sudoku state: the next puzzle index per difficulty and at most one game
// in progress. Two fixed-size snapshots alternate like the Todo store, so a
// failed write keeps the previous save. A damaged snapshot (wrong size, CRC or
// contents) is renamed to "<name>.bad" and kept for inspection. A snapshot that
// cannot be opened or read is left alone, and saving stays off until the next
// load, so an I/O error never replaces a save that may still be good.
class SudokuStore {
 public:
  struct Progress {
    uint32_t nextIndex[DIFFICULTIES]{};
    uint32_t packCrc[DIFFICULTIES]{};  // puzzle set the index belongs to
  };
  enum class StartResult : uint8_t { Ok, SaveFailed, Missing, Damaged };
  enum class LoadResult : uint8_t { Ok, Recovered, Unreadable };

  static constexpr const char* PATHS[] = {"/.crosspoint/sudoku-a.bin", "/.crosspoint/sudoku-b.bin"};
  static constexpr const char* TEMP = "/.crosspoint/sudoku.tmp";
  static constexpr size_t FILE_SIZE = 4 + 2 + 4 + 1 + 8 * DIFFICULTIES + SudokuGame::ENCODED_SIZE + 4;

  // Reads both snapshots and keeps the newest intact one. Recovered: a damaged
  // snapshot was moved aside. Unreadable: a snapshot could not be read, so
  // saving is off until the next load.
  LoadResult load();
  bool hasGame() const { return gameActive; }
  SudokuGame& game() { return current; }
  const SudokuGame& game() const { return current; }
  const Progress& progress() const { return saved; }

  // Opens the next puzzle of a difficulty, replacing any game in progress.
  // SaveFailed still starts the game in memory. Damaged moves the next index
  // past the unreadable records, so a later attempt reaches good ones.
  StartResult startNext(Difficulty difficulty);
  // Ends the game in progress (after completion). game() still shows the
  // finished board until the next start or load.
  bool finishGame();
  bool save();
  // True after a failed save until the next successful one.
  bool unsaved() const { return pendingSave; }

 private:
  enum class Read : uint8_t { Missing, Valid, Invalid, Unreadable };
  Read read(const char* path, uint32_t& revision);
  bool writeSnapshot();
  bool decodeSnapshot(const uint8_t* data, uint32_t& revision);

  SudokuGame current;
  Progress saved;
  bool gameActive = false;
  bool writable = true;
  bool pendingSave = false;
  uint32_t generation = 0;
  int activeSlot = -1;
};

}  // namespace sudoku
