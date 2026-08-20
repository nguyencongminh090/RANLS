# RT-01 — execution guidance

## Order of work

Do **STATE-01 first**. RT-01 changes *when* the UI updates; STATE-01 changes *what* it shows.
Throttling on top of stale data makes the stale data harder to reproduce, not easier.

## Approach

Measure before changing anything (`perf-optimization` skill rule). Cheapest useful measurement:
count `signal_analysis` emissions and `BoardView` draw calls over a fixed-depth search at
`multiPV=1` and `multiPV=8`. Keep those numbers — they are the acceptance evidence.

Preferred mechanism: a **dirty-flag + single timeout source** on the consumer side, not a rate limit
inside the protocol parser.

- Let `GomocupProtocol` keep emitting per line — it is the honest representation of what the engine
  said, and `EngineController`/`BottomPanel` may legitimately want every line.
- Have `GameState::setAnalysisData` store the new data and mark dirty, then coalesce
  `signal_engine_analysis` onto a timer tick (~60–100ms).
- Keep a `flush()` path so search completion (`onPVDone`) and analysis-stopped can emit immediately
  rather than waiting up to a tick.

Putting the throttle in `GameState` rather than in each of the four consumers means it is one
mechanism, not four, and the consumers stay dumb.

## Pitfalls

- **Do not drop the last update.** The classic throttle bug: data arrives, timer isn't scheduled
  because one just fired, engine goes quiet, final result never renders. Always leave the dirty flag
  armed and let the tick pick it up.
- `evalHistory()` (`src/model/game_state.cpp:226`) walks the tree on every call and is invoked from
  three separate handlers in `AnalysisPanel` (`:74`, `:89`, `:98`). Cache it or the throttle only
  moves the cost around.
- `sigc::signal` emission is synchronous — a handler that itself mutates `GameState` will re-enter.
  Check that nothing in the analysis handlers writes back into the model before adding a timer.
- Timer sources must be disconnected on teardown, or they fire into a destroyed `GameState`.

## Do not touch

- Protocol parsing semantics (`src/engine/gomocup_protocol.cpp` parse functions) — RT-01 is purely
  about update rate. Parser changes belong to PROTO-01/STATE-03.
- `PVView`'s rebuild strategy — RT-03 owns that. Throttling will mask how bad it is; fix it properly
  there.
- Layering: the throttle belongs in `src/model/` or `src/ui/`, not in `src/engine/`. Do not let UI
  pacing concerns leak into the protocol layer (`software-architecture` skill).
