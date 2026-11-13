#pragma once
#include "SudokuPack.h"

namespace sudoku {
struct PublishedPack {
  uint32_t count;
  uint32_t bytes;
  const char* sha256;
};
const PublishedPack& publishedPack(Difficulty difficulty);
const char* partPath(Difficulty difficulty);
// FAT cannot rename over an existing file. A backup holds the old pack until
// the verified replacement is installed; recovery restores an interrupted swap.
bool recoverPack(Difficulty difficulty);
bool installPack(Difficulty difficulty);
// Removes the installed pack, backup and partial, never Sudoku snapshots.
bool deletePack(Difficulty difficulty);
}  // namespace sudoku
