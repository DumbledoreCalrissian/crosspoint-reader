# Claude handoff — BOOX Picco EPUB reading stats

## Read this scope correction first

Target hardware is BOOX Picco; new statistics support is for EPUB only.
The owner rejected the earlier multi-format handoff. Do not introduce other
device targets or cross-device validation requirements for this effort.
This note and `../../READING_STATS_PLAN.md` supersede the previous five-milestone
plan. Do not implement XTC, TXT or Markdown statistics. Do not add overall totals,
an aggregation/settings page, new themes or navigation redesign.

## Verified starting point

Firmware source was audited after Claude's undo on 13 November 2026:

- `picco` at `119344fb` (`feat: add local reading stats foundation`).
- Firmware tracked/untracked source was clean before this documentation revision.
- Every accepted milestone-1 file matched the saved SHA-256 review manifest.
- SDK clean at `8f7f00709229f5b1ba73b1ac1cd8d571e9ff1354`.
- Simulator source clean at `0fa04a9`.
- No remaining reading-stats feature branch, extra commit or stash was found.
- The 37 foundation tests passed again after rebuilding their target.
- `simulator_picco` was rebuilt successfully from the restored source.

The source undo is intact. This does not identify the cause of Claude's failed
attempt: its discarded edits are not present for review. Build artifacts can
outlive an undo; rebuild from current source before trusting a simulator binary.
Audit evidence is in `../../logs/reading-stats-undo-audit-*`.

Milestone 1 is accepted and committed. `src/util/ReadingStats.{h,cpp}` and
`ReadingStatsStore.{h,cpp}` are the existing foundation. Read
`docs/picco-reading-stats.md` for their exact contracts and failure behavior.
They are not connected to the reader or UI yet. No further commit/push is
authorized by the milestone-1 commit approval.

## Your next task: milestone 2 only

Implement working EPUB statistics and a small book-statistics/reset view.
Stop after this milestone is implemented, tested and reviewed. Do not start
Library/Home work in the same pass.

1. Read AGENTS.md and applicable rules. Inspect the current EPUB lifecycle and
   existing menu/dialog components before choosing insertion points.
2. Give the active EPUB one collector and one store writer. Start timing after
   a readable page is visible. Settle/pause for menus, overlays, loading, exit
   and sleep; resume on the displayed page. Repaints/failed turns preserve dwell.
3. Keep the accepted rules: dwell 2,000–300,000 ms inclusive; discard long idle
   spans; only ordinary forward departures add turns. Backwards/jumps can add
   time. Count final-page time on exit/sleep exactly once. No trusted date needed.
4. Load once. Retain pending totals on save failure. Checkpoint settled dirty
   totals at page transitions no more than once per minute; force save on pause,
   exit and sleep. Never split an active dwell to checkpoint it. Respect corrupt,
   unsupported and unreadable record status. Preserve plugin-session semantics.
5. Add an EPUB-menu Book statistics action using existing themed components.
   Show progress, time and forward turns. Confirm reset with Cancel first;
   preserve reading position/bookmarks and prevent stale totals returning.
6. Prove known EPUB workflows produce expected totals and survive restart; test
   pause/resume, idle, last-page sleep, reset and write-failure/retry. Run native
   Picco/simulator builds and repository-required reviews; record results.

Important: EpubReaderActivity also handles TXT/Markdown. Gate new collection
and menu actions on actual EPUB paths. Prefer EPUB-local state. A shared hook
is justified only when needed for EPUB lifecycle correctness; it is not an
invitation to integrate every reader. Do not change XtcReaderActivity for stats.
Do not add a worker, database, generic instrumentation framework or theme.

## After milestone 2: milestone 3

Add EPUB percentages at the left of Library book rows and current-EPUB
percentage/bar/reading-time on Home in all existing themes. Keep layout changes
small and retain app access. Cover Grid already has percentage/bar. Use full
paths for Library records and bounded visible-window caching. Do not read full
EPUB metadata on every repaint. Existing other-format progress stays unchanged.

The complete revised plan is at `../../READING_STATS_PLAN.md`. Verification is
part of each milestone, not a separate feature milestone. Keep this note and
`../../PROGRESS.md` current. Physical device checks are not simulator claims.

## Environment and baseline checks

From `/Users/vc/Documents/crosspoint_picco_todo/crosspoint-reader`:

```sh
cmake --build build/test --target ReadingStatsTest -j 6
./build/test/reading_stats/ReadingStatsTest
../.venv/bin/pio run -e simulator_picco
../.venv/bin/pio run -e picco
```

Use the installed Python 3.13/PlatformIO workspace environment. Preserve caches
and local settings. PlatformIO may require sandbox approval for its existing
user-level cache lock. Stay in the current checkout; do not switch to upstream
branches or create another worktree without a concrete need. No commit, push,
PR or physical flashing action is authorized by this handoff.
