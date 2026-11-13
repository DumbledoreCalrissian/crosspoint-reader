#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
mkdir -p ../qa-artifacts/sudoku-download
c++ -std=c++20 -Wall -Wextra -Wno-unused-parameter -Wno-deprecated-declarations -fsanitize=address,undefined \
  -DCROSSPOINT_VERSION='"host-test"' -Itest/sudoku_download/stubs -I../crosspoint-simulator/src \
  -Ilib/Memory -Ilib/Sudoku -Isrc test/sudoku_download/transfer.cpp src/network/HttpDownloader.cpp \
  lib/Sudoku/SudokuCatalog.cpp ../crosspoint-simulator/src/NetworkClient.cpp ../crosspoint-simulator/src/SimHttpRoutes.cpp \
  -I.pio/libdeps/simulator_picco/ArduinoJson/src -lcurl \
  -o ../qa-artifacts/sudoku-download/transfer-test
../.venv/bin/python -I test/sudoku_download/http_tests.py
../.venv/bin/python -I test/sudoku_download/contracts.py
