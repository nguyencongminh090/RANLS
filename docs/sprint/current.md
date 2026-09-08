# Current sprint

## Sprint 18

**Goal:** Yixin-Board-style live analysis board overlay — the board's engine overlay becomes a
per-cell, live-only visualization fed directly by the engine's search-progress streams, matching
the reference Yixin-Board.

**Dates:** 2026-09-08 to — (open — no fixed end date set yet)

**Dependency graph:**

- **PROTO-06** (replace the analysis board overlay with the Yixin-Board model) — spans engine →
  model → ui + settings + menu, one cohesive `CODE`. Design **fully resolved** with the user
  2026-09-08 (see the detail file's "Resolved decisions"; decision (a) = implement only what
  Rapfi's output actually supports). No `/systematic-debugging` needed — this is additive/
  replacement work off two spec notes, not a bug.
  - **Engine (`src/engine/gomocup_protocol.cpp`):** `parseMessage` REALTIME branch parses
    `POS`/`DONE`/`LOST`/`REFRESH` (keeps `BEST`/`PV`/`VAL`); `onPVDone` writes per-cell winrate/
    mate tag + `tagDepth` for `pv.moves[0]` and clears stale tags (`tagDepth < depth`) on the last
    PV of a round. Reuse `parseEngineCoord` — no axis special-casing.
  - **Model:** new `AnalysisOverlay` struct owned by `GameState` (per-cell `tag`/`tagDepth`/`pos`/
    `lost` + one `bestMove`); `overlayDirty_` flag emitted from the **existing**
    `GameState::tickAnalysis()` / `flush()` (the RT-01 75 ms coalescing point) via a new
    `signal_analysis_overlay`. **Never** route overlay updates through `setAnalysisData()`.
  - **UI (`src/ui/board_renderer.*`, `src/model/board_view_model.*`):** delete `candidateMoves` +
    `drawCandidateMoves`; add one single-winner per-cell layer (priority tag > lost > best >
    pos==1 > pos==2) in the same pipeline slot. `BoardViewModel::update()` copies overlay state
    into render fields **only while `isAnalyzing()`** (live-only — overlay vanishes on search end).
  - **Config/UI:** two persisted `ViewConfig` flags (`showSearchOverlay` master,
    `showSearchWinrate` tags-only), both default on, + a View-menu item each. Mirrors Yixin-Board
    `showanalysis` / `showanalysiswinrate`.
  - **Must NOT touch:** the RT-01 throttle / coalescing tick itself; `GameState::setAnalysisData`
    / `signal_engine_analysis` tree + eval-history work; the `MESSAGE (n)` / `Bestline` →
    `pvLines_` path or `PVView` / WinGraph; PROTO-05's `SHOW_DETAIL` / `YXSHOWINFO` / `YXNBEST` /
    `YXBOARD`; Rapfi's search config (no `aspiration_window`, no new engine command — decision (a),
    so `POS`/`DONE` simply won't arrive from a stock Rapfi and that is the accepted result).
  - Reference: `RefYXB/Yixin-Board/main.c` — REALTIME parse `:6408–6460`, INFO parse `:6477–6600`,
    render priority `:705–716`, resets `:951–953` / `clear_board_tag :583`. Spec notes:
    `docs/notes/2026-09-08-refyxb-multipv-rendering.md` + `-rapfi-engine-realtime-output.md`.
  - Detail: `docs/todo/PROTO-06-port-realtime-feed-to-board.md`; execution guidance:
    `docs/instruction/PROTO-06-port-realtime-feed-to-board.md`.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| PROTO-06 | replace the analysis board overlay with the Yixin-Board model (per-cell winrate tags + REALTIME feed) | PROTO-05 (shipped) | — | 🔲 Not started |

Points not yet estimated (consistent with Sprints 3–17).

**Lessons carried in from Sprint 17:**

- **The GUI was architected for a streaming feed it never subscribed to** (PROTO-05). PROTO-06 now
  consumes that feed for rendering — verify each stream it reads is actually emitted by a stock
  Rapfi before building UI on it. `docs/notes/2026-09-08-rapfi-engine-realtime-output.md` already
  did this: `POS`/`DONE` need `aspiration_window=false` (out of scope), `LOST`/`BEST`/`REFRESH`
  + `INFO PV DONE` are what's real.
- **The build host has no engine binary and no display.** PROTO-06's "watch the overlay update in
  a running app" acceptance is a **human** step; the automated regression must replay a
  captured/constructed transcript and assert real per-cell state (tags per depth, stale-tag
  cleanup, `REFRESH` clearing only `pos`), not "the parse path exists".
- **`sigc::mem_fun` on a recurring `Glib::signal_timeout` / `signal_idle` needs `sigc::track_obj`
  or an explicit `disconnect()`.** PROTO-06 adds a new model→ui signal but reuses the existing
  RT-01 tick — do not add a second timer/idle. An `src/ui/` audit sweep of every
  `signal_timeout` / `signal_idle` connect is still owed a future `CLEAN` item.

See `docs/sprint/burndown.md` for the daily remaining-items table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `/CLAUDE.md` ("Sprint cadence").
