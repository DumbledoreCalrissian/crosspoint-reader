# macOS setup

The simulator has been built on Apple Silicon macOS using Python 3.13,
pioarduino Core 6.1.19, SDL2 and libcurl. Python 3.15 is incompatible with the
pinned platform. macOS Command Line Tools provide the libcurl headers/library.

## Install tools

If you already have a working environment, reuse it. For a new Homebrew setup:

```sh
brew install sdl2 python@3.13
mkdir picco-workspace
cd picco-workspace
/opt/homebrew/bin/python3.13 -m venv .venv
.venv/bin/python -m pip install https://github.com/pioarduino/platformio-core/archive/refs/tags/v6.1.19.zip
. .venv/bin/activate
```

## Get the source

Keep the repositories side by side:

```sh
git clone --recurse-submodules -b picco https://github.com/DumbledoreCalrissian/crosspoint-reader.git
git clone -b fix/picco-test-link https://github.com/DumbledoreCalrissian/crosspoint-simulator.git
cd crosspoint-reader
```

If cloned without submodules, run `git submodule update --init --recursive`.

## Build and launch

```sh
pio run -e simulator_picco
pio run -e simulator_picco -t run_simulator
```

Put books in `fs_/books/`, or set `CROSSPOINT_SIM_SD` to another card directory.
To build firmware without uploading it:

```sh
pio run -e picco
```

Use explicit environment names; bare `pio run` selects an inherited C3 profile.
This fork has not been tested on physical hardware, and these instructions do
not provide a validated Picco flashing or recovery procedure.
