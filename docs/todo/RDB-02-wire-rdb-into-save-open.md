# RDB-02 — Wire `.rdb` into Save/Open; retire `.yxgame` write; keep `.yxgame` import

**Status:** ✅ DONE (Active — Sprint 11) [Model: Sonnet 5]

Implemented on branch `rdb-02/wire-rdb-into-save-open` (off `feat/rdb-save-format`).

- **New `src/model/rdb/game_archive.{h,cpp}`** — `IGameArchiveReader` / `IGameArchiveWriter`
  interfaces; `RdbArchive` (reader+writer: RDB-01 `writeContainer`/`readContainer` with the
  DEFLATE codec + `encodeCbor`/`decodeCbor`); `YxgameReader` (reader only — calls the unchanged
  `GameIO::loadGame` and maps `LoadedGame` → a linear-chain `GameGraph`: `nodes[0]` sentinel,
  node *i* parent = *i*−1, no `analysis` on any node ⇒ every eval NaN). Factory
  `archiveReaderFor` / `archiveWriterFor` pick by lowercased extension — `.rdb` ⇒ `RdbArchive`,
  `.yxgame` ⇒ `YxgameReader` for read / **nullptr** for write (saving `.yxgame` is a
  caller-visible error), any other/absent extension ⇒ `RdbArchive` for read (fails cleanly on a
  bad magic, never guesses `.yxgame`).
