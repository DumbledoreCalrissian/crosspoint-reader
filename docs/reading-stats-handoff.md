# Reading statistics handoff for Claude

## Current state

The owner accepted the milestone-1 counting/storage design and maintenance in
this downstream fork on 13 November 2026 and explicitly authorized its local
commit. That acceptance completes milestone-1 firmware handoff; do not request
it again unless the design changes materially. This does not authorize future
commits or pushes.

Milestone 1 is complete: `src/util/ReadingStats.{h,cpp}` contains the pure
`readingstats::Collector`, and `src/util/ReadingStatsStore.{h,cpp}` contains the
recoverable per-book store. `test/reading_stats/` has 37 focused tests, registered
in the existing CMake test tree. Read `docs/picco-reading-stats.md` for the exact
implemented contract, format, errors and ownership.

The foundation is not connected to reader activity, Home, Library or settings.
No reading statistics are collected by running firmware yet. Existing reading
position and plugin-session behavior remain unchanged.

Work is on firmware branch `picco`, based on `a7b020d1`; the commit containing
this note is the milestone-1 implementation. SDK remains
`8f7f00709229f5b1ba73b1ac1cd8d571e9ff1354`. Simulator code is unchanged by this
milestone. No physical device has been accessed or flashed.

## Owner's agreed scope

Deliver a complete local stats feature that is useful even without later work:

- Support EPUB, TXT/Markdown and XTC.
- Retain existing saved-position percentages; accumulate reading time and
  qualifying forward page turns. No calendar, sessions, streaks, finished-book
  counts, estimated time left or network dependency.
- Library: percentage in a fixed column on the physical left of each book row.
- Home: current-book percentage, thin bar and accumulated reading time in every
  existing theme, with minimal visual changes.
- Book statistics: progress, time, forward turns and a confirmed reset that
  preserves saved position and bookmarks.
- Overall statistics under System settings: time and forward turns across
  retained records, including deleted books. Resetting a book removes its
  contribution; do not describe these as immutable lifetime totals.

Keep the five current themes and app access. A new Inx-inspired theme, carousel
and the requested explicit Home navigation improvement are separate follow-ups.
Do not expand this task into those changes.

## Continue with milestone 2: reader integration

Read `AGENTS.md` and applicable `.agents/rules/` and skills first. The full
workspace plan and research are at `../../READING_STATS_PLAN.md` and
`../../READING_PROGRESS_RESEARCH.md` relative to this document. The workspace
progress log is `../../PROGRESS.md`. These files live outside the firmware Git
repository; keep progress current there as well as in this handoff note.

1. Trace `ReaderActivity`, both concrete reader implementations, internal menus,
   dictionary/footnote overlays, Control Center, render-complete notification,
   auto-turn and remote/touch/button turns. The base lifecycle does not cover
   every pause/navigation path. Avoid broad refactoring.
2. Give each active reader one collector and one store writer. UI views may read
   snapshots but must not save stale copies. Load totals once and retain pending
   increments on save failure. Respect blocked/corrupt/unsupported load results.
3. Start timing only after a readable page is displayed; exclude load/render
   work and overlays. Preserve dwell on ordinary repaint and failed turns.
   Settle navigation/pauses and resume with a fresh dwell. Count final-page time
   on exit/sleep, with idempotent settlement. Preserve the existing plugin event
   contract and its different thresholds.
4. Count dwell from 2,000 through 300,000 milliseconds inclusive. Longer idle
   intervals contribute nothing. Only an ordinary successful forward departure
   after a qualifying dwell adds a turn. Backward/jump departures may add time.
5. Checkpoint dirty settled totals at page transitions no more often than once
   per 60 seconds; force saves on pause, exit and sleep. Do not split pending
   dwell to checkpoint, because that would bypass idle filtering. Reuse existing
   synchronization and non-blocking save-error UI mechanisms; add no worker.
