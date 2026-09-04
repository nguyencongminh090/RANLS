# `.rdb` (Ranls Database) — binary, compressed, graph-structured save format

## Background

`.yxgame` (IO-01) is a flat plain-text file: header + one `move=x,y` line per
**mainline** ply. It stores no variation tree, no per-node analysis, and its
loader hard-rejects any file whose `yxgame_version` differs (no migration path —
see `src/model/game_io.h`).

Sprint 11 (ANLZ-03) originally set out to bolt an optional per-move win% token
onto `.yxgame`. During implementation discussion (2026-09-04) the user decided
against extending `.yxgame` at all and instead to:

1. **Reject `.yxgame` as the save format** — keep it read-only for *import* of
   existing files; stop writing it.
2. Introduce **`.rdb` (Ranls Database)** as the new canonical save format:
   binary, compressed, storing the whole variation graph with per-node analysis
   (move, win%, depth, nodes, …), with an **open, low-coupling, versioned
   structure** that can evolve without breaking old files.

This supersedes ANLZ-03's "additive `.yxgame` field" approach. ANLZ-03's *goal*
(a reloaded game keeps its WinGraph without re-analysis) is a subset of this
feature.

## Actors

- **Reviewer** — saves a game after analysis and re-opens it later expecting the
  full variation tree, comments, and the win-rate graph to come back intact.
- **RANLS model layer** (`VariationTree` / `GameState`) — owns the in-memory
  graph that gets serialised.
- **Future maintainer** — needs to add fields (new engine metadata, score-lead,
  timestamps, multi-game DB) to the format without a flag day.

## User stories

1. As a reviewer, I **Save** a game I have analysed; the `.rdb` file contains
   every node of the variation tree with its measured win% / depth / node count,
   my move comments, the board size and rule.
2. As a reviewer, I **Open** an `.rdb` file weeks later; the variation tree,
   branch structure, comments and the WinGraph are exactly as I left them — no
   re-analysis.
3. As a reviewer with an old `.yxgame` file, I can still **Import** it (moves +
   rule + board size only; every node's eval loads as the NaN "not evaluated"
   sentinel, exactly as a fresh game does today).
4. As a future maintainer, I add a new per-node field; an `.rdb` written by the
   new build still opens in an older build (unknown field skipped), and an old
   `.rdb` opens in the new build (missing field ⇒ default).
5. As a reviewer with a deep tree full of PV lines, the `.rdb` file is a fraction
   of the uncompressed size (structure + repeated PV strings compress heavily).

## Rules

- **NaN round-trips as absence.** A node with no measured eval is written with
  *no* win% value (or an explicit null), never `0.0` / `0.5`. Restoring it yields
  the NaN sentinel. (This is the exact UI-01 "false 50%" bug and its regression
  test must cover the round-trip.)
- **Whole graph, not just mainline.** Unlike `.yxgame`, `.rdb` persists the full
  `VariationTree` (all branches), each node's `move`, `eval`, `nodes`, `depth`,
  `comment`.
- **Win% range validation on load.** A value outside the project's eval range
  (`[0,1]` — confirm against `evalHistory()` semantics) is treated as absent, not
  clamped; a bad value never aborts the load.
- **No analysis on load.** Loading restores measured data only; the engine is not
  poked. Backfilling unanalysed nodes is out of scope (as in ANLZ-03).
- **`kFormatVersion` / the `.rdb` schema version is NOT `APP_VERSION`.** REL-02
  single-sourced the *app* version; the file-schema version is independent. Do
  not touch CMake `project(VERSION)` / `src/version.h.in`.
- **Atomic writes.** Write to `<path>.tmp`, `fsync`, `rename` over the target —
  never leave a truncated `.rdb`.
- **Loader contract unchanged:** any problem ⇒ `std::nullopt` + `*error`, never a
  partial success, never throws (matches current `GameIO::loadGame`).

## Hard constraints (do not touch)

- eval→win% conversion maths; UI-01 attribution; ANLZ-04 bridge rendering;
  `buildWinGraphSeries`; WinGraph axes/layout/drawing.
- The in-memory `VariationTree` / `TreeNode` API stays a **tree** (one parent per
  node). Any DAG/transposition capability is a *reader-side* index built from the
  serialised `zobrist` hints, not a change to the core model — see planning.md Q1.
- Analyze Mode (ANLZ-01), Engine-plays (UI-06/ENG-02) behaviour.

## Out of scope (separate follow-ups)

- Multi-game database in one `.rdb` (opening explorer, "search all games for this
  position"). The container is designed to *allow* it; this feature ships
  single-game.
- SGF import/export.
- Recent-files list, auto-save (already out of scope per IO-01).
- A settings migration to convert existing `.yxgame` files in bulk.

## Cross-links

- [planning.md](planning.md) — research findings + open questions + sequencing
- [diagram/container.md](diagram/container.md) — container framing + schema +
  save/load sequence
- `docs/todo/ANLZ-03-persist-winrate-in-save-file.md` — the superseded approach
- `docs/notes/2026-09-04-wingraph-analyze-mode-and-backfill.md` — "Rút ra" point 2
- `src/model/game_io.{h,cpp}` — the `.yxgame` reader kept for import
- `src/model/variation_tree.h` — the graph being serialised
