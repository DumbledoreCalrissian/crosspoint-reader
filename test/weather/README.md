# Weather host checks

Build through the root test CMake project, then run `ctest -R Weather`.
The tests use fake storage, time, heap statistics and HTTP data. They exercise
production model, parser, client and cache code; transport/input/rendering need
the simulator and hardware checks described in the task handoff.

`golden-weather-v1.bin` is independently encoded by `generate_fixture.py` using
Python `struct` and `zlib`, rather than the firmware codec. Regenerate from the
repository root with `python3 test/weather/generate_fixture.py`. It describes
Tokyo at 2026-11-15 12:00 local, with no trusted fetch timestamp, metric units
and three days of illustrative forecast values. It is not a default location.
