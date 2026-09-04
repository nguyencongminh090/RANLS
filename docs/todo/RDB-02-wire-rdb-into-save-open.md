# RDB-02 — Wire `.rdb` into Save/Open; retire `.yxgame` write; keep `.yxgame` import

**Status:** 🔲 OPEN (Active — Sprint 11) [Model: Sonnet 5]
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
