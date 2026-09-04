# 2026-09-04 — RDB-02: wire `.rdb` into Save/Open, retire `.yxgame` write

Tracked task, not a bug — logged here per the project convention that every
`TODO.md` code that touches shipping code gets a fix-log row. Second of
RDB-01/02/03 on integration branch `feat/rdb-save-format`; branch
`rdb-02/wire-rdb-into-save-open` off `feat/rdb-save-format` (base contains
`2fb1b66` "RDB-01: .rdb container framing …").

## Summary

RDB-01 gave a working `.rdb` substrate but nothing in the app produced or
consumed one. RDB-02 makes `.rdb` the format Save/Open use, demotes `.yxgame` to
import-only, and puts a clean interface between `main_window.cpp` and both.

### New — `src/model/rdb/game_archive.{h,cpp}`

- `IGameArchiveReader::load(path, err) -> std::optional<GameGraph>` and
  `IGameArchiveWriter::save(path, GameGraph, err) -> bool`. Every failure path
  returns nullopt/false + a non-empty `*error`; nothing throws.
- `RdbArchive` implements both: `save` = `encodeCbor` → `writeContainer` with the
  DEFLATE codec (id 2); `load` = `readContainer` → `decodeCbor`.
- `YxgameReader` implements the reader only — calls the **unchanged**
  `GameIO::loadGame` and maps `LoadedGame` (boardSize / rule / flat moves) into a
  linear-chain `GameGraph`: `nodes[0]` a sentinel, node *i* parent = *i*−1, no
  `analysis` on any node ⇒ every eval NaN, exactly like a fresh game.
- Factory `archiveReaderFor` / `archiveWriterFor` pick by **lowercased**
  extension: `.rdb` ⇒ `RdbArchive`; `.yxgame` ⇒ `YxgameReader` for read and
  **nullptr** for write (saving `.yxgame` is a caller-visible error); any other
  or absent extension ⇒ `RdbArchive` for read (fails cleanly on a bad magic —
  never guesses `.yxgame`).
- `applyGameGraphToState(GameState&, const GameGraph&, string*)` — the "apply a
  GameGraph on load" helper, kept out of `MainWindow` so it is unit-testable.
  Validates `schema` / board size / rule / every parent back-reference **and
  every move coord against `graph.board`** before mutating anything (RDB-01's
  `applyGameGraph` does not range-check coords — a hand-edited/corrupt `.rdb`
  could carry an off-board coord; carried RDB-01-review note). On success:
  `newGame(board)` → `setRule(rule)` → rebuild the whole variation tree from
  `graph.nodes` (DFS pre-order + parent indices) via `tree().addMove` from the
  correct parent `TreeNode*` — branches on the tree directly, never
  `makeMove`-down-a-branch-and-undo — then replay the mainline first-child chain
  through `makeMove` so `history_`/`board_` advance to the tip, then emit
  `signal_tree_updated` + `signal_board_changed`. `sendConfig()` stays in
  `onLoadGame`.

### Changed — `src/main_window.cpp`

- `onSaveGame`: default name `game.rdb`, single "RANLS game (*.rdb)" filter,
  appends `.rdb` when the chosen path has no extension.
  `toGameGraph(gameState_.tree(), boardSize, rule, {generator="RANLS"})` →
  `archiveWriterFor(path)->save(...)`. A non-`.rdb` target (nullptr writer) → an
  error dialog. The `GameIO::saveGame` call is gone.
- `onLoadGame`: filters `.rdb` (default) + `.yxgame` + all files.
  `archiveReaderFor(path)->load(...)` → `applyGameGraphToState(gameState_, …)` →
  `controller_.sendConfig()`. Parse or apply failure → `showErrorDialog`, current
  game untouched. Stops Analyze Mode first (an in-flight search would make
  `newGame`/`makeMove` early-return).

### Changed — `src/model/game_io.{h,cpp}`

- `GameIO::saveGame` deleted — declaration, definition, and the header comment
  rewritten to describe an import-only legacy parser. `grep -rn saveGame` now
  matches only a changelog-style comment in the test file. `GameIO::loadGame`
  and `kFormatVersion` are untouched.
- `tests/test_io01_game_io.cpp`: the two save/round-trip cases became
  hand-written-file load cases; every load / corrupt-input / missing-file case
  kept verbatim.

## Regression tests

- `tests/test_rdb02_archive.cpp` (`ranls-gui-tests`, gtkmm-free — 5 cases / 62
  assertions):
  - factory picks `RdbArchive` for `.rdb` / `.RDB` / unknown, `YxgameReader` for
    `.yxgame` / `.YxGame`; `archiveWriterFor` is non-null only for `.rdb`.
  - `RdbArchive` save → load round-trips a tree with a branch, a branch comment,
    and an in-range eval — decoded `GameGraph` matches, and after
    `applyGameGraphToState` onto a fresh `GameState` the board size / rule / total
    node count / mainline move count and the branch's comment + eval all survive.
  - `YxgameReader` on a hand-written legacy string → a linear chain, `analysis`
    absent on every node; after apply, `evalHistory()` is all-NaN.
  - a bad legacy line and an off-board `.rdb` coord each fail cleanly with the
    current game left intact.
- `tests/test_rdb02_save_open.cpp` (`ranls-gui-ui-tests`, links gtkmm,
  display-skip guarded — 2 cases / 14 assertions): a real `MainWindow` links the
  new wiring and still exposes `save-game` / `load-game`; a branched tree saved
  to a temp `.rdb`, then `newGame`, then opened, restores board size / rule /
  total node count / mainline move count.

## Verification

- `./build.sh build_cmd` — clean, only the 3 known pre-existing
  `-Wunused-function` warnings in `src/engine/gomocup_protocol.cpp`.
- `ctest --test-dir build_cmd` — 3/3: `ranls-gui-tests`, `ranls-gui-ui-tests`,
  `rel02-version-single-source`.
- `grep -rn "saveGame"` — no writer; the `GameIO::saveGame` symbol is gone.
- `git diff --stat` — limited to `CMakeLists.txt`, `src/main_window.cpp`,
  `src/model/game_io.{h,cpp}`, `src/model/rdb/game_archive.{h,cpp}`, `tests/`,
  `docs/`, `CHANGELOG.md`.

## Deliberately out of scope (per the instruction boundaries)

- No `TreeNode` field additions, no `evalHistory()` gate change, no PV /
  annotation glyph / engine metadata / timestamp persistence — **RDB-03**.
- No recent-files, auto-save, bulk `.yxgame`→`.rdb` migration, SGF, RenLib.
- `GameIO::loadGame` behaviour unchanged — `YxgameReader` wraps it as-is.
