# Sprint 11 (closed 2026-09-04)

**Goal:** New `.rdb` (Ranls Database) binary save format — persist the whole variation tree with
per-node analysis so a reloaded game keeps its WinGraph without re-analysis. Replaces the flat text
`.yxgame` format for saving (`.yxgame` stays import-only).

**Dates:** 2026-09-04 to 2026-09-04.

## Final state — all committed items shipped

| CODE | Summary | Status |
|---|---|---|
| ANLZ-03 | ~~Persist per-node win% into the `.yxgame` save file (additive text token)~~ | ⛔ SUPERSEDED — approach rejected by the user during `/implement-task`; replaced by RDB-01/02/03. Goal + regression-test intent delivered by RDB-03. Not a rollover. |
| RDB-01 | `.rdb` container framing + `ICompressor` + `GameGraph` DTO + hand-rolled CBOR + `VariationTree`↔`GameGraph` convert | ✅ DONE (PR #11 squash `ad69e55` → feat) |
| RDB-02 | Wire `.rdb` into Save/Open; `IGameArchive*` + `RdbArchive` + `YxgameReader` (import-only) + extension factory; retire `GameIO::saveGame` | ✅ DONE (PR #12 squash `d1182d2` → feat) |
| RDB-03 | Persist + restore per-node analysis end-to-end — closes the original ANLZ-03 goal | ✅ DONE (PR #13 squash `51ff892` → feat) |

Sprint 11 opened around **ANLZ-03**; during `/implement-task ANLZ-03` discussion the user rejected
extending the `.yxgame` text schema and chose a new binary format. The feature was designed through
`features/rdb-save-format/` (`user_story.md`, `planning.md` — Q1–Q8 resolved, `diagram/container.md`)
and re-split into RDB-01/02/03. Decisions of record: **tree, not DAG** (transposition/symmetry merge
weighed and rejected — comment belongs to a path, navigation/delete ambiguity); **single game per
file** (container wraps it forward-compatibly for a future multi-game library); **CBOR payload +
DEFLATE container** over the already-present `zlib` (`zstd` reserved as codec id 1); RenLib `.lib`
studied as prior art → import filed as a follow-up. Points not estimated (consistent with Sprints
3–10). Development used the `feat/rdb-save-format` integration branch: three sub-PRs merged into it
(#11/#12/#13), it rebased on `main` between each, one merge PR (#14, merge commit `f44478b`) back to
`main` preserving the per-`CODE` commits.

## What shipped

- **RDB-01** (PR #11 squash `ad69e55`, on `main` as `1c6b53c`): the model-layer storage substrate.
  New `src/model/rdb/` — `compressor.{h,cpp}` (`ICompressor` + `RawCodec` id 0 + `DeflateCodec`
  id 2 over `zlib` level 7; `zstd` id 1 reserved, unimplemented); `rdb_container.{h,cpp}` (`"RDB1"`
  magic + explicit little-endian header + always-on `zlib` crc32; atomic write
  `.tmp`→fflush→fsync→`rename`; `readContainer` validates magic/version/codec/packed-size/crc
  before allocating — every failure ⇒ empty `std::optional` + error, never throws); `game_graph.h`
  (stdlib-only serialisation DTO); `game_graph_cbor.{h,cpp}` (hand-rolled total RFC 8949 subset —
  bounds-checked before every read, unknown map keys recursively skipped, recursion depth-capped,
  `winrate` outside `[0,1]` ⇒ absent); `game_graph_convert.{h,cpp}` (`toGameGraph` flat DFS
  pre-order + parent indices + root sentinel, no analysis block for NaN eval; `applyGameGraph`
  rejects forward/self/out-of-range parent refs + bad schema/board/rule before mutating). +15 test
  cases / 984 assertions incl. 400+500 random-truncation fuzz that must never crash.
- **RDB-02** (PR #12 squash `d1182d2`, on `main` as `df9b4ef`): wired `.rdb` into the app and
  retired `.yxgame` write. `src/model/rdb/game_archive.{h,cpp}` — `IGameArchiveReader`/`Writer`;
  `RdbArchive` (both directions); `YxgameReader` (read-only, wraps the unchanged `GameIO::loadGame`
  → linear-chain all-NaN `GameGraph`); `archiveReaderFor`/`archiveWriterFor` factory by lowercased
  extension (no `.yxgame` writer). `applyGameGraphToState` — validates board/rule/every parent ref
  **and every move coord vs board size** (carried RDB-01-review note) before mutating, then rebuilds
  the whole tree (branches on the tree directly, mainline replayed via `makeMove`). `onSaveGame`
  writes `game.rdb`; `onLoadGame` reads `.rdb`/`.yxgame`/all, stops Analyze Mode first.
  `GameIO::saveGame` deleted. `docs/audit/2026-09-04-rdb-save-format.md` records the format change.
  +7 test cases across a model + a display-guarded UI file.
- **RDB-03** (PR #13 squash `51ff892`, on `main` as `03a6718`): per-node analysis persistence —
  **closes the original ANLZ-03 goal**. **D1:** `evalHistory()` gate `(depth>0 || nodes>0)` →
  `!std::isnan(node->eval)`, and `TreeNode::eval` re-defaulted `0.0` → `quiet_NaN()` so a fresh
  unanalysed node stays a NaN gap, never a false `0.0`. Only consequential out-of-scope fallout:
  `src/ui/tree_explorer.cpp` eval column NaN-guarded. UI-01 / UI-13 tests stay green, unchanged —
  no ripple. **D2:** `std::optional<NodeAnalysisExtras>` member on `TreeNode` (evalText / pv / glyph
  / engineRef / analyzedUtc); `eval`/`nodes`/`depth` unchanged. `toGameGraph` now writes the full
  analysis for every non-NaN node; shared `applyNodeAnalysis()` helper validates `winrate ∈ [0,1]`
  (else drop block, eval NaN, never abort) and each pv coord vs board (else drop the pv). One
  display-only engine entry persisted. **`kSchemaVersion` not bumped** (stays 1 — the pv/g/e/ts
  keys were already in schema 1's codec + DTO). +9 test cases incl. the carried ANLZ-03 regression
  set + a fresh-game-all-NaN guard for the D1 default change.

## Lessons

- **A design decision the user owns can arrive mid-`/implement-task`.** ANLZ-03 was a scoped,
  sprint-ready task; the "just extend `.yxgame`" assumption in its detail file didn't survive first
  contact with the actual `game_io.cpp`. The recovery — stop, run it through `features/rdb-save-format/`,
  re-split into `RDB-01..03`, re-plan the sprint — is the process working, not failing. Detail files
  scaffolded ahead of a sprint (ANLZ-03 was filed 2026-09-04 with `_detail TBD_` then fleshed out)
  are worth a sanity pass against the code they name before the sprint commits to them.
- **Integration-branch model paid off for a 3-task feature.** Each sub-PR was independently
  build+ctest-verified by the orchestrator before merge; the branch rebased on `main` between each
  so conflicts (none arose) would surface early; the final merge PR kept `1c6b53c`/`df9b4ef`/`03a6718`
  as distinct commits on `main`. The friction cost was a few forced-with-lease pushes on the shared
  feature branch — acceptable solo.
- **Re-defaulting a widely-read struct field (`TreeNode::eval` `0.0`→NaN) needs a grep of every
  reader.** RDB-03 did this and found exactly one consequential site (`tree_explorer.cpp`); the
  `setAnalysisData` `!=` write-guards were provably unaffected (NaN compares unequal). Carried
  lesson from Sprint 8: don't ship a struct-default change on unit tests alone — the fresh-game
  all-NaN regression case is the guard.
- Carried from Sprint 10 and honoured: the whole storage layer is gtkmm-free (`src/model/rdb/`,
  exercised by `ranls-gui-tests`); only two `MainWindow` slots + one `tree_explorer.cpp` guard
  touch UI. `/sprint close` run the same day the last item merged.

## Rolled over to Backlog

Nothing rolled over — all committed items finished. ANLZ-03 was **superseded**, not rolled over
(its `docs/todo/ANLZ-03-*.md` stays `⛔ SUPERSEDED` with a pointer to RDB-01/02/03; it is not a
Backlog item).

**Outstanding (not a rollover — a verification task):** manual live/visual WinGraph smoke of a real
save→reopen `.rdb` round-trip. No display/engine on the build host; carried in
`docs/fix-log/2026-09-04-rdb-03-persist-node-analysis.md`. Do this on a machine with a display
before relying on the feature in anger.

## Next sprint

Sprint 12 — not yet opened. `TODO.md` Backlog holds **TOOL-02** (`check-task-structure.js` doesn't
recognise the `🔲`/`⛔` open-markers → false orphan reports; `[Model: Haiku 4.5]`). Run
`/sprint open 12 "<goal>" <CODE...>` to commit Backlog items and start it.

Release `v0.3.0` cut at this close (MINOR — `.rdb` is a new user-visible save format).
