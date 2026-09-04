# Current sprint

## Sprint 11

**Goal:** Persist per-node win% into the save-game file so a reloaded game keeps its WinGraph.
ANLZ-01 made every visited position carry a measured eval in memory; this makes those evals durable
— save a game with a populated win-rate graph, re-open it, and the graph comes back without
re-running analysis (the Sabaki/SGF `SBKV` precedent).

**Dates:** 2026-09-04 to — (open — no fixed end date set yet)

**Dependency graph:**
- **ANLZ-03** — model layer only: `src/model/game_io.cpp` (the `.yxgame` plain-text schema —
  add an optional per-move win% token, bump `kFormatVersion`), `src/model/game_state.{h,cpp}` /
  `src/model/variation_tree.*` (expose the per-node eval for serialisation, accept it on load,
  then `invalidateEvalHistoryCache()` / `treeDirty_`). **Additive + backward-compatible**: a file
  with no win% token loads exactly as today (all nodes NaN); old binary + new file must also load.
  **NaN round-trips as absence** — never write `0.5` for an unevaluated node (that is the exact
  UI-01 "false 50%" bug). **`kFormatVersion` is not `APP_VERSION`** — REL-02 single-sourced the app
  version; the save-file schema version is separate, and is the only version string this task
  touches (do not touch CMake `project(VERSION)` / `src/version.h.in`). Must not trigger analysis
  on load, must not change eval→win% maths, UI-01 attribution, ANLZ-04 bridge rendering, or
  `buildWinGraphSeries`. No new file format, no SGF — extend `.yxgame` in place. No
  `systematic-debugging` first (additive feature, not a bug). No "pick X with the user" design
  calls open — scoped directly from `docs/notes/2026-09-04-wingraph-analyze-mode-and-backfill.md`.
  See `docs/todo/ANLZ-03-persist-winrate-in-save-file.md` +
  `docs/instruction/ANLZ-03-persist-winrate-in-save-file.md`.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| ANLZ-03 | Persist per-node win% into the `.yxgame` save file so a reloaded game keeps its WinGraph (additive backward-compatible field, bumps `kFormatVersion` only) | follows ANLZ-01 (produces the evals this persists); relates to UI-13 (candidate A's derived reply-ply eval persists the same way), UI-01 (NaN ⇒ absent, never `0.5`), REL-02 (app version untouched) | — | 🔲 Not started |

Points not yet estimated (consistent with Sprints 3–10).

**Lesson carried in from Sprint 10:** split a pure helper out of any Cairo/GTK path for
testability (ANLZ-04's `computeGapBridges`, mirroring UX-06's `buildWinGraphSeries` split) — for
ANLZ-03, keep the win%-token parse/format as a pure function so the round-trip gets real
model-layer coverage without a display. Also carried: run `/sprint close` the same day the last
item merges (Sprint 10 held to this; keep it up).

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `/CLAUDE.md` ("Sprint cadence").
