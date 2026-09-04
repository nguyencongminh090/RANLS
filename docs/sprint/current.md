# Current sprint

## Sprint 11

**Goal:** New `.rdb` (Ranls Database) binary save format — persist the **whole variation tree** with
**per-node analysis** so a reloaded game keeps its WinGraph without re-analysis. Replaces the flat
text `.yxgame` format for saving (`.yxgame` stays import-only).

**Dates:** 2026-09-04 to — (open — no fixed end date set yet)

**Re-planned 2026-09-04:** Sprint 11 opened around **ANLZ-03** ("additive win% token on the
`.yxgame` text schema"). During implementation discussion the user **rejected** extending
`.yxgame` and chose a new binary format. Design worked through `features/rdb-save-format/`
(`user_story.md`, `planning.md` — Q1–Q8 all resolved, `diagram/container.md`). Decisions of record:
**tree not DAG** (transposition/symmetry merge weighed, rejected — comment/navigation ambiguity);
**single game per file** (container wraps it forward-compatibly); **CBOR payload + DEFLATE
container** over the already-present `zlib` (`zstd` = reserved codec id 1); RenLib `.lib` studied
(confirms flat DFS-preorder node stream) → import listed as a follow-up. ANLZ-03 → `⛔ SUPERSEDED`;
work re-split into `RDB-01..03`.

**Integration branch:** `feat/rdb-save-format`. Sub-PRs `RDB-01`, `RDB-02`, `RDB-03` merge into it;
it rebases on `main`; one final PR `feat/rdb-save-format → main`. (github skill "Branch model".)

**Dependency graph:**
- **RDB-01** — no upstream dep. `src/model/rdb/`: container framing + `ICompressor`
  (Raw / DEFLATE-zlib) + `GameGraph` DTO + hand-rolled CBOR subset + `VariationTree`↔`GameGraph`
  convert. Pure model layer, unit-tested, no UI, no `game_io.cpp` change. NaN eval ⇒ absent
  `winrate`, never `0.5` (UI-01). `schema` / `container_version` are NOT `APP_VERSION` (REL-02).
- **RDB-02** — needs RDB-01. `IGameArchiveReader/Writer` + `RdbArchive` + `YxgameReader`
  (import-only) + extension factory; rewire `onSaveGame` / `onLoadGame`; delete `GameIO::saveGame`;
  dialog filters; `docs/audit/` entry for the format change.
- **RDB-03** — needs RDB-01 + RDB-02. Extend `TreeNode` (`std::optional<NodeAnalysis>`), resolve
  the `evalHistory()` gate (`TreeNode::eval` defaults to `0.0` not NaN — the task's riskiest
  point), full save→reopen→WinGraph-identical path. **Closes the original ANLZ-03 goal** + carries
  its NaN-round-trip / legacy-import / out-of-range regression tests. `docs/fix-log/` entry.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| ~~ANLZ-03~~ | ~~Persist per-node win% into the `.yxgame` save file~~ | — | — | ⛔ Superseded by RDB-01/02/03 |
| RDB-01 | `.rdb` container + `ICompressor` + `GameGraph` DTO + CBOR payload + convert | — | — | 🔲 Not started |
| RDB-02 | Wire `.rdb` into Save/Open; `IGameArchive*` + `RdbArchive` + `YxgameReader`; retire `GameIO::saveGame` | RDB-01 | — | 🔲 Not started |
| RDB-03 | Persist + restore per-node analysis end-to-end (closes ANLZ-03 goal) | RDB-01, RDB-02 | — | 🔲 Not started |

Points not yet estimated (consistent with Sprints 3–10).

**Lesson carried in from Sprint 10:** split a pure helper out of any Cairo/GTK path for
testability. For the RDB tasks the whole storage layer is already gtkmm-free by design (new
`src/model/rdb/`, exercised by `ranls-gui-tests`) — keep it that way; the only UI touch is the two
`MainWindow` slots in RDB-02. Also carried: run `/sprint close` the same day the last item merges.

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `/CLAUDE.md` ("Sprint cadence").
