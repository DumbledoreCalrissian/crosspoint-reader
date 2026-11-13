# Sudoku

Open **Sudoku** on Home (a 3×3 grid icon in themes with Home icons).
Home has six shortcuts. Classic, Lyra and RoundedRaff show
all six. In Lyra Extended, and in every list theme when Plugins is enabled, Sudoku
is below the visible rows: a scroll indicator appears, and a swipe or Up/Down shows
it.

Cover-grid Home adds a seventh tab. Sudoku is played by touch; the start list
and settings also work with Up/Down and Confirm.

## Puzzle files

Sudoku reads presolved puzzle files from `/games/sudoku/` on the SD card:

| File | Puzzles | Rating |
| --- | ---: | --- |
| `easy.cpsd` | 100,000 | 1.2 |
| `medium.cpsd` | 352,643 | 1.5–2.3 |
| `hard.cpsd` | 321,592 | 2.5–4.8 |
| `diabolical.cpsd` | 119,681 | 5.0–9.3 |

Install them from **Sudoku settings**, or download the `sudoku-bank-v2` release
of this fork and copy them with
USB Drive mode, or create the `games/sudoku` folder in File Transfer and upload
them. You can install only the difficulties you want. The puzzles come from the
public-domain [Sudoku Exchange puzzle bank](https://github.com/grantm/sudoku-exchange-puzzle-bank);
`scripts/build_sudoku_bank.py` adds the solutions and checks that every puzzle
has exactly one solution. The format is in [sudoku-bank-format.md](sudoku-bank-format.md).
`.cpsd` files never appear in Library or Home.

## Start list

- **Continue** appears when a puzzle is in progress, with its difficulty and how
  much of the board is filled.
- Each difficulty shows its puzzle count, **Not installed**, or **Damaged**. Only
  installed ones can be tapped. With no files at all, the screen explains where
  to put them.
- Puzzles are taken in file order. The next one is remembered per difficulty;
  after the last puzzle, the file starts again. Replacing a file with different
  puzzles starts that difficulty from its first puzzle.
- Only one puzzle is kept. Starting another while one is in progress asks
  **Start a new puzzle?** (Cancel is selected first).
- The gear opens **Sudoku settings**. Header Back or the Home gesture goes Home.

## Playing

The header shows the difficulty and the puzzle's rating.

Tap a cell, then a digit. The selected cell uses the theme's selection paint.
Given digits are bold and cannot change; your entries use the regular weight.

- **Erase** clears the entry, or the cell's notes when it has no entry.
- **Notes** switches the pad to notes until you tap it again. Notes are small
  digits in a 3×3 layout and are shown only in empty cells.
- **Undo** reverses the last change, including notes, up to 64 steps. Undo also
  works after leaving and reopening the puzzle.
- **Check** frames entries that differ from the solution until your next change.
- **Reveal** fills the selected cell with its solution.
- Check and Reveal each count as a hint.
- With **Mark conflicts** on, an entry that repeats a digit in its row, column or
  box is underlined. This uses only the rules, not the solution.
- The puzzle is solved when the board is full and nothing repeats. The finished
  board stays on screen with the rating and hints used; **Done** returns to the
  start list.
- Up, Down and Confirm do nothing on the board, so a key press never changes it.

## Saving

There is no Save command. The game is saved after each entry, erase, undo,
Check or Reveal, when you leave the board, and before sleep. Changes to notes
alone are saved with the next of these, so a power cut can lose only recent
notes. If a save fails, a note appears above the board and the game continues
in memory; the previous save is kept. The start list also reports the failure,
and Continue reopens the game kept in memory.

Saves are `/.crosspoint/sudoku-a.bin` and `sudoku-b.bin`, written alternately
like the Todo snapshots, so a failed or cut write keeps the previous one. A save
holds its own copy of the puzzle and solution, so removing a puzzle file does not
affect the game in progress. A damaged save is renamed to `sudoku-a.bin.bad` or
`sudoku-b.bin.bad`, the start list says so, and Sudoku continues from the other
save or from a fresh state. Delete the `.bad` files when no longer needed. Sudoku
leaves unreadable saves in place. If a save cannot be opened or read, the start
list explains that progress will not be saved until you reopen Sudoku. You can
keep playing in memory, but those changes are lost when you leave Sudoku.
The board writes no other files. Pack management uses only the paths described below.

## Sudoku settings

All are on by default and stored in `settings.json`.

| Setting | Effect |
| --- | --- |
| Show Easy | Off hides Easy from the start list (files are kept). |
| Mark conflicts | Underlines repeated digits. |
| Check button | Off removes Check. |
| Reveal button | Off removes Reveal. |

## Downloading and deleting packs

Below the four toggles, settings offers **Download all packs**, then **Download**
and **Delete** for each difficulty. Each Download row shows Installed, Not
installed or Damaged, the puzzle count (installed count when available, otherwise
the published count), and the full download size in bytes. A copied pack may
have a different puzzle count; the download size always describes the published
replacement. Show Easy does not hide pack management.

Download opens the existing Wi-Fi selector. It fetches the pinned CPSD v2 assets
from this fork's `sudoku-bank-v2` release. Download all processes all four,
including installed difficulties, in order and stops on the first failure or
cancellation. The result reports how many packs were installed in that attempt;
completed packs remain installed. Wi-Fi turns off when the transfer ends. Back
or Done leaves the result and silently restarts into Sudoku settings, with the
start list underneath so Back still returns to the game list. Cancelling Wi-Fi
selection also restarts into settings. The normal Home gesture during a transfer
cancels it; acknowledge the result to return to settings. Manual sleep keeps the
existing wake/Home policy and does not force a network-exit restart. A power
sleep request cancels the transfer, closes its file and turns Wi-Fi off before
the main loop sleeps; the partial remains available for resume. Outside the
transfer, Home leaves for Home, including after network use. In Control Center,
the first Home gesture closes the panel as usual.

The progress screen has a touch **Cancel** button. Hardware Back also cancels
on boards that have that key. Only an active transfer prevents auto-sleep;
settings and the result screen use normal idle sleep. The Wi-Fi selector keeps
its existing sleep policy. After a long synchronous transfer, the normal idle
interval starts again so the result can be read.

Transfers stream to `/games/sudoku/<difficulty>.part`. An interrupted or
cancelled transfer keeps this file for HTTP Range resume on the next attempt.
The retained prefix is hashed again before resuming. If a server ignores Range,
the download restarts from zero. An invalid Range response is rejected. The
complete size and SHA256 must match pins built into the firmware; a mismatch
removes the partial, and failed writes remove it when the card allows removal.
No unverified file replaces an installed pack. Replacing a pack needs room for
both old and new copies (up to 33,854,240 extra bytes for Medium).

After verification, installation temporarily renames the old pack to
`<difficulty>.cpsd.bak`. If the rename of the new pack fails, it restores the old
one. If storage failure prevents restoration, the backup stays in place; opening
Sudoku or settings retries recovery. A restart between renames also recovers it.
FAT and the card do not provide a power-loss transaction guarantee; verify this
on hardware. Do not remove the backup manually during recovery.

Delete asks for confirmation with Cancel selected first. It removes only that
difficulty's installed pack, backup and partial, then refreshes its status. It
never deletes snapshots: **Continue works after pack removal**. Toggles and
pack deletion are available offline. Downloads are blocked while a failed save
has left unsaved progress in memory, because the Wi-Fi restart would lose it.
Open Continue and save successfully before downloading.

The fork owner maintains the release URL, sizes and hashes together with the
converter provenance. If the release becomes unavailable or changes, downloads
fail; manually copied CPSD v2 packs keep working. There is no device solver.

## Limits

- Portrait only, like the other Home apps. No timer, statistics or automatic
  note removal.
- Rendering is turn-based with fast refresh; a half refresh clears ghosting at
  the reader's configured refresh frequency and when a puzzle is solved.
- Memory: the start list owns one puzzle store, which the board borrows;
  puzzle files are read one 96-byte record at a time.

Native milestone 3 build: flash 5,522,762 bytes, 8,464 more than the merged
milestone 2 baseline (`2441f1c5`); static DRAM remains 112,676 bytes. These are
linked totals, not runtime measurements. Target debug information reports a
2,140-byte start-list object, 3,084-byte settings object, 1,356-byte Wi-Fi child
and 1,140-byte transfer hash state. Parents stay resident beneath children; TLS,
font caches, sockets, allocator overhead and task stacks are additional costs.
The hash state is allocated only for one transfer. The existing TLS floor is
checked after that allocation as well as before starting the flow.

## Verification

From the firmware root, after `pio run -e simulator_picco`:

```sh
sh test/sudoku_download/run.sh
../.venv/bin/python -I test/sudoku_download/simulator.py
sh test/sudoku/run.sh
../.venv/bin/python test/sudoku/simulator.py
../.venv/bin/python -I test/test_sudoku_bank.py
```

Hardware checks still needed on a real Picco: cell and button size and touch
accuracy at cell edges, readability of notes and of given versus entered digits,
fast-refresh ghosting during play, save timing on the real SD card, sleep and
wake with a puzzle open, removing the card while playing, and heap and stack use
while the board is open.

Download hardware checks: use a backed-up card; cancel partway, reopen Download
and confirm resumed progress; cut Wi-Fi and retry; replace a copied pack; delete
it while a saved game exists and Continue that game; exit Wi-Fi and confirm the
silent restart lands in Sudoku settings, Back returns to the start list, and
the live frontlight is preserved. Check full/read-only/removed-card messages,
auto-sleep after a long transfer, manual sleep, and per-pool free/minimum/largest
heap blocks plus stack high-water marks across repeated TLS transfers. Test header
Back, touch Cancel, Home cancellation, physical key remapping and all reader
orientations before returning to portrait Sudoku. A stalled Cancel, lost game,
wrong restart destination or decreasing largest blocks is a failure sign.
The simulator's heap/PSRAM/stack readings are mocks and its restart is a no-op;
physical hardware has never been tested.
