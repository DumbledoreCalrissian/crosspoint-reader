# Picco custom firmware roadmap

This roadmap describes this fork. It does not inherit CrossPoint's phases,
feature prohibitions or device-expansion plans. [SCOPE.md](SCOPE.md) defines
boundaries; an item here is not permission to start work without an owner request.

## Implemented; hardware verification pending

Todo, foreground Pomodoro, Sudoku and its puzzle-pack downloads, File Transfer
navigation and clippings-export filtering are implemented. Native Picco builds
and desktop simulator checks are recorded. No physical Picco has been flashed
or verified by this project. See the linked application guides in [README.md](README.md)
for behavior, storage recovery and physical test steps.

## Unfinished: EPUB reading statistics

1. **Foundation complete:** collector/store, accepted and committed as `119344fb`.
2. **Next milestone:** actual EPUB collection and a small book-statistics/reset
   view. Reader integration and UI remain unimplemented. Stop for owner review
   before the next milestone.
3. **Later milestone:** EPUB percentages in Library and current-EPUB progress/time
   on Home in the existing themes, preserving application and plugin access.

Verification is part of each milestone. The [current handoff](docs/reading-stats-handoff.md)
contains the continuation instructions. No milestones for other-format/device
statistics, overall aggregation, new themes or navigation redesign follow this plan.

## Maintenance and future decisions

Fix demonstrated regressions and keep the implementation, tests and guides in
agreement. Physical Picco validation remains separate from simulator results.
Other proposed features need a separate owner decision; no additional theme,
device port, format integration or background service is scheduled by this file.
