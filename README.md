# Picco custom firmware

An independently maintained BOOX Picco build derived from
[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader).
This repository has its own features, scope and maintenance decisions. It is
not an official CrossPoint release and is not affiliated with or supported by
CrossPoint, FreeInk or BOOX.

**Native builds and desktop simulator checks pass; physical Picco validation
is still pending.** No build of this fork has been flashed onto a physical
Picco. Touch accuracy, e-ink refresh, sleep/wake, power use, real memory limits
and SD-card durability remain unverified on hardware.

## Reading and applications

EPUB is the primary book format. The inherited reader provides book layout,
fonts, Library, bookmarks, dictionary lookup, highlights and clippings.
Existing support for other formats and devices remains in the source; it does
not establish support or verification by this fork.

This build adds:

- An offline [Todo list](docs/picco-todo.md) with completion, editing and
  confirmed deletion.
- A foreground [Pomodoro timer](docs/picco-pomodoro.md) with adjustable phases,
  pause/resume and manual phase advancement. It does not run in the background.
- [Sudoku](docs/picco-sudoku.md) with notes, undo, Check, Reveal, automatic
  saving and resumable, SHA256-verified puzzle-pack downloads.
- File Transfer in the Browse Files root header, with Back returning one level
  and Home returning to Home.
- The root `My Clippings.txt` export excluded from Library and Home book lists,
  while remaining available in Browse Files and file transfer.
- Optional checkbox and strike-through support in the pinned
  [FreeInk SDK fork](https://github.com/DumbledoreCalrissian/freeink-sdk).

Reading statistics is **unfinished**. Milestone 1, committed as `119344fb`,
provides the collector and per-book store. Reader collection and statistics
screens are not connected yet. New statistics work is **EPUB-only on BOOX
Picco**; other-format/device statistics, overall aggregation, new themes and
navigation redesign are outside that effort. See the
[foundation contract](docs/picco-reading-stats.md) and
[current handoff and milestones](docs/reading-stats-handoff.md).

## Build and run

The checked environment is Apple Silicon macOS with Python 3.13, pioarduino
Core 6.1.19 and SDL2. The simulator's HTTP checks also use libcurl. Keep firmware
and simulator checkouts side by side:

```sh
git clone --recurse-submodules -b picco https://github.com/DumbledoreCalrissian/crosspoint-reader.git
git clone -b fix/picco-test-link https://github.com/DumbledoreCalrissian/crosspoint-simulator.git
```

Use the existing workspace environment when available. From the firmware
checkout, with that environment activated:

```sh
pio run -e simulator_picco
pio run -e simulator_picco -t run_simulator
pio run -e picco
```

The last command compiles firmware; it does not upload it. Always select the
Picco profile explicitly: bare `pio run` selects an inherited C3 profile.
For a new host, follow [Getting Started](docs/contributing/getting-started.md).
These are build instructions, not a hardware installation or recovery guide.

The default simulator card is `fs_/`. Set `CROSSPOINT_SIM_SD` to use a separate
card directory. Tests use isolated cards. Back up the real card before storage
experiments: `/.crosspoint/` contains settings, Todo, Sudoku saves and reading
positions as well as rebuildable caches. Do not delete it as a general reset.

For Sudoku, copy the desired `.cpsd` files from this fork's
[sudoku-bank-v2 release](https://github.com/DumbledoreCalrissian/crosspoint-reader/releases/tag/sudoku-bank-v2)
to `/games/sudoku/`, or download them from Sudoku settings. Deleting a pack
keeps the saved game. See the guide for cancellation, save failures and recovery.

## Documentation and maintenance

- [User guide](USER_GUIDE.md): inherited reader behavior with downstream updates.
  Device-specific sections describe the inherited implementation, not hardware
  verification by this fork.
- [Scope](SCOPE.md) and [roadmap](ROADMAP.md): this fork's accepted boundaries
  and unfinished work.
- [Contributor guide](docs/contributing/README.md) and [agent instructions](AGENTS.md).
- [Web file transfer](docs/webserver.md) and [API endpoints](docs/webserver-endpoints.md).
- [SD-card fonts](docs/sd-card-fonts.md) and [file formats](docs/file-formats.md).

Use this repository's available issue and review channels for downstream bugs.
Include the commit, build profile, simulator or physical device, and steps to
reproduce. Upstream maintainers do not support this custom build. Retained
upstream CI workflows are not evidence that this fork's builds or devices have
been checked; use recorded checks for the revision under review.

## Attribution and licences

CrossPoint Reader and FreeInk provide the reader, hardware abstraction and UI
foundation. This fork retains their copyright and licence notices and identifies
its downstream changes separately. See [ATTRIBUTIONS.md](ATTRIBUTIONS.md).

Original CrossPoint code is covered by [LICENSE](LICENSE), with separate terms
for third-party code and assets. The combined firmware executable uses GPLv3
because it includes wolfSSL; see [licensing and source requirements](licenses/README.md).
Project branding and funding decisions are separate from those notices.
