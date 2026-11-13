# Sudoku

Open **Sudoku** on Home and choose a difficulty. If the shortcut is below the
visible rows, swipe or use Up/Down to reveal it. The board is touch-operated.

## Install puzzles

Open the header gear for Sudoku settings, then choose **Download** for a
difficulty or **Download all packs**. Select Wi-Fi when prompted. Downloads
check their size and SHA256 before replacing an installed pack.

For manual installation, copy the `.cpsd` files from this fork's
[sudoku-bank-v2 release](https://github.com/DumbledoreCalrissian/crosspoint-reader/releases/tag/sudoku-bank-v2)
to `/games/sudoku/` on the card. The files are `easy.cpsd`, `medium.cpsd`,
`hard.cpsd` and `diabolical.cpsd`. Install only the difficulties you want.
In the default simulator card, use `fs_/games/sudoku/`.

Puzzles are taken in file order, with the next puzzle remembered for each
difficulty. **Continue** reopens the saved game. Starting another game asks
before replacing it.

## Play

Tap a cell, then a digit. Given digits cannot be changed.

- **Erase** clears an entry, or its notes when the cell has no entry.
- **Notes** switches the digit pad to pencil marks. Notes stay hidden beneath
  an entered digit and return when it is erased.
- **Undo** reverses up to 64 changes, including notes, and survives reopening.
- **Check** frames incorrect entries until the next change.
- **Reveal** fills the selected cell. Check and Reveal each count as a hint.
- **Mark conflicts** underlines a repeated digit in a row, column or box.

A solved board stays visible until **Done** returns to the start list.
Settings can hide Easy, turn conflict marking off, or remove Check and Reveal.

## Saving

Entries, erases, undo, Check and Reveal save automatically. Notes-only changes
save with the next move, when leaving the board or before sleep. A power cut
can lose unsaved notes.

If saving fails, the game continues in memory and a message appears. Continue
keeps that game while Sudoku stays open; leaving Sudoku can lose unsaved changes.
Resolve the card problem and save successfully before downloading packs.

Back up both `/.crosspoint/sudoku-a.bin` and `sudoku-b.bin`. The saved game
includes its puzzle, so deleting a puzzle pack does not remove the game.
Damaged saves are moved to `.bad` files; an intact alternate save is used when
available. Unreadable saves stay untouched and saving remains disabled until
Sudoku is reopened. Back up the card before attempting recovery.

## Downloads, cancellation and deletion

**Cancel**, Back or the Home gesture during a transfer stops the download.
An interrupted download keeps a partial file and resumes on the next attempt.
Already installed packs remain available. Download all stops on the first
failure or cancellation and keeps packs already installed.

Wi-Fi turns off after transfer. **Back/Done** from the result briefly restarts
into Sudoku settings; Back there returns to the game list. Home outside a
transfer returns to Home. In Control Center, the first Home gesture closes
the panel. Manual sleep stops the transfer before sleeping and keeps its partial.

**Delete** asks for confirmation and removes that difficulty's pack and partial
download, preserving the saved game. Replacing a pack needs enough card space
for the old and new copies. Downloads are blocked while game progress is unsaved.

If replacement recovery leaves a `.cpsd.bak` file, keep it: opening Sudoku or
settings retries recovery. Power loss or card failure can still damage data;
keep a backup of the card.
