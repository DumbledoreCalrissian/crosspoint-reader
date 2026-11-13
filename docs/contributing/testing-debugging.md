# Testing and debugging the Picco fork

## Select the affected checks

Use the existing workspace environment and build caches. From the firmware checkout:

```sh
./bin/clang-format-fix -g
git diff --check
pio run -e simulator_picco
pio run -e picco
```

These are compile checks, not upload commands. Run the relevant host tests and
simulator workflows from the [Todo](../picco-todo.md), [Pomodoro](../picco-pomodoro.md),
[Sudoku](../picco-sudoku.md) or [statistics](../picco-reading-stats.md) guide.
CMake/CTest checks can use the existing `build/test` directory. Some inherited
parser parameter cases share temporary filenames; run those serially if parallel
runs collide rather than changing unrelated parser code.

Use isolated simulator cards for fault tests. Back up real card data before
recovery work. `/.crosspoint/` includes Todo, Sudoku, settings and reading
positions; deleting it is not a safe general cache reset. Follow the affected
store's recovery procedure and preserve unrelated files.

## Evidence and physical limits

Record the source revision, selected target, commands and complete logs. Native
build totals measure linked flash/static RAM. Simulator heap, PSRAM, stack and
restart behavior do not establish physical resource or reboot behavior.
Physical Picco testing requires owner authorization and remains the owner's task.
For changed input/UI paths, cover relevant remapping and all reader orientations.
Budget internal RAM and PSRAM separately, including largest blocks and stack margins.

[AGENTS.md](../../AGENTS.md) and the firmware-handoff skill define firmware review
and acceptance. A documentation check or a sandbox failure is not a firmware pass.

## Bug reports

Include the commit, `picco` or `simulator_picco` target, host/device, steps to
reproduce, expected/actual result and relevant logs or screenshots. State whether
the issue reproduces on physical hardware. Remove secrets from shared logs.
See the [user guide](../../USER_GUIDE.md#7-troubleshooting-issues--escaping-bootloop)
and [webserver troubleshooting](../troubleshooting.md) for inherited reader details;
use this fork's scope and workflow for development decisions.