6. Prove known reading scripts persist expected totals after restart without a
   trusted date or plugin subscription, for every supported format and input
   path. Verify settings/overlay entry, sleep, orientation/layout changes and
   save failure/retry. Measure actual linked resource deltas now callers exist.

## Remaining milestones

**3 — Usable statistics views.** Reuse native themed list/dialog components.
Expose Book statistics in supported reader menus, with Cancel-first reset.
Expose overall time/turns in System settings. Aggregate authoritative per-book
records one at a time, only when the overall view opens, yielding cooperatively.
Count only the winning `a`/`b` generation once, never temporary files. Report
incomplete/corrupt data honestly. Avoid a second global totals file and its
cross-file transaction problem. Preserve history through cache clears and book
deletion. Verify reset while a reader exists cannot resurrect stale totals.

**4 — Library and Home.** Reuse/extend the saved-progress helper carefully:
`loadBookProgress()` allocates book metadata and should not run for every row on
every paint. Library must use full paths via `index.readPath()`; its existing
row text helper returns a filename for indexed rows but a full path for pinned
Recent entries. Cache bounded visible-window work, not the entire library.
Cover Grid already shows current-book percentage/bar; add time there and add
progress/time to Classic, Lyra, Lyra Extended and RoundedRaff. Preserve app
shortcuts, Plugins overflow, cover-cache repaint and matching touch geometry.
Use one rounding/end-position policy, with unavailable progress shown as an em
dash. Verify EPUB versus XTC final-page semantics before changing the helper.
The shared SDK list currently has no left-side text slot; add an optional one
only if required, retaining defaults for all other callers. Pin SDK changes
deliberately and document their resource/layout cost.

**5 — Validation and handoff.** Test both readers, all themes, all four reader
orientations, SD fonts, large-library latency and app transitions. Run final
native Picco/simulator builds and applicable regressions. Follow
`.agents/skills/firmware-handoff/SKILL.md`: main agent verifies, four independent
read-only review axes, at most one substantive follow-up round, concrete human
acceptance. Provide physical Picco checks for SD durability/latency, memory/stack,
ghosting and touch. Do not claim simulator heap or restart mocks prove hardware
behavior. No flashing, pushing or PR actions are authorized.

## Milestone-1 evidence and commands

Verification on the accepted candidate:

- 37 focused tests passed under AddressSanitizer and UndefinedBehaviorSanitizer.
- All 968 CMake/CTest tests passed.
- Native `picco` and `simulator_picco` builds passed.
- Four independent read-only reviews found no actionable issues.
- Native debug sizes: Totals 16 bytes, Store 632 bytes, Workspace 2,136 bytes.
  Workspace is checked, allocated once and reused; collector has no allocation.
- Native flash 5,522,762 bytes, static DRAM 112,676 bytes, unchanged because the
  unreferenced foundation is removed at link time. This is not its integrated
  runtime or flash cost. Physical memory/stack/storage behavior is unverified.

From the firmware checkout:

```sh
cmake -S test -B build/test -DFETCHCONTENT_UPDATES_DISCONNECTED=ON
cmake --build build/test --target ReadingStatsTest -j 6
./build/test/reading_stats/ReadingStatsTest
ctest --test-dir build/test --output-on-failure -j 6
../.venv/bin/pio run -e picco
../.venv/bin/pio run -e simulator_picco
./bin/clang-format-fix -g
```

The workspace `.venv` has the working Python 3.13/PlatformIO installation. Do
not replace it or clean existing build caches. PlatformIO needs access to its
user-level cache/lock outside the sandbox. This checkout is the personal Picco
fork; do not switch to upstream develop just because generic rules mention it.

Full logs are at `../../logs/reading-stats-m1-*`. The frozen review diff,
SHA-256 manifest, review summary and native type sizes are under
`../../qa-artifacts/reading-stats-m1/`. These workspace artifacts are not committed
firmware files. The optional review suggestion to add an equal-generation,
different-total ambiguity test remains advisory; that branch was reviewed
correct by inspection. No unresolved review finding blocks milestone 2.
