# Instruction — UI-13: WinGraph records win% regardless of side

## Approach

Run the `systematic-debugging` pipeline first — the todo file already carries a trace (2026-09-03);
start at its "Investigation still to do" list, do not re-derive the data path. Reproduce the NaN
gaps for both the manual-analyze and "Engine plays" auto-play cases before touching code, and
confirm with a logged run of `setAnalysisData` args which node each write lands on.

This is a design decision as much as a bug: bring the reproduction data + candidate fixes A/B/C
from the todo file to the user and let them pick before implementing. Default recommendation is
**A (write the child node from the best PV)** plus fixing the `flush()`-before-move ordering — both
are local to the model/controller, need no extra engine search, and are unit-testable gtkmm-free.

## Pitfalls

- `setAnalysisData`'s write is gated on `!pvLines_.empty()` AND on depth/nodes/eval actually
  changing — a naive child-node write must apply the same "only if changed" guard and must call
  `invalidateEvalHistoryCache()` / set `treeDirty_`, or the graph won't refresh.
- Don't write a child node that doesn't exist yet in the variation tree (engine PV moves are not
  played moves) — either only fill children already on the played line, or accept that off-line PV
  evals aren't plotted (the graph only walks the played line anyway).
- Keep the NaN sentinel semantics: a derived child eval must set `depth > 0` (or `nodes > 0`) or
  `evalHistory()` will still treat it as unevaluated.
- `EngineController::signal_move` calls `flush()` before `signal_engine_move`; reordering affects
  RT-01 guarantees — verify the final PV/status still isn't dropped for the searched position.

## Boundaries — do not touch

- `buildWinGraphSeries` perspective/attribution logic (UI-01, UI-09).
- The eval→win% conversion maths.
- RT-01 throttle cadence, graph axes/layout, `WinGraphView` drawing.
