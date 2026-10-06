# Current sprint

## Sprint 22

**Goal:** Board mark visual language: stacked engine/database/variant marks, best ring, hover tooltips

**Dates:** 2026-10-06 to — (open — no fixed end date set yet)

**Dependency graph:**
- **UX-07** — `src/model/board_view_model.{h,cpp}` (stacking + index map + DB best/variant count) then `src/ui/board_renderer.{h,cpp}` (layer order, outlined DB diamond / corner badge, ring+dot variants); tests in `tests/`. Design resolved (`features/board-marks/planning.md`); read `docs/instruction/UX-07-*.md` first. Must not add settings, a new colour ramp, or on-board bound/comment glyphs; `ui → model` only. Not a bug fix, so no `systematic-debugging`. Size L: consider splitting into real CODEs (model / renderer) if the PR grows — letter suffixes are not allowed.
- **UX-08** — `src/ui/board_view.cpp` (`query-tooltip`, hover crosshair, margin label highlight) + ViewModel tooltip text. Starts by investigating where `DatabaseEntry::hasComment` comes from (planning Q12). Soft-depends on UX-07 (mark data model settled once); keyboard navigation out of scope.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| UX-07 | Redesign board mark language (engine / database / variant / best) | — | — | 🔲 Not started |
| UX-08 | Board hover crosshair, margin highlight, mark tooltips | UX-07 (soft) | — | 🔲 Not started |

Points not yet estimated (consistent with Sprints 3–…).

**Lesson carried in from Sprint 21:** A render/pixel regression pattern now exists (`test_ui16_hover_occupied.cpp`, `test_ui17_renderer_state_isolation.cpp`) — reuse it. Live/visual checks are still human-owed: UI-18's ring geometry was never seen on a real board, so verify it with screenshots in the UX-07 PR.

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `.claude/rules/sprint-cadence.md`.
