#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
artifacts=../qa-artifacts
mkdir -p "$artifacts"
json=.pio/libdeps/simulator_picco/ArduinoJson/src
if [ ! -d "$json" ]; then echo 'Build simulator_picco first to install ArduinoJson' >&2; exit 1; fi
c++ -std=c++20 -Wall -Wextra -Werror -fsanitize=address,undefined -Ilib/Todo \
  test/todo/model.cpp lib/Todo/TodoModel.cpp -o "$artifacts/todo-model-test"
"$artifacts/todo-model-test"
c++ -std=c++20 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Itest/todo/stubs -Ilib/Todo -Ilib/Memory -I"$json" \
  test/todo/store.cpp lib/Todo/TodoModel.cpp lib/Todo/TodoStore.cpp -o "$artifacts/todo-store-test"
"$artifacts/todo-store-test"
