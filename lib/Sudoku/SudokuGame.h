#pragma once
#include "SudokuPack.h"

namespace sudoku {

// One puzzle in progress: givens, entries, notes and an exact undo ring.
// Completion uses the rules (full board, no repeats), not the stored solution;
// the solution is only used for Check and Reveal.
class SudokuGame {
 public:
  static constexpr int UNDO_CAPACITY = 64;
  // Serialized size of encode()/decode().
  static constexpr size_t ENCODED_SIZE = 3 * PACKED_CELLS + 2 * CELLS + 2 + 4 * UNDO_CAPACITY + 12;

  void start(Difficulty difficulty, uint32_t index, uint32_t packCrc, const Puzzle& puzzle);

  bool isGiven(int cell) const { return valid(cell) && givens_[cell] != 0; }
  uint8_t value(int cell) const { return valid(cell) ? values_[cell] : 0; }
  // Bit d-1 set when note d is shown.
  uint16_t notes(int cell) const { return valid(cell) ? notes_[cell] : 0; }

  // Each edit returns true when the board changed (and records an undo step).
  bool setValue(int cell, uint8_t digit);
  // Clears the entry, or the notes when the cell has no entry.
  bool erase(int cell);
  bool toggleNote(int cell, uint8_t digit);
  // Fills the cell from the solution and counts a hint.
  bool reveal(int cell);
  bool undo();
  bool canUndo() const { return undoCount_ > 0; }

  // Counts Check as a hint; isWrong() then marks entries that differ from the solution.
  void countCheck();
  bool hasConflict(int cell) const;
  bool isWrong(int cell) const;
  bool solved() const;
  int filledCount() const;

  uint8_t hints() const { return hints_; }
  Difficulty difficulty() const { return difficulty_; }
  uint32_t index() const { return index_; }
  uint32_t packCrc() const { return packCrc_; }
  uint16_t rating10() const { return rating10_; }

  void encode(uint8_t out[ENCODED_SIZE]) const;
  // Validates every field; leaves the game unchanged on failure.
  bool decode(const uint8_t in[ENCODED_SIZE]);

 private:
  struct UndoStep {
    uint8_t cell;
    uint8_t value;
    uint16_t notes;
  };
  static bool valid(int cell) { return cell >= 0 && cell < CELLS; }
  bool editable(int cell) const { return valid(cell) && givens_[cell] == 0; }
  void remember(int cell);

  uint8_t givens_[CELLS]{};
  uint8_t solution_[CELLS]{};
  uint8_t values_[CELLS]{};
  uint16_t notes_[CELLS]{};
  UndoStep undo_[UNDO_CAPACITY]{};
  uint8_t undoCount_ = 0;
  uint8_t undoNext_ = 0;  // ring slot for the next step
  uint8_t hints_ = 0;
  Difficulty difficulty_ = Difficulty::Easy;
  uint32_t index_ = 0;
  uint32_t packCrc_ = 0;
  uint16_t rating10_ = 0;
};

}  // namespace sudoku
