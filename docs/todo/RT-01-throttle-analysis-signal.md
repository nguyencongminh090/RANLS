# RT-01 — Throttle/coalesce the realtime analysis signal path

**Status:** open
**Area:** realtime display / engine→UI pipeline
**Priority:** P0
**Source:** UI/UX + codebase review, 2026-08-21

## Problem

There is no throttle, coalescing, or diffing anywhere between the engine's stdout and the UI
redraw. Every parsed engine line runs the full update chain synchronously on the main loop.

`GomocupProtocol::signal_analysis.emit()` is called from **six** sites:

| Site | Context |
|---|---|
| `src/engine/gomocup_protocol.cpp:328` | `REALTIME BEST` |
| `src/engine/gomocup_protocol.cpp:364` | `commitPV` — fires **once per PV** |
| `src/engine/gomocup_protocol.cpp:472` | `Speed ...` line |
| `src/engine/gomocup_protocol.cpp:583` | generic token line |
| `src/engine/gomocup_protocol.cpp:612` | `parseRealtimePV` |
| `src/engine/gomocup_protocol.cpp:706` | `onPVDone` |

Each emit reaches `GameState::setAnalysisData` (`src/model/game_state.cpp:198`) →
`signal_engine_analysis`, whose handlers do four expensive things every time:

1. `BoardViewModel::update()` — scans the whole board `O(n²)` and rebuilds every marker vector
   (`src/model/board_view_model.cpp:18-25`), then a full `queue_draw`
   (`src/main_window.cpp:326-329`).
2. `PVView::update()` — destroys and recreates every row widget and its `EventControllerMotion`
   (`src/ui/pv_view.cpp:44-95`). See RT-03.
3. `winGraph_.setData(gameState_.evalHistory())` — `evalHistory()` walks the whole variation tree
   on every call (`src/model/game_state.cpp:226-241`).
4. `EngineStatusView::update()`.

With `multiPV = 8`, `commitPV` alone triggers 8 full-UI rebuilds per depth iteration. At a few
hundred engine lines per second this is the primary cause of the observed realtime lag and flicker.

## Why it matters

This is the app's core job — surfacing engine thinking legibly. Right now the surface degrades
exactly when the engine is working hardest, which is when the user is watching.

## Acceptance criteria

- Engine→UI analysis updates are coalesced to a bounded refresh rate (target: ~10–15 Hz / 60–100ms)
  rather than one-per-parsed-line. A `Glib::signal_timeout` tick or an idle-source-with-dirty-flag
  is the natural GTK mechanism.
- The most recent state always wins — coalescing must never drop the final update of a search
  (`onPVDone` / search completion must flush immediately, not wait for the next tick).
- `evalHistory()` is not recomputed per emit — either cached and invalidated on tree/board change,
  or moved off the per-update path.
- Measured before/after: engine line rate vs. redraw count, and main-loop responsiveness during a
  deep `multiPV=8` search.

## Scope boundary

- Do **not** fold the PVView rebuild fix in here — that is RT-03.
- Do **not** change protocol parsing semantics; this item is purely about update rate.
- Per `perf-optimization` skill rules: **profile/measure first**, then change.

## Related

- RT-02 (engine log flooding), RT-03 (PVView rebuild), RT-04 (tree view rebuild)
- STATE-01 shares the `signal_engine_analysis` path but is a correctness bug, not a rate bug.
