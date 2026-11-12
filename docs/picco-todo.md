# Offline Todo (local fork)

This fork adds a built-in Todo application. Interactive apps are outside upstream
CrossPoint's reading-focused scope; this feature is maintained locally.

## Use

Open **Todo** on Home (the book icon at the end of the cover-grid toolbar).
Tap **New task**, type on the existing CrossPoint keyboard, and tap **OK**.
Tap the checkbox to toggle completion; tap the task description to edit it directly.
Completed tasks have a checkmark and strike-through on each visible text line.
Tap the bin at the top right to open **Delete tasks**. Select any number of tasks,
then tap **Delete selected (N)** and confirm. Selection checkboxes on this screen
mean deletion selection, independently of task completion. Back cancels selection;
no data is written until deletion is confirmed. A confirmed batch uses one save.
Canceling the keyboard or confirmation also leaves saved data unchanged.
The Todo header arrow returns Home; the deletion header arrow returns to Todo.
The shared bottom-edge Home gesture works from either screen. No hardware Home
key or provisional Confirm button is required.

Swipe within a list to scroll. Task labels wrap up to two lines and truncate
longer text; tapping the description opens its full text in the keyboard.
Maximum: 32 tasks, 192 UTF-8 bytes per task.
Leading/trailing ASCII whitespace is trimmed; blank text, control characters,
invalid UTF-8, and over-limit text are rejected. The keyboard enforces the byte
limit without splitting a character. There are no background tasks or network
connections for Todo, and each accepted change is saved immediately.

## Storage and recovery

On the microSD: `/.crosspoint/todo-a.json` and `/.crosspoint/todo-b.json`.
Simulator default: `crosspoint-reader/fs_/.crosspoint/`; override the SD root with
`CROSSPOINT_SIM_SD`. Paths stay the same inside the virtual card.

Version 1 JSON has `version`, `generation`, `nextId`, `checksum`, and `tasks`.
Each task has integer `id`, boolean `completed`, and string `text`. IDs increase
and are never reused after deletion. The checksum is FNV-1a over little-endian
uint32 version, generation, nextId, count, then each task's uint32 ID, one byte
completion, UTF-8 text, and a zero byte. It detects accidental damage; it is not
an authentication mechanism. Keep both snapshots when backing up data.

The newest valid generation is loaded. A new snapshot is written to `todo.tmp`,
checked for serialization length and successful close, then renamed over the
inactive slot. The active slot is retained. Incomplete temp files are ignored.
A failed save leaves the displayed model unchanged and reports an error; retry
or reopen Todo after resolving SD access. No-op edits and cancellation do not
write files. Missing or zero-byte files start empty. If both nonempty snapshots
are invalid (or no valid snapshot survives a damaged one), editing is disabled;
files are preserved. Exit Todo, back up both snapshots, inspect the card and
restore an intact backup. To deliberately start over, move both JSON files out
of `/.crosspoint/`, then reopen Todo. Do not clear unrelated `.crosspoint` data.

This scheme preserves the preceding snapshot across short writes, close failures,
and failed promotion. FAT/power-loss behavior cannot be made transactional by
this application; a physical card/controller failure can still corrupt both
copies. Host tests cover the algorithm, not electrical durability.

## Implementation and budgets

- `lib/Todo/TodoModel.*`: bounded tasks and UTF-8 validation.
- `lib/Todo/TodoStore.*`: versioned snapshots through HalStorage.
- `src/activities/todo/TodoActivity.*`: UiListActivity, keyboard result adapter,
  and ConfirmationActivity. ActivityManager owns activity lifetime.
- The SDK FreeInkUI list supplies separate leading-checkbox and label actions;
  text targets draw optional strike-through per wrapped/truncated line. These
  opt-in additions leave other lists unchanged. The SDK is on the local
  `feat/todo-list-controls` branch; keep its recorded submodule revision.
- Home menu and cover-grid toolbar expose the application.

The model is 6,416 bytes on the tested arm64 host. One model is resident inside
the checked activity allocation; load/mutation uses one checked temporary model.
ArduinoJson allocations are capped at 32 KiB per document plus allocation headers;
input files are capped at 32 KiB and nesting at four. These are bounds, not
measured ESP32 heap usage. The candidate model coexists with the live model and
JSON document only during I/O. The keyboard/confirmation retains Todo as its
parent; render rows are supplied lazily and retain no extra task strings.
Deletion selection uses a 32-bit mask, a 64-byte footer label, and a 128-byte
stack array of stable IDs during confirmation. Row minimum height permits at
most ten full rows, keeping separate checkbox/label hits within the existing
24-entry interaction table. Existing text wrapping may allocate temporary lines;
strike-through reuses those same lines.
There are no additional worker tasks, framebuffers, or dependencies. The existing
keyboard owns its own temporary text string. A future text input can replace the
`enterText`/`acceptText` boundary without changing the model or storage.

## Verification

From the firmware checkout after building `simulator_picco`:

```sh
sh test/todo/run.sh
../.venv/bin/python test/todo/simulator.py
../.venv/bin/python test/todo/reader_smoke.py
sh freeink-sdk/libs/ui/FreeInkUI/test/host/run.sh
```

The first runs model/persistence checks under AddressSanitizer and UBSan using
ArduinoJson and an injected storage adapter. The second uses real firmware,
keyboard, dialogs and host-backed SD with scripted SDL touch events. Test data
and logs stay under the parent workspace's `qa-artifacts/` and `logs/`.

Physical Picco validation remains required: touch calibration, all orientations,
SD removal/write failures and power interruption, repeated keyboard/task cycles
and internal/PSRAM largest free blocks, e-ink refresh/ghosting, sleep/wake and
frontlight/slide switch behavior. No hardware validation or flashing is implied
by simulator checks. Other languages currently use the English fallback for new
Todo labels; displaying Unicode depends on installed glyph coverage.
