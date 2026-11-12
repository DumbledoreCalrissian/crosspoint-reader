#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
json=.pio/libdeps/simulator_picco/ArduinoJson/src
if [ ! -d "$json" ]; then json=.pio/libdeps/picco/ArduinoJson/src; fi
out=../qa-artifacts/clippings-exclusion
mkdir -p "$out"
clang++ -std=c++20 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Itest/clippings_export/stubs -Isrc -Ilib/Serialization -Ilib/FsHelpers -I"$json" \
  test/clippings_export/recent.cpp src/RecentBooksStore.cpp -o "$out/recent-test"
"$out/recent-test"
