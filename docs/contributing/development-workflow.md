# Development workflow for the Picco fork

## Start from the actual checkout

Inspect the branch, remotes, status and SDK gitlink. The downstream integration
branch is `picco`; keep a suitable current checkout unless the task requires
another branch. `develop` is an upstream comparison base, not an instruction to
switch branches or retarget this fork's work. Local remote names vary: verify
URLs instead of assuming that `origin` is upstream or a personal fork.

Read [SCOPE.md](../../SCOPE.md) and the current task/handoff. Preserve unrelated
uncommitted changes. Keep each change focused, and trace the implementation
before adding settings, state, dependencies or abstractions.

## Verify the changed behavior

Use the existing environment and explicit `picco` / `simulator_picco` profiles.
Run relevant host checks, use `./bin/clang-format-fix -g` for changed tracked C/C++
sources, and inspect `git diff --check`. Record the revision, commands and results.
Use the application-specific test guides for the affected paths. The
[testing guide](testing-debugging.md) distinguishes host, simulator and device checks.

Retained upstream CI definitions are not proof of checks on this fork. Firmware
changes follow [AGENTS.md](../../AGENTS.md) and its firmware-handoff workflow;
documentation-only changes use the lighter applicable checks. Preserve green
results unless a change or failure invalidates them.

## Commit, review and publication

Agents require explicit owner approval for local commits. Approval to edit or
commit does not authorize pushing, release publication, PR actions or flashing.
The human owns PR descriptions and publication. If the owner chooses a downstream
PR, its integration target is `picco` unless explicitly specified otherwise.
Upstream proposals are separate work and follow upstream's current rules.

Describe concrete behavior, resource ownership, storage compatibility and test
limits. Be clear whether a feature is planned, implemented, tested, committed or
hardware-verified. Respond to review respectfully and keep technical evidence
separate from assumptions. See [GOVERNANCE.md](../../GOVERNANCE.md).
