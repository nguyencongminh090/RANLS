# 2026-09-04 — RDB-03: persist + restore per-node analysis (closes the ANLZ-03 goal)

Tracked task, not a bug — logged here per the project convention that every
`TODO.md` code touching shipping code gets a fix-log row. Last of RDB-01/02/03 on
integration branch `feat/rdb-save-format`; branch
`rdb-03/persist-restore-node-analysis` off `feat/rdb-save-format` (base contains
`d4c52bc` "RDB-02: wire .rdb into Save/Open … (#12)").

## Summary

After RDB-02 a `.rdb` round-tripped the tree shape + `eval`/`nodes`/`depth`/
`comment`, but a saved win-rate graph did **not** come back on reopen: the
`evalHistory()` gate hid restored evals, and PV / glyph / engine-ref /
analyzed-at had nowhere to live on `TreeNode`. RDB-03 delivers the original
ANLZ-03 goal — save a game with a populated WinGraph, reopen it, the graph is
back with **no re-analysis and no engine call**.

## The two decisions

### D1 — the `evalHistory()` gate → **preferred path taken**

`GameState::evalHistory()` gated on `(node->depth > 0 || node->nodes > 0)` before
trusting `node->eval`. Changed to `std::isnan(node->eval) ? NaN : node->eval` —
"eval is not NaN" is now the single source of truth, matching the DTO's
"analysis absent ⇒ NaN" rule, so a restored node needs nothing but its `eval`.

This required re-defaulting `TreeNode::eval` from `0.0` to
`std::numeric_limits<double>::quiet_NaN()` (a fresh unanalysed node with
`eval == 0.0` would otherwise plot as a catastrophic 0.0 for Black). Grepped
every reader/writer of `TreeNode::eval`:

- `GameState::setAnalysisData` — the `!=` change guards (`current_node->eval !=
  bestPv.score`, `child->eval != childEval`) still behave (NaN compares unequal ⇒
  first write happens). No change needed.
- `GameState::evalHistory` — the gate itself (changed).
- `src/model/rdb/game_graph_convert.cpp` / `game_archive.cpp` — already
  `std::isnan`-aware.
- `src/ui/tree_explorer.cpp:118` — printed `node->eval` unconditionally for any
  node on the played line (showed `0.00` for unanalysed rows). Guarded with
  `!std::isnan` so an unanalysed row keeps its `-` instead of printing `nan`.
  **This is the only non-test file changed as a consequence of the eval
  re-default beyond the four in the task's scope list.**
- `src/model/board_view_model.cpp` `m.eval` is the unrelated `MoveView::eval`
  (`-1.0` sentinel), not `TreeNode` — untouched.

`test_ui01_winrate_attribution.cpp` and `test_ui13_wingraph_eval_coverage.cpp`
stay green unchanged — the gate change does **not** ripple into them (every
non-NaN eval in those tests is written via `setAnalysisData` at `depth > 0`, and
the "unevaluated ⇒ NaN" / "never-searched ply ⇒ NaN gap" cases now rest on the
NaN default instead of the depth==0 gate, with the same result).

### D2 — `TreeNode` shape → `std::optional<NodeAnalysisExtras>` member

Added `struct NodeAnalysisExtras { std::string evalText; std::vector<Coord> pv;
std::string glyph; int engineRef = -1; int64_t analyzedUtc = 0; }` and a
`std::optional<NodeAnalysisExtras> analysis` member on `TreeNode`.
`eval`/`nodes`/`depth` stay where they are (many call sites). The optional keeps
the common node small and makes "was this analysed" one check. Named
`NodeAnalysisExtras` (not `NodeAnalysis`) to avoid colliding with the existing
`rdb::NodeAnalysis` wire DTO.

## Changes

- **`src/model/variation_tree.h`** — `NodeAnalysisExtras` struct;
  `TreeNode::eval` default → `quiet_NaN()`; `TreeNode::analysis` optional member.
- **`src/model/game_state.cpp`** — `evalHistory()` gate → `!std::isnan(eval)`;
  `#include <cmath>`. Cache invalidation on load is already covered:
  `applyGameGraphToState` calls `gs.newGame()` (which calls
  `invalidateEvalHistoryCache()`) before rebuilding the tree, and the mainline
  replay through `gs.makeMove()` invalidates again; nothing reads `evalHistory()`
  in between, so the next call recomputes. `signal_tree_updated` /
  `signal_board_changed` are emitted by `applyGameGraphToState` as before.
