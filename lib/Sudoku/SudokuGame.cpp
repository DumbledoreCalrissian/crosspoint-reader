#include "SudokuGame.h"

#include <cstring>

namespace sudoku {
namespace {

constexpr uint16_t ALL_NOTES = 0x1FF;

bool samePeerGroup(const int a, const int b) {
  const int rowA = a / 9, colA = a % 9, rowB = b / 9, colB = b % 9;
  return rowA == rowB || colA == colB || (rowA / 3 == rowB / 3 && colA / 3 == colB / 3);
}

void putU16(uint8_t*& p, const uint16_t v) {
  *p++ = static_cast<uint8_t>(v);
  *p++ = static_cast<uint8_t>(v >> 8);
}

void putU32(uint8_t*& p, const uint32_t v) {
  for (int i = 0; i < 4; ++i) *p++ = static_cast<uint8_t>(v >> (8 * i));
}

uint16_t getU16(const uint8_t*& p) {
  const uint16_t v = static_cast<uint16_t>(p[0] | (p[1] << 8));
  p += 2;
  return v;
}

uint32_t getU32(const uint8_t*& p) {
  uint32_t v = 0;
  for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(p[i]) << (8 * i);
  p += 4;
  return v;
}

}  // namespace

void SudokuGame::start(const Difficulty difficulty, const uint32_t index, const uint32_t packCrc,
                       const Puzzle& puzzle) {
  *this = SudokuGame{};
  std::memcpy(givens_, puzzle.givens, CELLS);
  std::memcpy(solution_, puzzle.solution, CELLS);
  std::memcpy(values_, puzzle.givens, CELLS);
  difficulty_ = difficulty;
  index_ = index;
  packCrc_ = packCrc;
  rating10_ = puzzle.rating10;
}

void SudokuGame::remember(const int cell) {
  undo_[undoNext_] = UndoStep{static_cast<uint8_t>(cell), values_[cell], notes_[cell]};
  undoNext_ = static_cast<uint8_t>((undoNext_ + 1) % UNDO_CAPACITY);
  if (undoCount_ < UNDO_CAPACITY) ++undoCount_;
}

bool SudokuGame::setValue(const int cell, const uint8_t digit) {
  if (!editable(cell) || digit < 1 || digit > 9 || values_[cell] == digit) return false;
  remember(cell);
  values_[cell] = digit;
  return true;
}

bool SudokuGame::erase(const int cell) {
  if (!editable(cell) || (values_[cell] == 0 && notes_[cell] == 0)) return false;
  remember(cell);
  if (values_[cell]) {
    values_[cell] = 0;
  } else {
    notes_[cell] = 0;
  }
  return true;
}

bool SudokuGame::toggleNote(const int cell, const uint8_t digit) {
  if (!editable(cell) || values_[cell] != 0 || digit < 1 || digit > 9) return false;
  remember(cell);
  notes_[cell] ^= static_cast<uint16_t>(1U << (digit - 1));
  return true;
}

bool SudokuGame::reveal(const int cell) {
  if (!editable(cell) || values_[cell] == solution_[cell]) return false;
  remember(cell);
  values_[cell] = solution_[cell];
  if (hints_ < UINT8_MAX) ++hints_;
  return true;
}

bool SudokuGame::undo() {
  if (!undoCount_) return false;
  undoNext_ = static_cast<uint8_t>((undoNext_ + UNDO_CAPACITY - 1) % UNDO_CAPACITY);
  --undoCount_;
  const UndoStep& step = undo_[undoNext_];
  values_[step.cell] = step.value;
  notes_[step.cell] = step.notes;
  return true;
}

void SudokuGame::countCheck() {
  if (hints_ < UINT8_MAX) ++hints_;
}

bool SudokuGame::hasConflict(const int cell) const {
  if (!valid(cell) || !values_[cell]) return false;
  for (int other = 0; other < CELLS; ++other) {
    if (other != cell && values_[other] == values_[cell] && samePeerGroup(cell, other)) return true;
  }
  return false;
}

bool SudokuGame::isWrong(const int cell) const {
  return editable(cell) && values_[cell] != 0 && values_[cell] != solution_[cell];
}

bool SudokuGame::solved() const { return isCompleteGrid(values_); }

int SudokuGame::filledCount() const {
  int count = 0;
  for (const uint8_t value : values_) count += value != 0;
  return count;
}

void SudokuGame::encode(uint8_t out[ENCODED_SIZE]) const {
  uint8_t* p = out;
  packCells(givens_, p);
  p += PACKED_CELLS;
  packCells(solution_, p);
  p += PACKED_CELLS;
  packCells(values_, p);
  p += PACKED_CELLS;
  for (const uint16_t notes : notes_) putU16(p, notes);
  *p++ = undoCount_;
  *p++ = undoNext_;
  for (const UndoStep& step : undo_) {
    *p++ = step.cell;
    *p++ = step.value;
    putU16(p, step.notes);
  }
  *p++ = hints_;
  *p++ = static_cast<uint8_t>(difficulty_);
  putU32(p, index_);
  putU32(p, packCrc_);
  putU16(p, rating10_);
}

bool SudokuGame::decode(const uint8_t in[ENCODED_SIZE]) {
  SudokuGame game;
  const uint8_t* p = in;
  if (!unpackCells(p, game.givens_, 0, 9) || !unpackCells(p + PACKED_CELLS, game.solution_, 1, 9) ||
      !unpackCells(p + 2 * PACKED_CELLS, game.values_, 0, 9) || !isConsistent(game.givens_, game.solution_)) {
    return false;
  }
  p += 3 * PACKED_CELLS;
  for (int cell = 0; cell < CELLS; ++cell) {
    game.notes_[cell] = getU16(p);
    if (game.notes_[cell] > ALL_NOTES) return false;
    if (game.givens_[cell] && (game.values_[cell] != game.givens_[cell] || game.notes_[cell])) return false;
  }
  game.undoCount_ = *p++;
  game.undoNext_ = *p++;
  if (game.undoCount_ > UNDO_CAPACITY || game.undoNext_ >= UNDO_CAPACITY) return false;
  for (UndoStep& step : game.undo_) {
    step.cell = *p++;
    step.value = *p++;
    step.notes = getU16(p);
  }
  for (int i = 0; i < game.undoCount_; ++i) {
    const UndoStep& step = game.undo_[(game.undoNext_ + UNDO_CAPACITY - 1 - i) % UNDO_CAPACITY];
    if (!game.editable(step.cell) || step.value > 9 || step.notes > ALL_NOTES) return false;
  }
  game.hints_ = *p++;
  const uint8_t difficulty = *p++;
  if (difficulty >= DIFFICULTIES) return false;
  game.difficulty_ = static_cast<Difficulty>(difficulty);
  game.index_ = getU32(p);
  game.packCrc_ = getU32(p);
  game.rating10_ = getU16(p);
  *this = game;
  return true;
}

}  // namespace sudoku
