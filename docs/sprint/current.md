# Current sprint

## Sprint 28

**Goal:** Design the analysis workspace for a new game and live analysis

**Dates:** 2026-11-18 to — (open — no fixed end date set yet)

**Dependency graph**

- [UX-10](../todo/UX-10-analysis-workspace-design.md) covers main-window UI design. The baseline is UI-08, UI-23, and the collapsible panels from PR #66. Deliver two mockups and an interaction spec under `docs/design/`. Keep application code and engine behavior outside the scope. No defect investigation or pending design approval is required to draft the proposal. [Dispatch instructions](../instruction/UX-10-analysis-workspace-design.md)

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| UX-10 | Design the analysis workspace for a new game and live analysis | UI-08, UI-23, PR #66 (shipped baseline) | — | 🔲 Not started |

Points have yet to be estimated.

**Lessons carried in from Sprint 27**

- Rerun the build and ctest and review the visuals on the dispatched branch. For headless UI runs, use `xvfb-run` with `GDK_BACKEND=x11` and `WAYLAND_DISPLAY` unset.
- Write the instruction file before dispatch so the agent has the boundaries and verification criteria.

See [burndown](burndown.md) for progress and the [Sprint 27 archive](archive/sprint-27.md) for the previous sprint.
