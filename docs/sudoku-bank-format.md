# Sudoku bank format (CPSD version 1)

Sudoku puzzle files (`.cpsd`) hold presolved puzzles for the Picco fork's Sudoku.
`scripts/build_sudoku_bank.py` creates them from the public-domain
[Sudoku Exchange puzzle bank](https://github.com/grantm/sudoku-exchange-puzzle-bank)
at commit `d8c8ebaee0c08c412cfba96af1923dfa61c83317`. Each puzzle has exactly
one solution; the converter checks this for every record.

There is one file per difficulty: `easy.cpsd`, `medium.cpsd`, `hard.cpsd` and
`diabolical.cpsd`. On the SD card they live in `/games/sudoku/`.

All numbers are unsigned and little-endian. CRC32 is the standard CRC-32 used
by zlib (polynomial `0xEDB88320`, reflected, initial value and final XOR
`0xFFFFFFFF`); the CRC32 of the ASCII text `123456789` is `0xCBF43926`.

## File layout

A file is a 512-byte header followed by `record count` records of 96 bytes.
The file length must be exactly `512 + 96 × record count`. Records are numbered
from 0. Record *i* starts at byte `512 + 96 × i` and holds the puzzle on line
*i* of the source file, also counting from 0.

## Header (512 bytes)

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | Magic: ASCII `CPSD` |
| 4 | 2 | Format version: `1` |
| 6 | 2 | Header size: `512` |
| 8 | 2 | Record size: `96` |
| 10 | 1 | Difficulty: `0` easy, `1` medium, `2` hard, `3` diabolical |
| 11 | 1 | Reserved, `0` |
| 12 | 4 | Record count |
| 16 | 4 | CRC32 of all record bytes (identifies the puzzle set) |
| 20 | 20 | Source commit SHA1: the 20 bytes of the hexadecimal hash, in order |
| 40 | 2 | Lowest rating × 10 |
| 42 | 2 | Highest rating × 10 |
| 44 | 4 | Converter version |
| 48 | 460 | Provenance: UTF-8 text, padded with zero bytes; not terminated if it fills all 460 bytes |
| 508 | 4 | CRC32 of bytes 0–507 |

## Record (96 bytes)

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 41 | Givens: 81 cells, 4 bits each, `0` = empty |
| 41 | 41 | Solution: 81 cells, 4 bits each, values `1`–`9` |
| 82 | 6 | First 6 bytes of the SHA1 of the source's 81-digit puzzle string |
| 88 | 2 | Rating × 10 (for example `23` for 2.3) |
| 90 | 1 | Number of givens |
| 91 | 1 | Flags, `0` (reserved) |
| 92 | 4 | CRC32 of bytes 0–91 |

Cells are numbered 0–80, row by row from the top-left. Cell `2k` is stored in
the high 4 bits of byte `k` and cell `2k + 1` in the low 4 bits. Cell 80 uses the
high 4 bits of the last byte; its low 4 bits are `0`.

## Reader checks

A reader rejects the file when any of these is wrong:

- magic, format version, header size or record size;
- difficulty: it must match the file name (`easy.cpsd` is `0`, and so on);
- reserved byte 11, which must be `0`;
- header CRC;
- file length.

When it reads a record, it rejects the record when any of these is wrong:

- record CRC;
- a cell value above 9, or non-zero unused bits after cell 80;
- solution: every cell 1–9 and a valid Sudoku grid;
- givens: each given equals the solution at that cell, and the givens count
  matches;
- flags, which must be `0`;
- rating: inside the header's lowest–highest range.

The records CRC at offset 16 identifies the puzzle set (for example, to reset
saved progress when a file is replaced). A device need not check it when opening
a file, because that would mean reading the whole file; downloads are checked
with SHA256 instead. The converter's own reader checks it.

## Provenance and licence

The converter stops on any bad source record (layout, hash, rating, fewer than
17 givens, conflicting givens, not exactly one solution) and on a puzzle that
appears more than once in any of the four files. Nothing in the output folder
changes when it stops.

The source bank is dedicated to the public domain by its author (Unlicense-style
text in the source repository). The converter writes `PROVENANCE.json` next to
the files with the source URL and commit, the SHA256 of each input and output
file, record counts and the converter and format versions. It contains no
timestamps, so the same input always gives byte-identical output.
