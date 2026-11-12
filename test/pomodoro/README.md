# Pomodoro checks

`PomodoroTimerTest` is part of the existing CMake/CTest suite. It exercises real
25/5/15-minute durations with a deterministic clock: ready, pause/resume,
rollover, completion boundaries, delayed updates, stale Pause, reset, invalid
Next, the full four-focus/long-break cycle, custom durations and cycle size,
setting changes while Ready or Paused, and zero-value clamping.

After building `simulator_picco`, run:

```sh
../.venv/bin/python test/pomodoro/simulator.py
../.venv/bin/python test/pomodoro/simulator.py --power
```

The second command takes about two minutes: a running timer stays awake past a
one-minute sleep timeout, then a paused timer sleeps and wakes to Home.
Post-wake input uses the simulator's separate after-wake script.

Cards are isolated under workspace QA artifacts. Each timer workflow verifies
that an existing Todo snapshot remains byte-for-byte unchanged. The five-theme
matrix captures ready/running/paused states, tests native controls and tests
Home with Plugins enabled. The settings workflow edits Focus through the shared
dialog, checks `settings.json`, confirms the paused phase keeps its length until
Reset, and loads and clamps saved values. Control Center must pause, not reset.
Reset to defaults is checked with touch (Cancel keeps custom values; Reset
restores all four and the Ready timer shows 25:00) and with keys only.
Script delays count from the previous WAIT, so key steps within one segment
must use increasing times. The button test drives Start, Pause, Resume, Reset and the gear with Up/Down and
Confirm only. The Home check requires Lyra Extended to draw a scroll indicator
(five visible rows for six shortcuts) and Lyra to draw none. It reads pixels
from screenshots, so it needs Pillow. Timer completion and long-break transitions use the
host model tests, rather than a shortened production duration or test clock in
firmware. Physical e-ink and power behavior remain unverified.
