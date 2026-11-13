#!/bin/sh
# Host tests for lib/Sudoku under AddressSanitizer/UBSan. The puzzle files come
# from the real converter, so this also checks the CPSD format contract.
set -eu
cd "$(dirname "$0")/../.."
artifacts=../qa-artifacts/sudoku
mkdir -p "$artifacts"
fixtures=$(mktemp -d)
trap 'rm -rf "$fixtures"' EXIT
python=${PYTHON:-../.venv/bin/python}
"$python" -I - "$fixtures" <<'PY'
import hashlib, importlib.util, pathlib, sys
spec = importlib.util.spec_from_file_location("bank", "scripts/build_sudoku_bank.py")
bank = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bank)
a = "050703060007000800000816000000030000005000100730040086906000204840572093000409000"
b = "302401809001000300000000000040708010780502036000090000200609003900000008800070005"
def line(d):
    return f"{hashlib.sha1(d.encode()).hexdigest()[:12]} {d}  1.2\n".encode()
root = pathlib.Path(sys.argv[1])
for name, order in (("normal", (a, b)), ("reordered", (b, a))):
    src = root / name / "src"
    src.mkdir(parents=True)
    data = b"".join(line(d) for d in order)
    (src / "easy.txt").write_bytes(data)
    bank.build(src, root / name / "out", sources=(("easy", 0, hashlib.sha256(data).hexdigest()),), jobs=1)
PY
c++ -std=c++20 -Wall -Wextra -Werror -fsanitize=address,undefined -Itest/sudoku/stubs -Ilib/Sudoku \
  test/sudoku/tests.cpp lib/Sudoku/SudokuCatalog.cpp lib/Sudoku/SudokuPack.cpp lib/Sudoku/SudokuGame.cpp lib/Sudoku/SudokuStore.cpp \
  -o "$artifacts/sudoku-test"
"$artifacts/sudoku-test" "$fixtures/normal/out/easy.cpsd" "$fixtures/reordered/out/easy.cpsd"
