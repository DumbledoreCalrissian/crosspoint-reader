---
name: scope-discipline
description: Check proposed firmware surface against this Picco fork's owner requirements and scope. Use before adding features, activities, libraries, settings or dependencies.
---

# Scope discipline for the Picco fork

Read [SCOPE.md](../../../SCOPE.md) and the current owner request. This fork
intentionally includes Todo, foreground Pomodoro and Sudoku. Do not reject their
maintenance because upstream excludes interactive apps. Upstream plans and
research are references, not authorization to expand the current task.

## Before adding surface

- Identify the concrete owner requirement. Existing accepted features do not
  authorize every possible extension. If a proposal exceeds the current task,
  explain the boundary and clarify only what cannot be inferred from the request.
- For reading statistics, use the [current handoff](../../../docs/reading-stats-handoff.md).
  New work is EPUB-only on Picco. Do not introduce other-format/device integration,
  overall aggregation, new themes or navigation redesign as implied follow-ups.
- Trace existing settings, activities, UI components, HAL and pinned SDK APIs.
  Stop at the smallest mechanism that meets the requirement; leave speculative
  registries, generic instrumentation and future hooks for a concrete need.
- Account for peak RAM, internal/PSRAM placement, largest blocks, stack, flash,
  storage and power on the selected Picco profile. Preserve inherited shared-code
  compatibility without adding unrelated device validation targets. Label
  estimates separately from measurements; runtime power/heap claims need hardware.
- Define state, resource and failure ownership, including retained parent
  activities, storage recovery, network/task lifetime, retries and exit/sleep
  cleanup. Follow the [activity rules](../../rules/ui-activities.md).
- Explain the maintenance cost of new settings, dependencies, persistent formats
  and failure paths. A setting needs a user need; a fixed sensible default is
  preferable when no choice is required.

Do not delete inherited format/device support merely to narrow the fork's identity.
Keep upstream submission decisions separate from downstream acceptance.
