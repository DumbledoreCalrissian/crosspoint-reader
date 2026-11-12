# Foreground Pomodoro

Open **Pomodoro** on Home. It uses the native header, theme and touch buttons.
When a list Home has more shortcuts than fit (for example Lyra Extended, or
Plugins enabled), a scroll indicator like the Settings lists' appears at the
menu's edge; swipe or use Up/Down to reveal the rest. Rows stay in place while
tapping a visible item. Cover-grid Home has a timer icon.

The screen shows the phase, a large countdown, a phase progress bar, the run
state, and one dot per focus session in the cycle.

- Defaults: **25-minute** focus, **5-minute** short break, **15-minute** long
  break after **four** focus sessions.
- Tap **Start**, **Pause** or **Resume**. While a phase is running or paused, a
  **Reset phase** button appears to its right. Buttons are outlined; the focused
  control (gear, primary button or Reset) uses the theme's list selection paint,
  like the selected row in Settings. Reset stops the
  unfinished phase and restores its full length, keeping completed sessions.
- At completion, the message and `00:00` remain visible. Tap **Start next phase**
  to begin the next phase. Reset is not offered on completed phases, so a
  completed focus session cannot be counted again. Finishing the long break
  resets the cycle count when the next focus starts.
- Header Back or the Home gesture exits. Leaving or manually sleeping ends the
  timer; reopening starts a new cycle. There is no saved timer or background alert.
- Opening **Timer settings** or Control Center pauses a running phase. It stays
  paused, with its time and session count, until you choose **Resume**.
- Running blocks auto-sleep. Ready, paused and completed screens use the normal
  configured sleep timeout. Manual sleep remains available while running.

## Timer settings

Tap the gear in the Pomodoro header. Each value row opens the shared slider dialog
(− / + / slider / Confirm); Back or a tap outside cancels.

| Setting | Range | Default |
| --- | --- | --- |
| Focus | 5–90 min | 25 min |
| Short break | 1–30 min | 5 min |
| Long break | 5–60 min | 15 min |
| Focus sessions per cycle | 2–8 | 4 |

Values are saved in the normal `/.crosspoint/settings.json` as
`pomodoroFocusMinutes`, `pomodoroShortBreakMinutes`, `pomodoroLongBreakMinutes`
and `pomodoroSessionsPerCycle`. Out-of-range values are clamped on load. They are
not listed on the device Settings screen or in the web settings API.

The last row, **Reset to defaults**, asks for confirmation (Cancel is selected
first) and then restores all four values above in one save. Cancel or Back
leaves them unchanged.

A new length applies immediately when the timer is Ready. A paused phase keeps
its current length; the new length applies after **Reset phase** or from the
next phase. If the cycle is shortened below the sessions already completed, the
next break is the long break.

Remaining time is calculated from monotonic elapsed milliseconds, including slow
renders and counter rollover. The screen normally updates about every ten
seconds, with immediate updates after actions and completion. It can therefore
show an older value between updates. Completion is a visual message, without
sound, vibration, flashing frontlight or automatic phase advancement.

Repaints use fast refresh, with a half refresh at the configured reader
"Refresh frequency" cadence to clear countdown ghosting. While running, the timer
blocks auto-sleep but still lets the CPU drop to its idle speed after three
seconds without input; rendering raises the speed through the existing power lock.

Without touch: Up/Down (Picco's page keys; arrow keys in the simulator) move
focus through the gear, the primary button and Reset (when shown), wrapping at
the ends. Confirm (Return in the simulator) activates the focused control. Focus
starts on the primary button and follows the last tapped control. Back exits on
boards that have it. Picco has no Back key and its GPIO5 Confirm role is still
unverified on hardware. Setting **Controls → Short click** to **Confirm** makes a short power press activate the focused control.

## Ownership and resources

`PomodoroActivity` owns a fixed-scalar `PomodoroTimer` (configuration plus state)
and one existing `UiAppHost`. Timer settings are four `uint8_t` fields in the
existing `CrossPointSettings` store, persisted by the generic settings loop.
`PomodoroSettingsActivity` is a four-row `UiListActivity` with 64 bytes of value
buffers; it is allocated only while open and reuses `IntervalSelectionActivity`.
There are no new workers, dependencies, timer storage files or SDK changes.

`Activity::allowIdlePowerSaving()` (default false) lets an activity that blocks
auto-sleep keep the idle CPU speed; only Pomodoro returns true, so every other
activity keeps its existing power behavior. `Activity::onCovered()` is a no-op hook called under the existing RenderLock
before an activity is pushed beneath a child. Only Pomodoro overrides it, to pause.
`UiAppHost` routes at most six actions, so Start also performs Resume.

The countdown temporarily binds the UI title slot to the built-in 18 pt sans
(as the shared slider readout does) and restores the theme title font. The Home
change adds one menu item, one tab, and a bounded viewport for crowded list
themes. The timer icon is a 128-byte immutable mask per consuming translation unit.
Native Picco link totals versus `28c042ae`: static RAM +8 bytes, flash +15,800 bytes.

This is a downstream feature, maintained alongside Todo; upstream CrossPoint's
interactive-app scope does not change. The phase workflow was researched in
SUMI, but the model and native integration here do not import its plugin/UI stack.

## Verification

From the firmware checkout, using the existing workspace environment:

```sh
cmake --build build/test -j 6
ctest --test-dir build/test --output-on-failure -j 6
../.venv/bin/python test/pomodoro/simulator.py
../.venv/bin/python test/pomodoro/simulator.py --power
../.venv/bin/python test/todo/simulator.py
../.venv/bin/python test/todo/reader_smoke.py
../.venv/bin/pio run -e simulator_picco
../.venv/bin/pio run -e picco
```

Tests use isolated cards. Do not delete the normal card's `.crosspoint` directory:
it contains Todo, settings and reading progress. Deterministic host model tests
exercise completion and the entire four-focus cycle without waiting in real time.
The simulator tests exercise actual touch controls, paint cadence, Home entry,
reset, exit/reopen, settings edit/persistence/clamping, Control Center pause, keys-only focus
navigation, the Home overflow indicator,
normal idle sleep, and manual sleep/relaunch.

Physical checks remain the owner's responsibility:

1. In each theme, open Pomodoro by touch, including a Home with Plugins enabled.
   All visible shortcuts must activate the item touched; swiping reveals overflow.
   In Lyra Extended, a scroll indicator must show at the menu's right edge.
2. Start, pause, resume and reset; change each timer setting, then leave and reopen. Check the displayed phase,
   count, native header and finger-sized controls. Repeat after reading in each
   of the four orientations; normal application UI returns to portrait.
3. Let a real 25-minute phase finish. Confirm `00:00`, one completed focus session,
   no automatic next phase, and no unexpected sleep while running. Verify manual
   sleep still works and wake does not resume a timer. Paused/ready/completed
   screens should sleep after the configured timeout.
4. Observe refresh ghosting (a half refresh should clear it periodically), timing
   and battery use; a running timer should draw about the same as an idle screen. Compare internal RAM and
   PSRAM free/largest blocks across repeated reader/Todo/Pomodoro transitions;
   host allocations are not physical heap measurements. Confirm tasks and reader
   progress survive those transitions.
5. Without touching the screen, press the page keys: focus must cycle through the
   gear, the primary button and Reset (when shown). Check which key, if any,
   activates the focused control (GPIO5, or a short power press when the power
   action is set to Confirm), and that nothing else starts, pauses or resets.

No upload, flashing or physical device access is part of the automated checks.
