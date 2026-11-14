# Picco custom firmware

A personal BOOX Picco build based on
[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader).
This is an unofficial fork, not supported by the original project or BOOX.

**Experimental: tested in the desktop simulator, not on a physical Picco.**

## Features

EPUB reading, Library, custom fonts, bookmarks, dictionary lookup and clippings,
plus:

- [Todo](docs/picco-todo.md): an offline task list.
- [Pomodoro](docs/picco-pomodoro.md): a foreground focus/break timer.
- [Sudoku](docs/picco-sudoku.md): notes, undo, hints, saved games and puzzle-pack downloads.
- File Transfer from the Browse Files root header.
- `My Clippings.txt` hidden from Library and Home, while remaining accessible in Browse Files.

The EPUB reader menu includes Book statistics: current progress, reading time,
forward page turns and a confirmed reset that keeps position and bookmarks.
Library shows each opened EPUB's saved percentage. Home shows the current EPUB's
progress and reading time in every theme; Lyra Extended shows it on each recent
cover, and Cover Grid thumbnails show each book's percentage and bar.

The Desk theme shows the current book's cover large, with its title, progress,
reading time and pages turned, and the other recent books as a swipeable strip
of small covers with their titles and progress bars. Its other apps (Browse
Files, Library, Settings, Todo, Pomodoro, Sudoku and any Plugins or OPDS
shortcut) open from the menu button at the top right of Home.
Its [reading calendar](docs/reading-calendar.md) shows monthly minutes and a streak
of days meeting your adjustable daily reading goal.
Its [weather widget and screen](docs/weather.md) show a manually refreshed
three-day forecast, humidity and precipitation for a city you choose.

## Try the simulator

Keep the firmware and simulator repositories side by side:

```sh
git clone --recurse-submodules -b picco https://github.com/DumbledoreCalrissian/crosspoint-reader.git
git clone -b fix/picco-test-link https://github.com/DumbledoreCalrissian/crosspoint-simulator.git
cd crosspoint-reader
pio run -e simulator_picco
pio run -e simulator_picco -t run_simulator
```

See [setup instructions](docs/contributing/getting-started.md) for the macOS
requirements. Put books in `fs_/books/`; set `CROSSPOINT_SIM_SD` to use another
virtual card directory. Build native Picco firmware with `pio run -e picco`.
Hardware installation and recovery have not been validated for this fork.

## Help

- [Reader user guide](USER_GUIDE.md)
- [Web file transfer](docs/webserver.md)
- [SD-card fonts](docs/sd-card-fonts.md)

Back up your card before experimenting. `/.crosspoint/` contains saved tasks,
puzzles, settings and reading positions; do not delete it as a general reset.

## Credits and licences

Based on CrossPoint Reader and [FreeInk SDK](https://github.com/Free-Ink/freeink-sdk),
with the [CrossPoint simulator](https://github.com/crosspoint-reader/crosspoint-simulator).
Sudoku puzzles come from the public-domain
[Sudoku Exchange puzzle bank](https://github.com/grantm/sudoku-exchange-puzzle-bank).
Original copyright and third-party notices are retained.

See [LICENSE](LICENSE) and [third-party licences and source information](licenses/README.md).
The combined firmware executable uses GPLv3 because it includes wolfSSL.
