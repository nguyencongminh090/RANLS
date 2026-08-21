# 2026-08-21 — UI-01: win-rate graph attributes evals to the wrong side, and evals can go unrecorded

## Summary

Three distinct correctness bugs in the win-rate graph / eval bookkeeping path, plus one
confirmation-only acceptance criterion, tracked together as `docs/todo/UI-01-winrate-attribution-errors.md`.

## Fix

1. **Side-to-move attribution off by one ply** — `toDisplayWinrate` (`src/ui/analysis_panel.cpp`)
   computed `bool blackToMove = (i % 2 == 0)`. `raw[i]` (from `GameState::evalHistory`,
   `src/model/game_state.cpp`) is the eval of the position **after** move `i`; after move 0
   (Black's move) it is White to move, so side-to-move is Black only on **odd** `i`. Changed to
   `bool blackToMove = (i % 2 == 1)`. This bug flipped the black/white series and mirrored the
   graph around the 50% line for every other ply.
2. **Eval writeback gated on the wrong condition** — `GameState::setAnalysisData`
   (`src/model/game_state.cpp`) only wrote a new eval when `depth` or `nodes` changed. An engine
   reporting a revised score at the same depth/nodes (aspiration-window re-search, final
   confirmation line) had that score silently discarded. Changed the condition to
   `depth != bestPv.depth || nodes != bestPv.nodes || eval != bestPv.score` — writes whenever any
   of the three actually differ. `signal_tree_updated`'s emission condition changes the same way as
   a side effect of sharing the `if`; its *frequency* tuning is RT-04's concern and was left
   otherwise untouched.
3. **Unevaluated nodes indistinguishable from a true 50%** — `GameState::evalHistory` substituted
   `0.5` both for a missing tree node and for a node with no analysis yet (`depth == 0 && nodes ==
   0`). Changed both cases to return `std::numeric_limits<double>::quiet_NaN()` instead. Consumers
   now propagate/check `std::isnan()`:
   - `toDisplayWinrate` (`src/ui/analysis_panel.cpp`) passes NaN straight through for both series
     rather than clamping it into a false reading.
   - `WinGraphView::onDraw` (`src/ui/win_graph_view.cpp`) breaks the line into disjoint segments
     around NaN runs (pen-up/pen-down) instead of interpolating through them or plotting a flat
     50%; suppresses the current-move dot when the current move is unevaluated; and the hover
     tooltip shows `"(no eval)"` instead of a fabricated percentage.
4. **Mate scores** — confirmed by tracing `parseEvalToken` (`src/engine/gomocup_protocol.cpp:121-146`,
   clamps `+M`/`-M` to winrate `1.0`/`0.0`) through `setAnalysisData` → `evalHistory` →
   `toDisplayWinrate` → `WinGraphView::onDraw`. The clamped 1.0/0.0 is a real, non-NaN value, so it
   renders as a line pinned to the top/bottom axis with a visible dot and a `"100.0%"`/`"0.0%"`
   tooltip — a meaningful, distinguishable rendering of a forced win/loss, not silently dropped.
   **No code change was made for this part**; it was a confirmation-only acceptance criterion and
   the existing behavior already satisfies it.

### Fallout: pre-existing test broken by the NaN sentinel

`tests/test_rt01_throttle.cpp`'s "evalHistory is cached" test asserted `first == second` on two
`vector<double>` reads of an unanalyzed node. With fix (3) both reads are now `[NaN]`, and
`NaN == NaN` is `false` under `vector::operator==`, so the assertion would spuriously fail even
though the cache correctly returned the identical value both times. Updated to an element-wise
comparison that treats `NaN`-in-both-vectors-at-the-same-index as equal, rather than weakening or
dropping the check.

## Verification

- **Build:** ran a genuine clean build — a fresh `cmake -S . -B build_cmd_clean -G Ninja
  -DCMAKE_BUILD_TYPE=Release` (not reusing the pre-existing `build_cmd`) followed by
  `cmake --build build_cmd_clean -j$(nproc)`. All 43 targets compiled with no errors and no
  warnings from any of the changed files (`game_state.cpp`, `analysis_panel.cpp`,
  `win_graph_view.cpp`, `test_ui01_winrate_attribution.cpp`, `test_rt01_throttle.cpp`). Removed
  `build_cmd_clean` afterward; the app's normal `build_cmd` directory (used by `bash build.sh`) was
  already up to date from a prior session (`ninja: no work to do`), which this clean rebuild
  independently corroborates rather than merely trusting.
- **Tests:** `ctest --test-dir build_cmd_clean --output-on-failure` — 1/1 test target passed.
  Direct run of `tests/rapfi-gui-tests`: **66/66 test cases passed, 279/279 assertions passed**,
  including all 6 new cases in `tests/test_ui01_winrate_attribution.cpp`:
  - side-to-move attribution formula pinned per ply (guards the exact original inversion)
  - `evalHistory` per-ply raw values match what was fed via `setAnalysisData`
  - eval overwrite at unchanged depth/nodes (the core of fix 2)
  - eval update still occurs when depth/nodes do change (no-regression)
  - NaN returned for an unanalyzed node, and the NaN→real transition once analyzed
  - a genuinely-analyzed exact-0.5 score still reads back as `0.5`, not NaN (guards against an
    over-eager "treat 0.5 as unevaluated" mistake)
- `test_rt01_throttle.cpp`'s cache-identity test updated as described above and passes with the new
  NaN-producing `evalHistory`.
- `analysis_panel.cpp`'s `toDisplayWinrate` is UI-layer (includes `gtkmm.h`) and by design is not
  linked into the headless `rapfi-gui-tests` target (see `tests/CMakeLists.txt`'s "no gtkmm" guard
  comment). The new test mirrors its one-line attribution formula as a local helper instead, with a
  comment explaining why direct linkage isn't feasible — see the header comment in
  `tests/test_ui01_winrate_attribution.cpp`.

## Scope boundaries respected

- Did not touch update *frequency* of `signal_tree_updated`/`signal_engine_analysis` — that's RT-04.
- Did not touch graph styling/axis design beyond the "unevaluated" visual distinction required by
  the acceptance criteria.
- Did not make any code change for the mate-score criterion — verified by tracing existing code
  only, per the scope note in `docs/todo/UI-01-winrate-attribution-errors.md`.