- **`applyGameGraphToState(GameState&, const GameGraph&, string*)`** in `game_archive.cpp` — the
  testable "apply on load" helper (NOT inline in `MainWindow`). Validates board size, rule, every
  parent back-reference **and every move coord against `graph.board`** (carried RDB-01-review
  note — RDB-01's `applyGameGraph` does not range-check coords) *before* mutating anything; on
  any problem returns false + sets `*error`, current game untouched. On success: `newGame(board)`
  → `setRule(rule)` → rebuild the whole variation tree from `graph.nodes` via `tree().addMove`
  from the correct parent `TreeNode*` (branches on the tree directly, never `makeMove`/undo) →
  replay the mainline first-child chain through `makeMove` so `history_`/`board_` advance →
  emit `signal_tree_updated` + `signal_board_changed`. `sendConfig()` stays in `onLoadGame`.
- **`src/main_window.cpp`** — `onSaveGame`: default name `game.rdb`, single "RANLS game (*.rdb)"
  filter, appends `.rdb` when the chosen path has no extension, `toGameGraph(tree, boardSize,
  rule, {generator="RANLS"})` → `archiveWriterFor(path)->save(...)`; a non-`.rdb` target (no
  writer) → error dialog. `GameIO::saveGame` call removed. `onLoadGame`: `.rdb` (default) +
  `.yxgame` + all-files filters, `archiveReaderFor(path)->load(...)` → `applyGameGraphToState`
  → `sendConfig()`; parse/apply failure → `showErrorDialog`, current game untouched; stops
  Analyze Mode first if active.
- **Retired `.yxgame` write** — `GameIO::saveGame` declaration + definition deleted, `game_io.h`
  header comment rewritten (import-only). `grep -rn saveGame` → only the changelog-style comment
  in `test_io01_game_io.cpp` remains, no writer. `test_io01` save/round-trip cases rewritten as
  hand-written-file load cases (all load / corrupt-input cases kept).
- **`docs/audit/2026-09-04-rdb-save-format.md`** + `docs/audit.md` row (format-change decision);
  **`CHANGELOG.md`** `[Unreleased]` line; **`docs/fix-log`** entry.

### Verification

- `./build.sh build_cmd` clean — only the 3 known pre-existing `-Wunused-function` warnings in
  `gomocup_protocol.cpp`.
- `ctest --test-dir build_cmd` — 3/3 green: `ranls-gui-tests`, `ranls-gui-ui-tests`,
  `rel02-version-single-source`.
- New `tests/test_rdb02_archive.cpp` (`ranls-gui-tests`, 5 cases / 62 assertions): factory picks
  `RdbArchive` for `.rdb`/`.RDB`/unknown and `YxgameReader` for `.yxgame`/`.YxGame`, no
  `.yxgame` writer; `RdbArchive` save→load round-trips a branched tree with a comment + in-range
  eval (node count, mainline move count, branch comment/eval survive); `YxgameReader` on a
  hand-written legacy string → linear chain, `analysis` absent on every node, `evalHistory()`
  all-NaN after apply; a bad legacy line and an off-board `.rdb` coord both fail cleanly with the
  current game intact.
- New `tests/test_rdb02_save_open.cpp` (`ranls-gui-ui-tests`, 2 cases / 14 assertions,
  display-skip guarded): a real `MainWindow` links the new wiring and still exposes the
  `save-game` / `load-game` actions; a branched tree saved to a temp `.rdb`, then `newGame`,
  then opened, restores board size / rule / total node count / mainline move count.
**Area:** new `src/model/rdb/game_archive.{h,cpp}` (`IGameArchiveReader`/`IGameArchiveWriter`,
`RdbArchive`, `YxgameReader`, extension factory). `src/main_window.cpp` (`onSaveGame` ~L754,
`onLoadGame` ~L721 — swap to the archive interface; file-dialog filters + default name/extension).
`src/model/game_io.{h,cpp}` (`saveGame` removed or left dead; `loadGame` stays, wrapped by
`YxgameReader`). `tests/`.
**Priority:** P2
**Source:** `features/rdb-save-format/` — user-approved 2026-09-04. Second of three tasks. Builds
directly on RDB-01. Integration branch `feat/rdb-save-format`.
**Design:** `features/rdb-save-format/planning.md` ("Abstraction / low coupling" block) +
`diagram/container.md` (save/load sequence diagrams).
**Depends on / relates to:** **RDB-01** (container + codec + `GameGraph` + convert — hard
dependency, do RDB-01 first). Supersedes ANLZ-03. `RDB-03` layers per-node analysis persistence on
top of what this task wires.

## Problem

RDB-01 gives a working `.rdb` storage substrate but nothing in the running app can produce or
consume one — Save still writes flat `.yxgame` (`GameIO::saveGame`, mainline only, no evals) and
Open still parses only `.yxgame`. This task makes `.rdb` the format the Save / Open menu items use,
demotes `.yxgame` to import-only, and puts a clean interface between `main_window.cpp` and both.

## Scope (in order)

1. **`IGameArchiveReader` / `IGameArchiveWriter`** (`game_archive.h`):
   `Reader::load(path, string* err) -> optional<GameGraph>`,
   `Writer::save(path, const GameGraph&, string* err) -> bool`.
2. **`RdbArchive`** implements both (RDB-01's container + CBOR + convert).
   **`YxgameReader`** implements Reader only — internally calls the existing `GameIO::loadGame`
   and maps `LoadedGame` (boardSize, rule, moves) into a `GameGraph` with a linear node chain,
   every node's `analysis` absent (⇒ NaN).
3. **Factory**: `archiveReaderFor(path)` / `archiveWriterFor(path)` pick by lowercased extension
   (`.rdb` ⇒ `RdbArchive`; `.yxgame` ⇒ `YxgameReader` for read, **no writer** — attempting to save
   `.yxgame` is a caller-visible error / not offered in the dialog).
4. **`onSaveGame`**: default name `game.rdb`, dialog filter "RANLS game (*.rdb)". Build a
   `GameGraph` via `toGameGraph(gameState_.tree(), boardSize, rule, meta)` and
   `archiveWriterFor(path)->save(...)`. Remove the `GameIO::saveGame` call.
5. **`onLoadGame`**: dialog filters "RANLS game (*.rdb)" + "Legacy game (*.yxgame)" + "All".
   `archiveReaderFor(path)->load(...)` → on success, `newGame(graph.board)`, `setRule(graph.rule)`,
   replay `graph.nodes` into the tree (mainline first-child walk → `makeMove` for the mainline;
   branches rebuilt on the tree directly), `invalidateEvalHistoryCache()`, `controller_.sendConfig()`.
   Parse failure → `showErrorDialog`, current game untouched (unchanged contract).
6. **Retire `.yxgame` write**: delete `GameIO::saveGame` + its declaration + the now-dead
   `test_io01_game_io.cpp` save cases (keep the load/round-trip-via-load cases; they now guard
   `YxgameReader`'s dependency). Update `game_io.h` header comment.
7. **`docs/audit/2026-<mm-dd>-rdb-save-format.md`** + `docs/audit.md` row: record that the on-disk
   save format changed from text `.yxgame` to binary `.rdb`, `.yxgame` is import-only, no
   migration tool, rationale (per-node analysis persistence + open/versioned structure).
8. **`CHANGELOG.md`** `[Unreleased]`: user-facing line — "Games now save as `.rdb` (preserves the
   full variation tree; older `.yxgame` files still open)".
9. **Tests** (`tests/test_rdb02_archive.cpp`, `ranls-gui-tests`): factory picks the right impl by
   extension; `RdbArchive` save→load round-trip of a `GameGraph`; `YxgameReader` on a hand-written
   legacy `.yxgame` string yields the right moves + all-NaN nodes; `.yxgame` write is not offered
   / errors. A `ranls-gui-ui-tests` case (`test_rdb02_save_open.cpp`) drives `MainWindow`
   save-then-open on a temp path and asserts the tree + move count survive.

## Acceptance criteria

- Save writes a `.rdb`; re-opening it restores board size, rule, and the full variation tree
  (all branches, comments) — not just the mainline.
- An existing `.yxgame` file still opens (moves + rule + board size; nodes all NaN, exactly as a
  fresh game).
- Saving is only offered as `.rdb`. No code path writes `.yxgame`.
- Corrupt / truncated `.rdb`, or a `.yxgame` with a bad line → visible error, current game intact.
- `./build.sh` clean; `ctest` 3/3 (new `ranls-gui-tests` + `ranls-gui-ui-tests` cases).
- `docs/audit.md` + detail entry; `CHANGELOG.md` line.

## Scope boundary

- **No per-node engine-analysis persistence yet** beyond what `TreeNode` already stores
  (`eval`/`nodes`/`depth`/`comment`). PV, annotation glyphs, engine metadata, timestamps: the DTO
  carries them, this task neither populates nor restores them — **RDB-03**.
- Do not extend `TreeNode` — RDB-03.
- Do not change the `evalHistory()` `depth>0 || nodes>0` gate — RDB-03 decides that.
- No recent-files list, no auto-save, no `.yxgame`→`.rdb` bulk migration (all out of scope per
  IO-01 and `features/rdb-save-format/user_story.md`).
- No SGF, no RenLib import (follow-ups in planning.md).
- No `APP_VERSION` / CMake `project(VERSION)` change.
