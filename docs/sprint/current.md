# Current sprint

## Sprint 17

**Goal:** Enable the engine incremental analysis stream — per-depth PV / Multi-PV / value / board-PV updates reach the GUI during a search, not just once at the end

**Dates:** 2026-09-07 to — (open — no fixed end date set yet)

**Dependency graph:**

- **PROTO-05** (enable incremental analysis stream) — engine-output-configuration commands only.
  Touches `src/engine/gomocup_protocol.cpp` (`generateConfig` / `generateStart` /
  `generateAnalyzeRequest`, the hardcoded `INFO SHOW_DETAIL 0` at `:260`, the `parseMessage`
  `"Speed "` NPS drop), likely `src/model/engine_config.h` + `src/ui/settings_dialog.*` if
  `SHOW_DETAIL` becomes configurable; regression test under `tests/` fed a captured NORMAL +
  `SHOW_DETAIL 3` transcript. **`/systematic-debugging` Phase 1–2 first** — reproduce against a
  real engine and capture the "before" transcript as the fixture (diagnosis already written in
  `docs/notes/2026-09-07-pv-multipv-display-root-cause.md`). **Design call with the user** before
  implementing — pick the enable mechanism (A `YXSHOWINFO`, B `INFO SHOW_DETAIL 3`, C both;
  instruction file recommends C with `SHOW_DETAIL` as an `EngineConfig` field defaulting to 3).
  Must **not** touch: the RT-01 throttle / coalescing tick, `PVView` / `BoardRenderer` drawing,
  the `YXNBEST` / `YXBOARD` request shape, PROTO-03's extension DSL, or `analysisConverged()` /
  ANLZ-07 convergence semantics (verify its behaviour, don't alter it). The "stage-and-swap" round
  buffering is in scope only if the per-depth feed exposes PVView flicker.

Scoped directly from the diagnosis note — no `features/<slug>/` folder. Detail:
`docs/todo/PROTO-05-enable-incremental-analysis-stream.md`; execution guidance:
`docs/instruction/PROTO-05-enable-incremental-analysis-stream.md`.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| PROTO-05 | enable engine incremental analysis stream (per-depth PV / Multi-PV / value / board-PV) | — | — | 🔲 Not started |

Points not yet estimated (consistent with Sprints 3–16).

**Lessons carried in from Sprint 16:**

- The build host has no engine binary and no display for the interactive path. PROTO-05's
  acceptance explicitly needs a live-engine manual check (analyse ~5 s, watch the PV list / value
  readout update progressively, confirm Multi-PV rows 2..N appear) — that stays an outstanding
  **human** step; the automated regression must run off a captured transcript.
- A green guard can still be a false positive. The new PROTO-05 regression test must assert real
  behaviour — `engineStatus().depth` actually increasing across the transcript, `pvLines()`
  actually reaching `multiPV` non-empty entries each carrying their own depth — not "the parse
  path exists".
- `sigc::mem_fun` on a recurring `Glib::signal_timeout` / `signal_idle` needs `sigc::track_obj`
  (or an explicit `disconnect()` in the destructor). PROTO-05 drives a heavier engine-log append
  stream (`SHOW_DETAIL 3` adds `REALTIME` lines) through `bottom_panel` — confirm RT-02 bounding
  and UI-10/UI-15 sticky-scroll still hold, and don't add an untracked timer/idle connection.

See `docs/sprint/burndown.md` for the daily remaining-items table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `/CLAUDE.md` ("Sprint cadence").
