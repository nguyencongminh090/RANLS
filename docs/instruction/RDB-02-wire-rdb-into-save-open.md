# Instruction — RDB-02: wire `.rdb` into Save/Open, retire `.yxgame` write

## Approach

RDB-01 must be merged (or at least present on `feat/rdb-save-format`) first. This task adds the
archive interface + two implementations + the extension factory, then rewires the two
`MainWindow` slots and demotes `.yxgame` to import-only.

Current call sites (`src/main_window.cpp`):
- `onSaveGame` ~L754 → `GameIO::saveGame(path, boardSize, rule, gameState_.history().moves(), &err)`
- `onLoadGame` ~L721 → `GameIO::loadGame(path, &err)` then `newGame` / `setRule` / `makeMove` loop
  / `controller_.sendConfig()`

After:
- `onSaveGame` → `toGameGraph(gameState_.tree(), gameState_.boardSize(), gameState_.rule(), meta)`
  then `archiveWriterFor(path)->save(path, graph, &err)`.
- `onLoadGame` → `archiveReaderFor(path)->load(path, &err)` → apply the `GameGraph`.

`YxgameReader` reuses `GameIO::loadGame` unchanged and adapts `LoadedGame` → a linear-chain
`GameGraph` (node i's parent = node i−1; no `analysis`).

## Applying a `GameGraph` on load

Factor this into a helper (`GameState::loadFromGraph(const GameGraph&)` or a free function in
`game_graph_convert`) so it is unit-testable without `MainWindow`:
1. `newGame(graph.board)` — clears tree/history/board, resets analysis state.
2. `setRule(graph.rule)`.
3. Rebuild the tree from `graph.nodes`: the DTO is DFS pre-order with parent indices. Walk it,
   `tree.addMove(parentNode, move)` for each; the **first child** of the root chain is the
   mainline — replay those through `makeMove` so `history_`/`board_` advance to the mainline tip;
   branch nodes are added to the tree only.
4. `invalidateEvalHistoryCache()`, emit `signal_tree_updated` + `signal_board_changed`.
5. `controller_.sendConfig()` stays in `onLoadGame` (engine resync), not in the helper.

Analysis fields (`eval`/`nodes`/`depth`/`comment`) that RDB-01's `applyGameGraph` already restores
carry through — RDB-02 does not add new ones.

## Pitfalls

- **`makeMove` refuses while `analyzing_`.** `newGame` also early-returns if `analyzing_`. Guard
  the load the same way `onLoadGame`'s `confirmDiscardGame` already does; if Analyze Mode is on,
  stop analysis first (mirror the existing pattern).
- **Branch replay order.** `makeMove` only walks the mainline. For branches you must operate on
  `gameState_.tree()` directly (`addMove` from the correct parent `TreeNode*`) — do not try to
  `makeMove` down a branch and undo back. Keep a `vector<TreeNode*>` indexed by DTO node id while
  rebuilding.
- **Extension casing / missing extension.** Lowercase the extension before matching. A path with
  no extension on save → append `.rdb`. On load, an unknown extension → try `.rdb` first, fall
  back to an error (do not guess `.yxgame`).
- **Dialog filters (GTK4 `Gtk::FileDialog`).** Save: a single `.rdb` filter, `set_initial_name
  ("game.rdb")`. Open: a `Gio::ListStore<Gtk::FileFilter>` with `.rdb`, `.yxgame`, `*` — `.rdb`
  first/default.
- **Deleting `GameIO::saveGame`.** Remove the declaration in `game_io.h`, the definition, the
  `test_io01_game_io.cpp` cases that call it (keep every load / load-after-hand-written-file
  case). Update the `game_io.h` header comment (it currently documents a save/load pair). Grep for
  other `saveGame` callers first — there should be exactly one (`onSaveGame`).
- **`YxgameReader` node chain.** `LoadedGame.moves` is a flat vector; build `nodes[0]` = root
  sentinel (no move), then one node per move with `parent = previous index`. Do not set any
  `analysis`.
- **Error contract.** Every failure path returns `nullopt`/`false` + a non-empty `*err` and leaves
  the current game untouched. No throw escapes the archive layer.

## Verification before marking this task done

1. `./build.sh` clean (3 known warnings only).
2. `ctest` 3/3.
3. **New `ranls-gui-tests`** (`test_rdb02_archive.cpp`): factory picks `RdbArchive` for `.rdb` /
   `.RDB`, `YxgameReader` for `.yxgame`, error for a `.yxgame` *writer*; `RdbArchive` save→load
   round-trip (tree + branches + comments survive); `YxgameReader` on a hand-written legacy string
   → correct moves, every node NaN.
4. **New `ranls-gui-ui-tests`** (`test_rdb02_save_open.cpp`): construct `MainWindow`, build a
   small tree with a branch, save to a temp `.rdb`, `newGame`, open it, assert board size / rule /
   total node count / mainline move count match. Skips cleanly with exit 0 if no display (existing
   pattern).
5. Grep: no writer produces `.yxgame`; `GameIO::saveGame` symbol is gone.
6. `docs/audit.md` row + `docs/audit/<date>-rdb-save-format.md`; `CHANGELOG.md` `[Unreleased]` line.

Tiers 3–6 required.

## Boundaries — do not touch

- `TreeNode` fields — RDB-03.
- `GameState::evalHistory()` gate — RDB-03.
- eval→win% maths, UI-01 attribution, ANLZ-04 bridge, `buildWinGraphSeries`, WinGraph drawing.
- `GameIO::loadGame` behaviour — `YxgameReader` wraps it as-is; do not "improve" the legacy parser.
- Analyze Mode (ANLZ-01), Engine-plays / ENG-02.
- CMake `project(VERSION)`, `src/version.h.in`.
- No recent-files, auto-save, bulk migration, SGF, RenLib.
