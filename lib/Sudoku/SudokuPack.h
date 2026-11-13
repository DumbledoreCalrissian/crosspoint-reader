#pragma once
#include <cstddef>
#include <cstdint>

// Reader for presolved Sudoku puzzle files (CPSD version 2, see
// docs/sudoku-bank-format.md). Reads one 96-byte record at a time; never loads
// a whole file.
namespace sudoku {

constexpr int CELLS = 81;
constexpr int PACKED_CELLS = 41;
constexpr int DIFFICULTIES = 4;

enum class Difficulty : uint8_t { Easy, Medium, Hard, Diabolical };

struct Puzzle {
  uint8_t givens[CELLS]{};    // 0 = empty
  uint8_t solution[CELLS]{};  // 1-9
  uint16_t rating10 = 0;      // rating x 10
};

struct PackInfo {
  uint32_t count = 0;
  uint32_t recordsCrc = 0;  // identifies the puzzle set
  uint16_t minRating = 0;
  uint16_t maxRating = 0;
};

enum class PackStatus : uint8_t { Missing, Valid, Damaged };

// Standard CRC-32 (zlib): pass the previous result to continue a running CRC.
uint32_t crc32(const uint8_t* data, size_t size, uint32_t previous = 0);

// 81 cells, two per byte, high nibble first; the last low nibble is zero.
void packCells(const uint8_t cells[CELLS], uint8_t out[PACKED_CELLS]);
// False when a value is above maxValue, below minValue, or the unused nibble is set.
bool unpackCells(const uint8_t in[PACKED_CELLS], uint8_t cells[CELLS], uint8_t minValue, uint8_t maxValue);

// Every row, column and box holds 1-9 exactly once.
bool isCompleteGrid(const uint8_t cells[CELLS]);
// Valid solution grid that keeps every given.
bool isConsistent(const uint8_t givens[CELLS], const uint8_t solution[CELLS]);

const char* packPath(Difficulty difficulty);
const char* difficultyFileName(Difficulty difficulty);

PackStatus readPackInfo(Difficulty difficulty, PackInfo& info);
// Reads and checks record `index`. False for a damaged record or read error.
bool readPuzzle(Difficulty difficulty, const PackInfo& info, uint32_t index, Puzzle& puzzle);

}  // namespace sudoku
