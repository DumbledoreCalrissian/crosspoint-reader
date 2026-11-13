// Host tests for lib/Sudoku: puzzle files, game rules, undo and saves.
// Usage: sudoku-test <easy.cpsd> <reordered-easy.cpsd> (both written by
// scripts/build_sudoku_bank.py from the same two puzzles, see run.sh).
#include <HalStorage.h>
#include <SudokuStore.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

using namespace sudoku;

#define CHECK(condition)                                                                 \
  do {                                                                                   \
    if (!(condition)) {                                                                  \
      std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition); \
      std::exit(1);                                                                      \
    }                                                                                    \
  } while (0)

namespace {

const char* const PUZZLE_A = "050703060007000800000816000000030000005000100730040086906000204840572093000409000";
const char* const PUZZLE_B = "302401809001000300000000000040708010780502036000090000200609003900000008800070005";
const char* const EASY = "/games/sudoku/easy.cpsd";
const char* const SAVE_A = "/.crosspoint/sudoku-a.bin";
const char* const SAVE_B = "/.crosspoint/sudoku-b.bin";

std::string readFile(const char* path) {
  std::ifstream in(path, std::ios::binary);
  std::stringstream text;
  text << in.rdbuf();
  CHECK(in.good() || in.eof());
  return text.str();
}

bool matches(const uint8_t cells[CELLS], const char* digits) {
  for (int i = 0; i < CELLS; ++i) {
    if (cells[i] != digits[i] - '0') return false;
  }
  return true;
}

bool gameShows(const SudokuGame& game, const char* givens) {
  for (int i = 0; i < CELLS; ++i) {
    if (game.isGiven(i) != (givens[i] != '0') || (game.isGiven(i) && game.value(i) != givens[i] - '0')) return false;
  }
  return true;
}

int firstEmpty(const SudokuGame& game, int after = -1) {
  for (int cell = after + 1; cell < CELLS; ++cell) {
    if (!game.isGiven(cell)) return cell;
  }
  return -1;
}

void resetStorage(const std::string& pack) {
  Storage = FakeStorage{};
  Storage.files[EASY] = pack;
}

void testCrcAndPacking() {
  const char* text = "123456789";
  CHECK(crc32(reinterpret_cast<const uint8_t*>(text), 9) == 0xCBF43926U);
  uint8_t cells[CELLS];
  for (int i = 0; i < CELLS; ++i) cells[i] = static_cast<uint8_t>(i % 10);
  uint8_t packed[PACKED_CELLS];
  packCells(cells, packed);
  CHECK(packed[0] == 0x01 && (packed[PACKED_CELLS - 1] & 0x0F) == 0);
  uint8_t back[CELLS];
  CHECK(unpackCells(packed, back, 0, 9));
  for (int i = 0; i < CELLS; ++i) CHECK(back[i] == cells[i]);
  CHECK(!unpackCells(packed, back, 1, 9));  // contains zeros
  packed[PACKED_CELLS - 1] |= 1;
  CHECK(!unpackCells(packed, back, 0, 9));
}

void testPackReading(const std::string& pack) {
  Storage = FakeStorage{};
  PackInfo info;
  CHECK(readPackInfo(Difficulty::Easy, info) == PackStatus::Missing);

  resetStorage(pack);
  CHECK(readPackInfo(Difficulty::Easy, info) == PackStatus::Valid);
  CHECK(info.count == 2 && info.minRating == 12 && info.maxRating == 12);
  Puzzle puzzle;
  CHECK(readPuzzle(Difficulty::Easy, info, 0, puzzle));
  CHECK(matches(puzzle.givens, PUZZLE_A) && puzzle.rating10 == 12 && isConsistent(puzzle.givens, puzzle.solution));
  CHECK(readPuzzle(Difficulty::Easy, info, 1, puzzle) && matches(puzzle.givens, PUZZLE_B));
  CHECK(!readPuzzle(Difficulty::Easy, info, 2, puzzle));

  Storage.failSeek = true;
  CHECK(!readPuzzle(Difficulty::Easy, info, 1, puzzle));
  Storage.failSeek = false;

  // The same file under another difficulty's name is rejected.
  Storage.files["/games/sudoku/medium.cpsd"] = pack;
  CHECK(readPackInfo(Difficulty::Medium, info) == PackStatus::Damaged);

  for (const size_t offset : {size_t{0}, size_t{4}, size_t{11}, size_t{100}, size_t{509}}) {
    resetStorage(pack);
    Storage.files[EASY][offset] ^= 0x01;
    CHECK(readPackInfo(Difficulty::Easy, info) == PackStatus::Damaged);
  }
  resetStorage(pack);
  Storage.files[EASY].pop_back();
  CHECK(readPackInfo(Difficulty::Easy, info) == PackStatus::Damaged);
  resetStorage(pack.substr(0, 100));
  CHECK(readPackInfo(Difficulty::Easy, info) == PackStatus::Damaged);

  // Record damage affects only that record.
  resetStorage(pack);
  Storage.files[EASY][512 + 96 + 3] ^= 0x10;
  CHECK(readPackInfo(Difficulty::Easy, info) == PackStatus::Valid);
  CHECK(readPuzzle(Difficulty::Easy, info, 0, puzzle));
  CHECK(!readPuzzle(Difficulty::Easy, info, 1, puzzle));
}

Puzzle loadPuzzle(const std::string& pack, uint32_t index) {
  resetStorage(pack);
  PackInfo info;
  Puzzle puzzle;
  CHECK(readPackInfo(Difficulty::Easy, info) == PackStatus::Valid && readPuzzle(Difficulty::Easy, info, index, puzzle));
  return puzzle;
}

void testGameRules(const std::string& pack) {
  const Puzzle puzzle = loadPuzzle(pack, 0);
  auto game = std::make_unique<SudokuGame>();
  game->start(Difficulty::Easy, 0, 7, puzzle);
  CHECK(game->isGiven(1) && game->value(1) == 5);
  CHECK(!game->setValue(1, 4) && !game->erase(1) && !game->toggleNote(1, 4) && !game->reveal(1));
  CHECK(!game->canUndo() && !game->undo());

  const int a = firstEmpty(*game);
  const int b = firstEmpty(*game, a);
  CHECK(!game->setValue(a, 0) && !game->setValue(a, 10) && !game->setValue(-1, 1) && !game->setValue(CELLS, 1));
  CHECK(game->toggleNote(a, 3) && game->toggleNote(a, 9) && game->notes(a) == ((1U << 2) | (1U << 8)));
  CHECK(game->setValue(a, 5));  // 5 repeats the given in row 0
  CHECK(game->value(a) == 5 && game->hasConflict(a) && game->hasConflict(1));
  CHECK(!game->toggleNote(a, 1));  // notes only on empty cells
  CHECK(!game->setValue(a, 5));    // unchanged
  CHECK(game->erase(a) && game->value(a) == 0 && game->notes(a) == ((1U << 2) | (1U << 8)));
  CHECK(game->erase(a) && game->notes(a) == 0 && !game->erase(a));

  // Undo restores value and notes exactly, newest first.
  CHECK(game->undo() && game->notes(a) == ((1U << 2) | (1U << 8)) && game->value(a) == 0);
  CHECK(game->undo() && game->value(a) == 5);
  CHECK(game->undo() && game->value(a) == 0 && game->notes(a) == ((1U << 2) | (1U << 8)));
  CHECK(game->undo() && game->notes(a) == (1U << 2));
  CHECK(game->undo() && game->notes(a) == 0 && !game->canUndo());

  // Check and Reveal.
  const uint8_t right = puzzle.solution[b];
  const uint8_t wrong = right == 9 ? 1 : right + 1;
  CHECK(game->setValue(b, wrong) && game->isWrong(b) && !game->isWrong(a));
  game->countCheck();
  CHECK(game->hints() == 1);
  CHECK(game->reveal(b) && game->value(b) == right && !game->isWrong(b) && game->hints() == 2);
  CHECK(!game->reveal(b));
  CHECK(game->undo() && game->value(b) == wrong && game->hints() == 2);

  // The undo ring keeps the newest 64 steps.
  game->start(Difficulty::Easy, 0, 7, puzzle);
  for (int i = 0; i < 70; ++i) CHECK(game->toggleNote(a, static_cast<uint8_t>(i % 9 + 1)));
  int undone = 0;
  while (game->undo()) ++undone;
  CHECK(undone == SudokuGame::UNDO_CAPACITY && game->notes(a) != 0);

  // Completion by the rules.
  game->start(Difficulty::Easy, 0, 7, puzzle);
  CHECK(!game->solved());
  for (int cell = 0; cell < CELLS; ++cell) {
    if (!game->isGiven(cell)) CHECK(game->setValue(cell, puzzle.solution[cell]));
  }
  CHECK(game->solved() && game->filledCount() == CELLS);
}

void testEncoding(const std::string& pack) {
  const Puzzle puzzle = loadPuzzle(pack, 1);
  auto game = std::make_unique<SudokuGame>();
  game->start(Difficulty::Hard, 1, 0xABCDEF01U, puzzle);
  const int a = firstEmpty(*game);
  CHECK(game->toggleNote(a, 4) && game->setValue(firstEmpty(*game, a), 2) && game->reveal(a));
  game->countCheck();
  uint8_t bytes[SudokuGame::ENCODED_SIZE];
  game->encode(bytes);
  auto copy = std::make_unique<SudokuGame>();
  CHECK(copy->decode(bytes));
  uint8_t again[SudokuGame::ENCODED_SIZE];
  copy->encode(again);
  CHECK(std::memcmp(bytes, again, sizeof(bytes)) == 0);
  CHECK(copy->difficulty() == Difficulty::Hard && copy->index() == 1 && copy->packCrc() == 0xABCDEF01U &&
        copy->hints() == 2 && copy->rating10() == 12);
  CHECK(copy->undo() && copy->undo() && copy->undo() && !copy->undo());

  // Each corruption is rejected and leaves the target unchanged.
  const size_t notesOffset = 3 * PACKED_CELLS;
  const size_t undoOffset = notesOffset + 2 * CELLS + 2;
  struct Corruption {
    size_t offset;
    uint8_t value;
  };
  int given = 0;
  while (!game->isGiven(given)) ++given;
  const Corruption corruptions[] = {
      {PACKED_CELLS, 0x00},                                       // solution cell 0 empty
      {static_cast<size_t>(2 * PACKED_CELLS + given / 2), 0x00},  // given cell entry cleared
      {notesOffset + 1, 0xFF},                                    // notes above 9 bits
      {notesOffset + 2 * CELLS, 65},                              // undo count above capacity
      {notesOffset + 2 * CELLS + 1, 64},                          // undo slot out of range
      {undoOffset, static_cast<uint8_t>(given)},                  // undo step on a given (step 0 is in use)
      {SudokuGame::ENCODED_SIZE - 11, 4},                         // difficulty
  };
  for (const auto& corruption : corruptions) {
    uint8_t damaged[SudokuGame::ENCODED_SIZE];
    std::memcpy(damaged, bytes, sizeof(bytes));
    damaged[corruption.offset] = corruption.value;
    auto target = std::make_unique<SudokuGame>();
    target->start(Difficulty::Easy, 9, 9, puzzle);
    CHECK(!target->decode(damaged));
    CHECK(target->index() == 9);
  }
}

void testStore(const std::string& pack, const std::string& reordered) {
  Storage = FakeStorage{};
  auto store = std::make_unique<SudokuStore>();
  CHECK(store->load() == SudokuStore::LoadResult::Ok && !store->hasGame());
  CHECK(store->startNext(Difficulty::Easy) == SudokuStore::StartResult::Missing && Storage.files.empty());

  resetStorage(pack);
  CHECK(store->load() == SudokuStore::LoadResult::Ok &&
        store->startNext(Difficulty::Easy) == SudokuStore::StartResult::Ok);
  CHECK(store->hasGame() && store->game().index() == 0 && store->progress().nextIndex[0] == 1);
  CHECK(Storage.files.count(SAVE_A) && Storage.files[SAVE_A].size() == SudokuStore::FILE_SIZE);
  const int a = firstEmpty(store->game());
  CHECK(store->game().setValue(a, 4) && store->save() && Storage.files.count(SAVE_B));

  auto restarted = std::make_unique<SudokuStore>();
  CHECK(restarted->load() == SudokuStore::LoadResult::Ok && restarted->hasGame() && restarted->game().value(a) == 4);
  CHECK(restarted->game().undo());

  // Next puzzle wraps around the two-puzzle file.
  CHECK(restarted->startNext(Difficulty::Easy) == SudokuStore::StartResult::Ok);
  CHECK(restarted->game().index() == 1 && restarted->progress().nextIndex[0] == 0);
  const int filledBefore = restarted->game().filledCount();
  CHECK(restarted->finishGame() && !restarted->hasGame() && restarted->game().filledCount() == filledBefore);
  restarted = std::make_unique<SudokuStore>();
  CHECK(restarted->load() == SudokuStore::LoadResult::Ok && !restarted->hasGame() &&
        restarted->progress().nextIndex[0] == 0);

  // A replaced puzzle set restarts at its first puzzle.
  CHECK(restarted->startNext(Difficulty::Easy) == SudokuStore::StartResult::Ok && restarted->game().index() == 0);
  Storage.files[EASY] = reordered;
  CHECK(restarted->startNext(Difficulty::Easy) == SudokuStore::StartResult::Ok);
  CHECK(restarted->game().index() == 0 && gameShows(restarted->game(), PUZZLE_B));

  // Every failed save keeps the previous snapshot.
  const auto savedFiles = Storage.files;
  for (int failure = 0; failure < 6; ++failure) {
    Storage.files = savedFiles;
    auto retry = std::make_unique<SudokuStore>();
    CHECK(retry->load() == SudokuStore::LoadResult::Ok);
    uint8_t before[SudokuGame::ENCODED_SIZE];
    retry->game().encode(before);
    const int cell = firstEmpty(retry->game());
    CHECK(retry->game().toggleNote(cell, 7));
    Storage.writeBudget = failure == 0 ? 100 : -1;
    Storage.failOpen = failure == 1;
    Storage.failClose = failure == 2;
    Storage.failRemove = failure == 3;
    Storage.failRename = failure == 4;
    Storage.failDirectory = failure == 5;
    CHECK(!retry->save());
    Storage.writeBudget = -1;
    Storage.failOpen = Storage.failClose = Storage.failRemove = Storage.failRename = Storage.failDirectory = false;
    auto reloaded = std::make_unique<SudokuStore>();
    CHECK(reloaded->load() == SudokuStore::LoadResult::Ok && reloaded->hasGame());
    uint8_t after[SudokuGame::ENCODED_SIZE];
    reloaded->game().encode(after);
    CHECK(std::memcmp(before, after, sizeof(before)) == 0);
  }

  // A damaged newest snapshot is moved aside and the older one is used.
  Storage.files = savedFiles;
  auto older = std::make_unique<SudokuStore>();
  CHECK(older->load() == SudokuStore::LoadResult::Ok);
  const uint32_t olderIndex = older->game().index();
  CHECK(older->game().toggleNote(firstEmpty(older->game()), 2) && older->save());
  const std::string newest =
      Storage.files.count(SAVE_A) && Storage.files[SAVE_A] != savedFiles.at(SAVE_A) ? SAVE_A : SAVE_B;
  Storage.files[newest][20] ^= 0x01;
  auto recovered = std::make_unique<SudokuStore>();
  CHECK(recovered->load() == SudokuStore::LoadResult::Recovered && recovered->hasGame() &&
        recovered->game().index() == olderIndex);
  CHECK(Storage.files.count(newest + ".bad") && !Storage.files.count(newest));
  CHECK(recovered->game().notes(firstEmpty(recovered->game())) == 0);
  CHECK(recovered->save());

  // Both damaged: start fresh, keep both for inspection.
  Storage = FakeStorage{};
  Storage.files[SAVE_A] = "x";
  Storage.files[SAVE_B] = std::string(SudokuStore::FILE_SIZE, '\0');
  auto fresh = std::make_unique<SudokuStore>();
  CHECK(fresh->load() == SudokuStore::LoadResult::Recovered && !fresh->hasGame());
  CHECK(Storage.files.count(std::string(SAVE_A) + ".bad") && Storage.files.count(std::string(SAVE_B) + ".bad"));

  // Damaged records are skipped; a file with no readable record is Damaged.
  resetStorage(pack);
  Storage.files[EASY][512 + 7] ^= 0x01;
  auto skipping = std::make_unique<SudokuStore>();
  CHECK(skipping->load() == SudokuStore::LoadResult::Ok &&
        skipping->startNext(Difficulty::Easy) == SudokuStore::StartResult::Ok);
  CHECK(skipping->game().index() == 1);
  Storage.files[EASY][512 + 96 + 7] ^= 0x01;
  CHECK(skipping->startNext(Difficulty::Easy) == SudokuStore::StartResult::Damaged);
  CHECK(skipping->game().index() == 1);  // previous game kept

  // An open failure leaves both slots untouched and disables writes until load.
  Storage.files = savedFiles;
  auto unreadable = std::make_unique<SudokuStore>();
  Storage.failOpen = true;
  CHECK(unreadable->load() == SudokuStore::LoadResult::Unreadable);
  CHECK(Storage.files == savedFiles);
  Storage.failOpen = false;
  CHECK(!unreadable->save() && unreadable->unsaved());
  CHECK(Storage.files == savedFiles);
  CHECK(unreadable->load() == SudokuStore::LoadResult::Ok && unreadable->save());

  // Eight damaged records advance the cursor; the next start reaches record 8.
  std::string longPack = pack.substr(0, 512);
  for (int i = 0; i < 9; ++i) longPack += pack.substr(512, 96);
  longPack[12] = 9;
  const uint32_t headerCrc = crc32(reinterpret_cast<const uint8_t*>(longPack.data()), 508);
  for (int i = 0; i < 4; ++i) longPack[508 + i] = static_cast<char>(headerCrc >> (8 * i));
  for (int i = 0; i < 8; ++i) longPack[512 + 96 * i + 7] ^= 1;
  resetStorage(longPack);
  auto advancing = std::make_unique<SudokuStore>();
  CHECK(advancing->load() == SudokuStore::LoadResult::Ok);
  CHECK(advancing->startNext(Difficulty::Easy) == SudokuStore::StartResult::Damaged);
  CHECK(advancing->progress().nextIndex[0] == 8);
  CHECK(advancing->startNext(Difficulty::Easy) == SudokuStore::StartResult::Ok);
  CHECK(advancing->game().index() == 8);

  // A failed save still starts the game in memory.
  resetStorage(pack);
  auto unsaved = std::make_unique<SudokuStore>();
  CHECK(unsaved->load() == SudokuStore::LoadResult::Ok);
  Storage.failRename = true;
  CHECK(unsaved->startNext(Difficulty::Easy) == SudokuStore::StartResult::SaveFailed && unsaved->hasGame());
}

}  // namespace

int main(int argc, char** argv) {
  CHECK(argc == 3);
  const std::string pack = readFile(argv[1]);
  const std::string reordered = readFile(argv[2]);
  CHECK(pack.size() == 512 + 2 * 96 && reordered.size() == pack.size());
  testCrcAndPacking();
  testPackReading(pack);
  testGameRules(pack);
  testEncoding(pack);
  testStore(pack, reordered);
  std::puts("sudoku tests passed");
  return 0;
}