- **`src/model/rdb/game_graph_convert.{h,cpp}`** — `toGameGraph` now writes the
  full `NodeAnalysis` for every non-NaN node (winrate + depth + nodes + evalText
  + pv + glyph + engineRef + analyzedUtc, as available); a NaN node still emits
  **no** analysis block. New shared helper `applyNodeAnalysis(GraphNode, board,
  TreeNode&)`: winrate absent or outside `[0,1]` ⇒ drop the whole block, leave
  `eval` NaN, never abort; each `pv` coord validated against the board, the whole
  pv dropped (not the load) on any off-board coord. `GraphMeta` gains a
  `std::vector<EngineInfo> engines` copied verbatim into `GameGraph::engines`.
- **`src/model/rdb/game_archive.cpp`** — `applyGameGraphToState` now calls the
  same `applyNodeAnalysis` helper (was inline winrate-only restore).
- **`src/main_window.cpp`** — `onSaveGame` populates one display-only
  `rdb::EngineInfo` (id 0, name = engine binary filename, params =
  `threads=… hash=…MB`) from the current `EngineConfig`. A missing/empty engine
  list never fails a load.
- **`src/ui/tree_explorer.cpp`** — NaN-guard on the eval column (see D1).

## Schema version

`kSchemaVersion` **not bumped** — stays `1`. The `pv`/`g`/`e`/`ts` keys and the
`NodeAnalysis` DTO fields were already modelled and coded by RDB-01's CBOR
codec; RDB-03 only makes `toGameGraph` *populate* more of the schema-1 key set.
No key was added or renamed, and unknown keys are skipped on read, so an old
build reads a new file and a new build reads an old file with no incompatibility.

## Regression tests

- **`tests/test_rdb03_node_analysis.cpp`** (`ranls-gui-tests`, gtkmm-free — 8
  cases): fresh never-analysed game ⇒ all-NaN `evalHistory()` (D1 guard); mixed
  evaluated/NaN mainline ⇒ convert→CBOR→apply round-trip ⇒ `evalHistory()` equals
  the original vector exactly, NaN positions still NaN; a NaN node serialises no
  `analysis` block and never restores as `0.5`; legacy `.yxgame` via
  `YxgameReader` ⇒ all nodes NaN, no crash; a crafted `winrate = 1.7` (direct and
  through CBOR) ⇒ treated as absent, load succeeds; PV + evalText + glyph +
  comment + engineRef + analyzedUtc survive a node round-trip; an off-board pv
  coord ⇒ pv dropped, eval still restored; `engines[]` round-trips and an empty
  list never fails the load.
- **`tests/test_rdb03_ui_node_analysis.cpp`** (`ranls-gui-ui-tests`, links gtkmm,
  display-skip guarded — 1 case): analyse-stub two tree nodes, save a real
  on-disk `.rdb`, `newGame()`, open it, assert `GameState::evalHistory()` comes
  back identical (NaN gaps still gaps) with no engine call.

## Verification

- `./build.sh` (`build_cmd`) — clean, only the 3 known pre-existing
  `-Wunused-function` warnings in `src/engine/gomocup_protocol.cpp`.
- `ctest --test-dir build_cmd` — 3/3: `ranls-gui-tests`, `ranls-gui-ui-tests`,
  `rel02-version-single-source`.
- `test_ui01_winrate_attribution.cpp` + `test_ui13_wingraph_eval_coverage.cpp` —
  green, unchanged.
- `git diff --stat` — limited to `src/main_window.cpp`, `src/model/game_state.cpp`,
  `src/model/variation_tree.h`, `src/model/rdb/game_graph_convert.{h,cpp}`,
  `src/model/rdb/game_archive.cpp`, `src/ui/tree_explorer.cpp` (D1 consequence),
  `tests/`, `docs/`, `CHANGELOG.md`.

## Outstanding

- **Manual smoke (needs a human — no display/engine on the build host):** analyse
  a real game, save `.rdb`, reopen, confirm the WinGraph visually (points back,
  NaN gaps still gaps, ANLZ-04 bridge still bridges). Not run here.

## Deliberately out of scope (per the instruction boundaries)

- No analysis-on-load, no unanalysed-node backfill. No eval→win% maths, UI-01
  attribution, ANLZ-04 bridge, `buildWinGraphSeries`, or WinGraph drawing change.
- No Settings entry / `ViewConfig` flag — persistence is unconditional.
- `APP_VERSION` / CMake `project(VERSION)` / `src/version.h.in` untouched.
- `engines[]` is stored on the `GameGraph` for display only; no `GameState`
  field or behaviour hangs off it yet.
