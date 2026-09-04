# RDB-03 — Persist + restore per-node analysis end-to-end (closes the ANLZ-03 goal)

**Status:** 🔲 OPEN (Active — Sprint 11) [Model: Sonnet 5]
**Area:** `src/model/variation_tree.h` (extend `TreeNode` — or add a `NodeAnalysis` member — with
the persisted analysis fields the DTO already models). `src/model/rdb/game_graph_convert.cpp`
(populate/restore the full `NodeAnalysis` in both directions). `src/model/game_state.{h,cpp}`
(`evalHistory()` gate; `invalidateEvalHistoryCache()` after load). `src/main_window.cpp` (load path
applies analysis onto the rebuilt tree). `tests/`.
**Priority:** P2
**Source:** `features/rdb-save-format/` — user-approved 2026-09-04. Third of three tasks. This is
where the **original ANLZ-03 goal is delivered**: save a game with a populated WinGraph, re-open
it, the graph is back with no re-analysis. Integration branch `feat/rdb-save-format`.
**Design:** `features/rdb-save-format/diagram/container.md` ("Mapping to the current in-memory
model" + "Load algorithm" + the `evalHistory` note).
**Depends on / relates to:** **RDB-01** (DTO + CBOR + convert) and **RDB-02** (Save/Open wiring) —
hard dependencies, both first. Carries the regression-test intent of **ANLZ-03** (NaN round-trips
as absence — the UI-01 "false 50%" bug; old-`.yxgame`-import; out-of-range value). **UI-01**
(NaN = unevaluated), **UI-13** (candidate-A derived child evals persist the same way),
**ANLZ-01** (produces the evals this makes durable), **REL-02** (app version untouched).

## Problem

After RDB-02, `.rdb` round-trips the *tree shape* + `eval`/`nodes`/`depth`/`comment`, but:
- `evalHistory()` only trusts `node->eval` when `node->depth > 0 || node->nodes > 0`. A restored
  node must carry `depth`/`nodes` too, or that gate must loosen — otherwise a saved win% loads but
  the WinGraph still shows a gap.
- PV, annotation glyph, engine reference, analyzed-at timestamp have nowhere to live on `TreeNode`.
- Nothing verifies the *full* save→reopen→WinGraph-identical path end-to-end.

## Scope (in order)

1. **Extend the model.** Add to `TreeNode` (directly, or as a `std::optional<NodeAnalysis>`
   member — pick whichever keeps `variation_tree.h` cleanest): `evalText` (string), `pv`
   (`vector<Coord>`), `annotationGlyph` (string), `engineRef` (small int/index), `analyzedUtc`
   (int64). `eval`/`nodes`/`depth` stay where they are. Keep the NaN sentinel semantics for `eval`.
2. **`evalHistory()` gate.** Change `(node->depth > 0 || node->nodes > 0) ? node->eval : NaN` to
   trust `node->eval` whenever it is not NaN (`!std::isnan(node->eval)`). Rationale: a restored
   node legitimately has a real eval with `depth == 0` if we chose not to persist depth; making
   "eval is not NaN" the single source of truth is simpler and matches the DTO's "absent ⇒ NaN"
   rule. **Alternative** (lower churn): always persist `depth`/`nodes` alongside `winrate` in
   `toGameGraph` so the existing gate passes unchanged — decide in the instruction file, note
   which in the fix-log.
3. **`game_graph_convert.cpp`.** `toGameGraph`: for every non-NaN node write the full
   `NodeAnalysis` (winrate + depth + nodes + evalText + pv + engineRef + analyzedUtc as available);
   NaN node ⇒ no `analysis` block at all. `applyGameGraph`: restore every field; validate
   `winrate ∈ [0,1]` else leave `eval` NaN and drop the block; never abort the load on a bad value.
4. **Load path** (`main_window.cpp` / a `GameState` helper): after the tree is rebuilt from the
   `GameGraph`, the per-node analysis is applied, then `invalidateEvalHistoryCache()` and the
   `signal_tree_updated` / `signal_board_changed` emissions so the WinGraph repaints from restored
   data. No engine call — loading never triggers analysis.
5. **`engines[]` metadata.** Populate `GameGraph.engines` on save from the current
   `EngineConfig` (name/path/params as available); on load keep it for display only (no behaviour).
   Thin — a single entry is fine.
6. **Fix-log** `docs/fix-log/2026-<mm-dd>-rdb-persist-node-analysis.md` + `docs/fix-log.md` row
   (this closes ANLZ-03's tracked goal). **`CHANGELOG.md`** `[Unreleased]`: "Re-opening a saved
   game restores its win-rate graph without re-running analysis."
7. **Tests** (`tests/test_rdb03_node_analysis.cpp`, `ranls-gui-tests`) — the ANLZ-03 regression
   set, carried here:
   - Mixed evaluated/NaN tree → `toGameGraph` → CBOR → container → back → `applyGameGraph` →
     `evalHistory()` matches the original vector **exactly**, NaN positions still NaN.
   - A NaN node never serialises a `winrate`; a restored NaN node never renders as `0.5`.
   - Legacy `.yxgame` import (via `YxgameReader`) → every node NaN, no crash, WinGraph empty as a
     fresh game.
   - `winrate` = `1.7` in a hand-built `GameGraph`/CBOR blob → treated as absent, load succeeds.
   - PV / evalText / comment on a node survive the full round-trip.
   - A `ranls-gui-ui-tests` case: `MainWindow` — analyse-stub a couple of nodes (set evals on the
     tree directly), save `.rdb`, `newGame`, open it, assert `gameState_.evalHistory()` restored.

## Acceptance criteria

- Save after a populated WinGraph, re-open → the WinGraph is identical (every persisted point
  back, NaN gaps still gaps; ANLZ-04 bridge still bridges them, unchanged).
- A restored non-NaN node shows its point on the graph (the `evalHistory` gate no longer hides it).
- NaN never serialises; a restored NaN node is never 50 %.
- Legacy `.yxgame` still imports as all-NaN.
- Out-of-range persisted value ⇒ treated as absent, load does not abort.
- `./build.sh` clean; `ctest` 3/3 including the new `test_rdb03_node_analysis.cpp` + UI case.
- `docs/fix-log.md` row + detail; `CHANGELOG.md` line.
- Manual (needs a human — no display/engine on the build host): analyse a real game, save `.rdb`,
  reopen, confirm the graph visually — noted in the fix-log as outstanding.

## Scope boundary

- Do **not** trigger analysis on load. Restoring measured data only; backfilling unanalysed nodes
  is out of scope (as it was for ANLZ-03).
- Do **not** change eval→win% maths, UI-01 attribution, ANLZ-04 bridge rendering,
  `buildWinGraphSeries`, or the WinGraph drawing.
- Do **not** add a Settings entry or a `ViewConfig` flag — persistence is unconditional.
- Do **not** touch `APP_VERSION` / CMake `project(VERSION)` / `src/version.h.in`. Only the `.rdb`
  `schema` field moves if the DTO shape changes.
- No multi-game, no SGF/RenLib import (follow-ups).
- Keep UI-13 candidate A's derived-child write intact — a restored real eval and a UI-13 derived
  eval coexist exactly as they do in memory today.
