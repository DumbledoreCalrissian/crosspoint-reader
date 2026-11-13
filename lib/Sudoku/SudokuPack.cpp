#include "SudokuPack.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

namespace sudoku {
namespace {

constexpr size_t HEADER_SIZE = 512;
constexpr size_t RECORD_SIZE = 96;
constexpr uint16_t FORMAT_VERSION = 2;

constexpr const char* PATHS[DIFFICULTIES] = {"/games/sudoku/easy.cpsd", "/games/sudoku/medium.cpsd",
                                             "/games/sudoku/hard.cpsd", "/games/sudoku/diabolical.cpsd"};
constexpr const char* NAMES[DIFFICULTIES] = {"easy.cpsd", "medium.cpsd", "hard.cpsd", "diabolical.cpsd"};

uint16_t readU16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

uint32_t readU32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

int unitOf(int kind, int cell) {
  const int row = cell / 9;
  const int col = cell % 9;
  if (kind == 0) return row;
  if (kind == 1) return col;
  return (row / 3) * 3 + col / 3;
}

bool readExact(HalFile& file, uint8_t* buffer, size_t size) {
  return file.read(buffer, size) == static_cast<int>(size);
}

}  // namespace

uint32_t crc32(const uint8_t* data, const size_t size, const uint32_t previous) {
  uint32_t crc = ~previous;
  for (size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
  }
  return ~crc;
}

void packCells(const uint8_t cells[CELLS], uint8_t out[PACKED_CELLS]) {
  for (int i = 0; i < PACKED_CELLS; ++i) {
    const uint8_t high = cells[2 * i];
    const uint8_t low = 2 * i + 1 < CELLS ? cells[2 * i + 1] : 0;
    out[i] = static_cast<uint8_t>((high << 4) | (low & 0x0F));
  }
}

bool unpackCells(const uint8_t in[PACKED_CELLS], uint8_t cells[CELLS], const uint8_t minValue, const uint8_t maxValue) {
  if (in[PACKED_CELLS - 1] & 0x0F) return false;
  for (int i = 0; i < CELLS; ++i) {
    const uint8_t value = i % 2 == 0 ? in[i / 2] >> 4 : in[i / 2] & 0x0F;
    if (value < minValue || value > maxValue) return false;
    cells[i] = value;
  }
  return true;
}

bool isCompleteGrid(const uint8_t cells[CELLS]) {
  for (int kind = 0; kind < 3; ++kind) {
    uint16_t seen[9]{};
    for (int cell = 0; cell < CELLS; ++cell) {
      const uint8_t value = cells[cell];
      if (value < 1 || value > 9) return false;
      seen[unitOf(kind, cell)] |= static_cast<uint16_t>(1U << (value - 1));
    }
    for (const uint16_t mask : seen) {
      if (mask != 0x1FF) return false;
    }
  }
  return true;
}

bool isConsistent(const uint8_t givens[CELLS], const uint8_t solution[CELLS]) {
  if (!isCompleteGrid(solution)) return false;
  for (int cell = 0; cell < CELLS; ++cell) {
    if (givens[cell] > 9 || (givens[cell] && givens[cell] != solution[cell])) return false;
  }
  return true;
}

const char* packPath(const Difficulty difficulty) { return PATHS[static_cast<int>(difficulty)]; }

const char* difficultyFileName(const Difficulty difficulty) { return NAMES[static_cast<int>(difficulty)]; }

PackStatus readPackInfo(const Difficulty difficulty, PackInfo& info) {
  info = {};
  const char* path = packPath(difficulty);
  if (!Storage.exists(path)) return PackStatus::Missing;
  auto file = Storage.open(path);
  if (!file) return PackStatus::Damaged;
  uint8_t header[HEADER_SIZE];
  const size_t length = file.fileSize();
  const bool read = readExact(file, header, sizeof(header));
  file.close();
  if (!read || std::memcmp(header, "CPSD", 4) != 0 || readU16(header + 4) != FORMAT_VERSION ||
      readU16(header + 6) != HEADER_SIZE || readU16(header + 8) != RECORD_SIZE ||
      header[10] != static_cast<uint8_t>(difficulty) || header[11] != 0 ||
      readU32(header + HEADER_SIZE - 4) != crc32(header, HEADER_SIZE - 4)) {
    LOG_ERR("SUDOKU", "Bad header: %s", path);
    return PackStatus::Damaged;
  }
  const uint32_t count = readU32(header + 12);
  if (count == 0 || static_cast<uint64_t>(length) != HEADER_SIZE + static_cast<uint64_t>(count) * RECORD_SIZE) {
    LOG_ERR("SUDOKU", "Bad length: %s", path);
    return PackStatus::Damaged;
  }
  info.count = count;
  info.recordsCrc = readU32(header + 16);
  info.minRating = readU16(header + 40);
  info.maxRating = readU16(header + 42);
  return PackStatus::Valid;
}

bool readPuzzle(const Difficulty difficulty, const PackInfo& info, const uint32_t index, Puzzle& puzzle) {
  if (index >= info.count) return false;
  auto file = Storage.open(packPath(difficulty));
  if (!file) return false;
  uint8_t record[RECORD_SIZE];
  const bool read =
      file.seek(HEADER_SIZE + static_cast<size_t>(index) * RECORD_SIZE) && readExact(file, record, sizeof(record));
  file.close();
  if (!read || readU32(record + 92) != crc32(record, 92)) return false;
  Puzzle candidate;
  if (!unpackCells(record, candidate.givens, 0, 9) || !unpackCells(record + PACKED_CELLS, candidate.solution, 1, 9) ||
      !isConsistent(candidate.givens, candidate.solution)) {
    return false;
  }
  int givens = 0;
  for (const uint8_t value : candidate.givens) givens += value != 0;
  candidate.rating10 = readU16(record + 88);
  if (record[90] != givens || record[91] != 0 || candidate.rating10 < info.minRating ||
      candidate.rating10 > info.maxRating) {
    return false;
  }
  puzzle = candidate;
  return true;
}

}  // namespace sudoku
