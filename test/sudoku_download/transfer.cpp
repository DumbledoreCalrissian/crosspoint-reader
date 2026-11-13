#include <Arduino.h>
#include <SudokuCatalog.h>

#include <cassert>
#include <fstream>
#include <iostream>

#include "network/HttpDownloader.h"
ESPMock ESP;
// The harness tests real download and FAT swap code against deterministic I/O failures.
namespace sudoku {
const char* packPath(Difficulty) { return "/games/sudoku/easy.cpsd"; }
}  // namespace sudoku
int main(int argc, char** argv) {
  assert(argc == 5);
  const std::string mode = argv[1], url = argv[2], hash = argv[4];
  std::ifstream stream(argv[3], std::ios::binary);
  const std::string data((std::istreambuf_iterator<char>(stream)), {});
  assert(data.size() > 16384);
  const auto difficulty = sudoku::Difficulty::Easy;
  const char* part = sudoku::partPath(difficulty);
  const char* installed = sudoku::packPath(difficulty);
  Storage.files[installed] = "existing manually copied pack";
  Storage.files["/.crosspoint/sudoku-a.bin"] = "saved-game";
  if (mode == "resume" || mode == "ignore" || mode == "invalid" || mode == "complete" || mode == "oversize")
    Storage.files[part] = mode == "complete" ? data : data.substr(0, 8192);
  if (mode == "oversize") Storage.files[part] = data + "extra";
  if (mode == "full") Storage.writeBudget = 4000;
  if (mode == "readonly") Storage.failOpen = true;
  if (mode == "close") Storage.failClose = true;
  if (mode == "lowmemory") setenv("CROSSPOINT_SIM_FREE_HEAP", "39000", 1);
  size_t progress = 0;
  unsigned polls = 0;
  const auto started = millis();
  const auto result = HttpDownloader::downloadVerified(
      url, part, data.size(), hash.c_str(),
      [&](size_t bytes, size_t expected) {
        assert(expected == data.size());
        progress = bytes;
      },
      [&] {
        ++polls;
        return (mode == "cancel" && progress >= 8192) || (mode == "stallcancel" && millis() - started > 300);
      });
  assert(Storage.files[installed] == "existing manually copied pack");
  assert(Storage.files["/.crosspoint/sudoku-a.bin"] == "saved-game");
  if (mode == "lowmemory") {
    assert(result == HttpDownloader::LOW_MEMORY && !Storage.exists(part));
  } else if (mode == "full" || mode == "readonly" || mode == "close") {
    assert(result == HttpDownloader::FILE_ERROR);
    assert(!Storage.exists(part));
  } else if (mode == "invalid" || mode == "interrupt") {
    assert(result == HttpDownloader::HTTP_ERROR);
    assert(Storage.exists(part));
    assert(Storage.files[part] == data.substr(0, Storage.files[part].size()));
  } else if (mode == "cancel" || mode == "stallcancel") {
    assert(result == HttpDownloader::ABORTED && polls > 0);
    assert(millis() - started < 3000);
    assert(Storage.exists(part));
  } else if (mode == "wrong" || mode == "oversize") {
    assert(result == HttpDownloader::HASH_ERROR && !Storage.exists(part));
  } else {
    assert(result == HttpDownloader::OK && Storage.files[part] == data);
    if (mode == "swapfail") {
      Storage.renameBudget = 1;
      assert(!sudoku::installPack(difficulty));
      assert(Storage.files["/games/sudoku/easy.cpsd.bak"] == "existing manually copied pack");
      Storage.renameBudget = -1;
      assert(sudoku::recoverPack(difficulty));
      assert(Storage.files[installed] == "existing manually copied pack");
    } else {
      assert(sudoku::installPack(difficulty));
      assert(Storage.files[installed] == data && !Storage.exists(part));
      Storage.files["/games/sudoku/easy.cpsd.bak"] = "old backup";
      assert(sudoku::recoverPack(difficulty));
      assert(!Storage.exists("/games/sudoku/easy.cpsd.bak"));
      assert(sudoku::deletePack(difficulty));
      assert(!Storage.exists(installed));
    }
  }
  assert(Storage.files["/.crosspoint/sudoku-a.bin"] == "saved-game");
  std::cout << mode << " PASS result=" << result << " polls=" << polls << "\n";
}
