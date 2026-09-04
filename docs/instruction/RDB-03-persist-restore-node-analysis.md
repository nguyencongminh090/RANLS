# Instruction — RDB-03: persist + restore per-node analysis (closes ANLZ-03)

## Approach

RDB-01 + RDB-02 must be in place on `feat/rdb-save-format`. This task fills the gap between "the
tree shape round-trips" and "the WinGraph comes back".

Two decisions to make and record in the fix-log:

### D1 — the `evalHistory()` gate

`GameState::evalHistory()` (src/model/game_state.cpp ~L400):
```cpp
evalHistoryCache_.push_back((node->depth > 0 || node->nodes > 0)
                                 ? node->eval
                                 : std::numeric_limits<double>::quiet_NaN());
```
- **Preferred:** change the condition to `!std::isnan(node->eval)`. Single source of truth,
  matches the DTO's "absent ⇒ NaN" rule, and a restored node needs nothing but its `eval`.
  Re-check the two existing callers/tests (`test_ui01_winrate_attribution.cpp`,
  `test_ui13_wingraph_eval_coverage.cpp`) — a node that has never been analysed still has
  `eval` at the NaN sentinel (verify `TreeNode::eval` default; it is `0.0` in the header today —
  **this matters**, see Pitfalls).
- **Fallback (lower risk):** keep the gate, and have `toGameGraph` always emit `depth` + `nodes`
  next to `winrate`, and `applyGameGraph` always restore them, so the gate passes. Choose this if
  changing the gate ripples into UI-01/UI-13 tests.

### D2 — `TreeNode` shape

Add either loose fields or a `std::optional<NodeAnalysis>` member. `NodeAnalysis { std::string
evalText; std::vector<Coord> pv; std::string glyph; int engineRef = -1; int64_t analyzedUtc = 0; }`.
Keep `eval` / `nodes` / `depth` where they are (lots of call sites). Prefer the optional member —
it keeps `TreeNode`'s common case small and makes "was this analysed" one check.

## Pitfalls

- **`TreeNode::eval` defaults to `0.0`, not NaN.** Grep every writer of `node->eval` / every
  `addChild`/`addMove` path. If you switch the `evalHistory` gate to `!isnan(eval)`, a
  freshly-added unanalysed node with `eval == 0.0` would suddenly plot as a real 0.0 (= a
  catastrophic loss for Black). **Either** initialise `TreeNode::eval` to
  `std::numeric_limits<double>::quiet_NaN()` (and fix any code that assumes 0.0), **or** take the
  D1 fallback. This is the single riskiest point in the task — decide deliberately, test both a
  fresh game and a loaded game.
- **NaN still never serialises.** Unchanged from RDB-01: NaN node ⇒ no `analysis` block ⇒ loads
  back as NaN. Add the explicit "restored NaN node is not 0.5" assertion here too (regression
  guard travels with the code, per CLAUDE.md).
- **`winrate` range.** `[0,1]` — validate in `applyGameGraph`; out-of-range ⇒ drop the block,
  leave `eval` NaN, load continues.
- **PV coords vs board size.** A persisted `pv` move could be off-board if the file is hand-edited
  or corrupt — validate each against `graph.board`; drop the offending pv rather than abort.
- **Don't poke the engine.** The load path applies data and emits repaint signals only.
  `controller_.sendConfig()` (position resync) is the only engine interaction and it already
  exists in `onLoadGame`.
- **UI-13 coexistence.** UI-13 candidate A writes a derived eval onto an existing played-line
  child at `depth > 0`. A restored real eval and a UI-13 derived eval must coexist exactly as in
  memory now — do not add logic that treats "restored" specially. The `setAnalysisData`
  "only if changed" + `treeDirty_` path is untouched.
- **`engines[]`** is display-only metadata. Populate one entry from `EngineConfig` on save; on
  load, store it but change no behaviour. Do not let a missing/empty engines list fail the load.

## Verification before marking this task done

1. `./build.sh` clean (3 known warnings only).
2. `ctest` 3/3.
3. **`tests/test_rdb03_node_analysis.cpp`** (`ranls-gui-tests`) — the carried-over ANLZ-03
   regression set, all required:
   - mixed evaluated/NaN tree → full round-trip (convert→CBOR→container→back) → `evalHistory()`
     equals the original vector exactly, NaN positions still NaN;
   - NaN node serialises no `winrate`; restored NaN node ≠ 0.5;
   - legacy `.yxgame` via `YxgameReader` → all nodes NaN, no crash, empty WinGraph like a fresh game;
   - `winrate = 1.7` in a crafted blob → treated as absent, load OK;
   - PV + evalText + comment survive a node round-trip;
   - a **fresh** game (no load) still produces an all-NaN `evalHistory()` — proves the
     `TreeNode::eval` default / gate decision (D1) didn't regress the unanalysed case.
4. **`ranls-gui-ui-tests`** case: `MainWindow` — set evals on a couple of tree nodes directly,
   save `.rdb`, `newGame`, open, assert `gameState_.evalHistory()` restored.
5. Re-run `test_ui01_winrate_attribution.cpp` + `test_ui13_wingraph_eval_coverage.cpp` — must stay
   green (they pin the gate's other consumers).
6. `docs/fix-log.md` row + `docs/fix-log/<date>-rdb-persist-node-analysis.md` (record D1 + D2
   choices, and the manual-smoke-outstanding note). `CHANGELOG.md` `[Unreleased]` line.
7. **Manual (needs a human):** analyse a real game, save `.rdb`, reopen, confirm the WinGraph
   visually — note as outstanding in the fix-log (no display/engine on the build host).

Tiers 3–7 required.

## Boundaries — do not touch

- eval→win% maths, UI-01 attribution, ANLZ-04 bridge rendering, `buildWinGraphSeries`, the
  WinGraph axes/labels/50%-line/hover/drawing.
- The RT-01 throttle cadence, `flush()` ordering (UI-13).
- Analyze Mode (ANLZ-01), Engine-plays / ENG-02.
- No Settings entry, no `ViewConfig` flag — persistence is unconditional.
- CMake `project(VERSION)`, `src/version.h.in`. Only the `.rdb` `schema` constant moves if the DTO
  shape changes (bump it if you add/rename a persisted field).
- No analysis-on-load, no unanalysed-node backfill, no multi-game, no SGF/RenLib.
