# Sudoku download checks

From the firmware checkout, activate `../.venv` and build `simulator_picco`:

```sh
sh test/sudoku_download/run.sh
python -I test/sudoku_download/simulator.py
```

These tests use only local fixtures, not public release assets. The transfer
harness compiles production HttpDownloader and SudokuCatalog under ASan/UBSan,
with an in-memory HalStorage that can reject writes, closes, opens and renames.
It uses the simulator's real libcurl stream and SDK-matched ResumableFetch.
Tests cover success, redirects, cross-call resume, ignored and invalid Range,
cut-and-resume, exhausted retries, active and stalled-header cancellation, wrong
hash, oversized and complete partials, full/read-only storage, close failure,
low heap after hash allocation, install rollback/recovery and deletion. Existing
packs and a saved-game sentinel are checked after every result.

The UI suite uses isolated simulator cards and valid CPSD fixture packs made
with the real converter's encoder. It checks Download all, replacement/resume,
hash failure, touch cancellation, deletion confirmation/cancellation, Continue
after deletion, failed-save download protection, five-theme settings lists and a transfer longer
than the one-minute idle timeout without immediate sleep on completion.
The existing Sudoku UI suite covers the four toggles, board and save behavior.
Full logs use the workspace `logs/sudoku-download-*` prefix. Images are in
`qa-artifacts/sudoku-download/`.

Simulator-only environment variables select fixtures: `CROSSPOINT_SIM_SUDOKU_BANK_URL`
is a base URL with a trailing slash; `CROSSPOINT_SIM_SUDOKU_PACK_0` through `_3`
use `bytes:lowercase-sha256`. Device builds have no override and always use the
release pins. The simulator requires libcurl headers and library. Its SHA256 is
backed by CommonCrypto on macOS and OpenSSL on Linux; hardware heap, PSRAM and
stack values are still mocks. The simulator's ESP.restart is a no-op, so tests
observe the requested target; a host routing check covers the boot branch, and
actual reboot/frontlight behavior remains an owner hardware check.

`contracts.py` also runs the production transfer prologue against a controlled
concurrent render contender. Cache release must finish before rendering enters;
a deliberately unlocked variant must fail. A targeted simulator check with real
SD NotoSans TTF UI fallback and Russian progress/Cancel text is recorded in the
workspace artifact folder as `check_followup_ui.py`.
