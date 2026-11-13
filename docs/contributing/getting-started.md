# Getting started with the Picco fork

For an existing workspace, use its installed tools and preserve caches and local
settings. The setup commands below are for a new host, not a pre-build ritual.
The checked host is Apple Silicon macOS; other host setup is not verified here.

## Checkouts

Keep the firmware and simulator side by side. The repository pins its SDK submodule.

```sh
mkdir picco-workspace
cd picco-workspace
git clone --recurse-submodules -b picco https://github.com/DumbledoreCalrissian/crosspoint-reader.git
git clone -b fix/picco-test-link https://github.com/DumbledoreCalrissian/crosspoint-simulator.git
```

If the firmware was cloned without submodules, run
`git submodule update --init --recursive` inside it. Do not replace its pinned
SDK revision with the latest upstream branch.

## First-time macOS environment

The recorded environment uses Python 3.13, pioarduino Core 6.1.19, SDL2, CMake
and clang-format 21 or newer. Python 3.15 was incompatible with the pinned platform.
libcurl is also needed for simulator downloads; macOS Command Line Tools provide
its headers and library.

From `picco-workspace`, on an Apple Silicon host with Homebrew:

```sh
brew install sdl2 python@3.13 cmake clang-format
/opt/homebrew/bin/python3.13 -m venv .venv
.venv/bin/python -m pip install https://github.com/pioarduino/platformio-core/archive/refs/tags/v6.1.19.zip
. .venv/bin/activate
```

Image checks additionally require Pillow in that environment. Install missing
dependencies only for the checks being run. Simulator tests that bind localhost
and PlatformIO's user-level cache lock may require sandbox approval.

Enable the existing formatting hook once per firmware clone:

```sh
cd crosspoint-reader
git config core.hooksPath .githooks
chmod +x .githooks/pre-commit
```

## Build and run

With the workspace environment active, from `crosspoint-reader`:

```sh
pio run -e simulator_picco
pio run -e simulator_picco -t run_simulator
pio run -e picco
```

The last command compiles firmware only. Bare `pio run` selects an inherited C3
profile, not Picco. The simulator defaults to `fs_/`; use `CROSSPOINT_SIM_SD` for
an isolated virtual card and `SDL_VIDEODRIVER=dummy` for scripted checks.

The installed checkout may use `../.venv/bin/pio` instead of activation. Native
and simulator build success do not establish physical Picco behavior. Hardware
installation/recovery has not been validated by this fork; this guide does not
prescribe a flash command or authorize device access.

See [development workflow](development-workflow.md), [testing](testing-debugging.md)
and [AGENTS.md](../../AGENTS.md) before changing firmware. Preserve the current
checkout and uncommitted work; create branches or worktrees only for a concrete need.
