# Scope of the Picco custom firmware

This is an independently maintained BOOX Picco build derived from CrossPoint
Reader. EPUB reading is the primary use case. Todo, Pomodoro and Sudoku are
intentional downstream features, even though upstream has a different product
policy. Upstream acceptance rules do not decide this fork's scope.

## Authority and current work

The owner's current request defines the work being authorized. This document
sets the fork's product boundaries; [AGENTS.md](AGENTS.md) defines agent workflow.
The [reading-statistics handoff](docs/reading-stats-handoff.md) records the current
statistics milestones and supersedes earlier multi-format plans. The workspace
`READING_STATS_PLAN.md`, when present, contains the detailed accepted plan.

Historical logs, archived review snapshots, research clones and upstream plans
are evidence or reference material. They do not authorize another feature or
replace current instructions. Resolve a real conflict against the current
request and implementation; ask the owner only when the answer remains unclear.

## Accepted product areas

- EPUB reading, Library, typography, bookmarks, dictionary and clippings.
- Maintenance of the existing Todo, foreground Pomodoro and Sudoku applications,
  including their storage, navigation and Sudoku pack downloads.
- EPUB reading statistics on Picco, within the milestones below.
- Small shared UI, input, HAL and resource fixes that a concrete accepted feature
  needs, with existing defaults and ownership preserved.

These areas are maintenance boundaries, not permission to start every possible
feature within them. Tie each addition to the owner's request. Prefer existing
mechanisms and the smallest maintainable implementation. Account for new state,
settings, allocations, dependencies and failure paths.

## Reading-statistics boundaries

Milestone 1 is accepted and committed as `119344fb`: collector and recoverable
per-book storage only. It does not collect from the reader or expose UI yet.

Milestone 2 connects actual EPUB reading to that foundation and adds a small
book-statistics/reset view. Stop for owner review before milestone 3.
Milestone 3 adds EPUB percentages to Library and current-EPUB progress/time to
Home across the five existing themes, keeping app and plugin access intact.
Tests, builds and reviews belong inside each milestone.

No statistics integration for XTC, TXT or Markdown, and no additional device
validation target is required. No overall totals/settings page, aggregation,
calendar/session counts, estimates, database or background service. New themes,
a carousel and Home navigation redesign are separate requests, not dependencies
or later statistics milestones. Shared EpubReaderActivity also handles other
formats; gate statistics on actual EPUB paths.

## Compatibility and resources

Target new feature work and verification at Picco and `simulator_picco`.
Inherited other-device and other-format support is not scope drift and must not
be deleted merely to narrow the fork's identity. Preserve existing behavior
when touching shared code. Broaden checks only for a concrete changed contract;
do not turn a Picco task into a new device port or multi-device project.

Use the pinned SDK through the firmware HAL and existing UI components. Budget
Picco internal RAM and PSRAM separately, including peak coexistence and largest
blocks. Preserve fallible allocations and storage recovery. Simulator memory,
power and restart behavior cannot establish physical hardware results.

## New proposals and upstream work

Additional apps, themes, formats, devices, connectors and architectural changes
need a separate owner requirement before implementation. Their presence in an
upstream roadmap or research project does not make them planned work here.
If proposing a change upstream, evaluate it separately against upstream's own
rules; downstream acceptance does not imply upstream acceptance.
