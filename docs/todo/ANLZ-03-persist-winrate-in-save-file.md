# ANLZ-03 — Persist per-node win% into the save-game file so a reloaded game keeps its WinGraph

**Status:** ⛔ SUPERSEDED 2026-09-04 (Active — Sprint 11) — this task's approach ("additive
per-move win% token on the `.yxgame` plain-text schema, bump `kFormatVersion`") was rejected by the
user during implementation discussion. Replacement: a new **binary `.rdb` (Ranls Database)** save
format — CBOR payload + DEFLATE container, whole variation tree + per-node analysis, open/versioned
structure (a **tree**, not a DAG — transposition/symmetry merge weighed and rejected; **single
game** per file). Design: `features/rdb-save-format/` (`user_story.md`, `planning.md` Q1–Q8 all
resolved, `diagram/container.md`). Re-split into **RDB-01** (`docs/todo/RDB-01-rdb-container-and-codec.md`
— container + `ICompressor` + `GameGraph` DTO + CBOR), **RDB-02**
(`docs/todo/RDB-02-wire-rdb-into-save-open.md` — Save/Open wiring, `GameIO::saveGame` retired,
`.yxgame` import-only via `YxgameReader`), **RDB-03**
(`docs/todo/RDB-03-persist-restore-node-analysis.md` — per-node analysis persistence end-to-end,
**closes this task's original goal**, carries its NaN-round-trip / legacy-import / out-of-range
regression tests). All three on integration branch `feat/rdb-save-format`. Everything below is the
original (pre-supersession) scope, kept for context.

---

**Original status:** 🔲 OPEN (Active — Sprint 11)
**Area:** `src/model/game_io.cpp` (save/load — the plain-text `.yxgame` schema, `kFormatVersion` /
`yxgame_version`), `src/model/game_state.{h,cpp}` (per-node eval already stored on the variation
tree by UI-13 / ANLZ-01 — expose it for serialisation + accept it on load), `src/model/variation_tree.*`
(node eval field). Read-only reference: `src/ui/win_graph_view.cpp`, `evalHistory()`.
**Priority:** P2
**Source:** WinGraph-coverage discussion 2026-09-04
(`docs/notes/2026-09-04-wingraph-analyze-mode-and-backfill.md`, "Rút ra" point 2: *persist win% into
node + save/load so re-analysis isn't needed — Sabaki/SGF `SBKV`*). Filed to Backlog 2026-09-04,
follows ANLZ-01 (which shipped the per-position measured eval that this makes durable).
**Design:** none — additive field on an existing plain-text schema, no user story needed. Scoped
directly from the note.
**Depends on / relates to:** ANLZ-01 (produces the per-node measured evals this persists), UI-13
(candidate A's derived reply-ply eval is also on the tree — persisted the same way), UI-01 (NaN =
"not evaluated" must round-trip as *absent*, never as a written `0.5`), REL-02 (do **not** touch the
app version — `kFormatVersion` / `yxgame_version` is the save-file schema version and is the only
version string in play here).

## Problem

Analyze Mode (ANLZ-01) fills a real WinGraph point for every position the engine settles on, but
those evals live only in memory. Saving a game and re-opening it drops every eval — the WinGraph
comes back empty and the user must re-run analysis on the whole line. Mature analysis GUIs persist
the per-move win% into the game file (Sabaki writes `SBKV` into the SGF) precisely so a reloaded
game keeps its graph without re-analysis.

## Scope (in order)

1. **Schema.** Add an optional per-move win% to the `.yxgame` move record in `game_io.cpp`. Bump
   `kFormatVersion` and keep the loader backward-compatible: a file written by an older version (no
   win% field) loads exactly as today, with every node's eval left at the NaN "not evaluated"
   sentinel.
2. **Save.** For each move node that carries a real (non-NaN) eval, write it. A node with the NaN
   sentinel writes **no** win% token (absence, not `0.5`).
3. **Load.** Parse the win% token when present; set the node's eval. Missing/blank token → leave the
   NaN sentinel. Validate the parsed value is in `[0, 1]` (or the project's chosen eval range) —
   out-of-range → treat as absent, don't abort the load.
4. **Invalidate** the `evalHistory()` cache after a load so the WinGraph repaints from the restored
   evals (same `invalidateEvalHistoryCache()` / `treeDirty_` path UI-13 uses).
5. Regression tests (`ranls-gui-tests`, model-layer): round-trip a game with a mix of evaluated and
   NaN nodes → reload → `evalHistory()` matches; load an old-format file with no win% fields → no
   crash, all-NaN; out-of-range token → treated as absent.

## Acceptance criteria

- Save a game after Analyze Mode has filled the WinGraph, re-open it → the WinGraph is identical
  (every persisted point restored, NaN gaps still gaps).
- A game file written before this change still loads correctly (no win% ⇒ all nodes NaN ⇒ WinGraph
  behaves exactly as a freshly loaded pre-ANLZ-03 game did).
- A NaN node never serialises a win% token; a restored NaN node is never rendered as 50%.
- No change to the app version string (REL-02) — only `kFormatVersion` moves.
- `./build.sh` clean; `ctest` both suites green.

## Scope boundary

- Do **not** change the eval→win% maths, UI-01 attribution, ANLZ-04 bridge rendering, or
  `buildWinGraphSeries`.
- Do **not** re-run or trigger analysis on load — this task only persists and restores what was
  already measured. Backfilling unanalysed nodes on load is explicitly out of scope.
- Do **not** touch `APP_VERSION` / CMake `project(VERSION)` — the only version bump here is the
  save-file `kFormatVersion`.
- Do **not** add a new file format or switch to SGF — extend the existing `.yxgame` plain-text
  schema in place.
