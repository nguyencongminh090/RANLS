# 2026-09-04 — ANLZ-05: Analyze Mode never auto-moves, and a click mid-search plays

**Task:** `docs/todo/ANLZ-05-analyze-mode-no-automove-allow-mid-search-moves.md`
**Instruction:** `docs/instruction/ANLZ-05-analyze-mode-no-automove-allow-mid-search-moves.md`
**Branch:** `anlz-05/analyze-mode-no-automove-allow-mid-search-moves`

## Prompt

Refinement of shipped ANLZ-01. User report 2026-09-04: with Analyze Mode **and**
"Engine plays &lt;side&gt;" both on, the engine kept auto-playing while the user was
trying to study the position; and a board click during the background search was
silently swallowed. Two linked behaviour changes, both scoped to Analyze Mode ON:

1. Analyze Mode blocks auto-move entirely — the engine only ever analyses,
   including its own assigned turn. Reverses `features/analyze-mode/planning.md`
   Q6 for Analyze Mode (with Analyze Mode off, Q6 / auto-move / ENG-02 unchanged).
2. A board click during an in-flight Analyze-Mode search stops the search, applies
   the move, and lets `scheduleAnalyzeModeRestart()` re-analyse the new position.

## Action

`MainWindow`-orchestration only — no model API, no `ViewConfig` field, no Settings
row, no protocol change. `GameState::makeMove()`'s `if (analyzing_) return false;`
guard left intact (the model invariant stays honest); the caller now stops the
search first.

- **`src/main_window.cpp` `maybeStartAutoMove()`** — the `Glib::signal_idle().connect_once`
  body gained `if (gameState_.viewConfig().analyzeMode) return;` alongside the
  existing `enginePlays==Off` / `!isRunning()` / `!=Idle` / `!isEnginesTurn` guards.
  The outer `autoMoveScheduled_` coalescing is unchanged. Net effect:
  `requestEngineMove()` is never reached while Analyze Mode is on.
- **`src/main_window.cpp` `scheduleAnalyzeModeRestart()`** — deleted the
  `if (isEnginesTurn(matchConfig().enginePlays, board().sideToMove())) return;`
  block and its comment. Remaining idle body: `analyzeMode` on → `isRunning()` →
  `engineState()==Idle` → `stopAnalysis(); analyze()`. The engine's-turn position
  is now analysed like any other.
- **`src/main_window.cpp` board-click handler** (`boardView_.signal_move_clicked`
  lambda in `connectSignals()`) — `if (controller_.isAnalyzing()) controller_.stopAnalysis();`
  before `gameState_.makeMove(pos)`. `stopAnalysis()` clears `analyzing_` and
  returns the controller to Idle synchronously, so `makeMove()`'s guard passes;
  the `signal_board_changed` it emits re-enters `scheduleAnalyzeModeRestart()`.
  Gated on `isAnalyzing()` so a click with no engine / no search stays a plain
  `makeMove()`. Occupied / invalid points are still rejected by `makeMove()`.
- **`src/main_window.h`** — added `friend struct RanlsAnlz05Probe;` (test-only seam
  for the widget-level test to reach `gameState_`/`engine_`/`controller_` and the
  scheduler methods). No production API.
- **`features/analyze-mode/planning.md`** — dated implementation note under the
  existing "Q6 reversed by ANLZ-05" revision section.

Deliberately **not** done (scope boundary): one-shot Analyze/Stop path untouched;
`revertEnginePlaysToOff()` / `MatchConfig` never touched (ENG-02 orthogonality);
undo/redo/jump during an Analyze-Mode search still hit the `analyzing_` guard —
called out in the todo as a separate follow-up, not addressed here.

## Tests

- **`tests/test_anlz05_stop_then_move.cpp`** (`ranls-gui-tests`, model/engine, no
  gtkmm) — 2 cases: (a) with `analyzing_` set, `makeMove()` still returns false and
  clears once the flag is down (guard unchanged); (b) real `EngineController` over
  `/bin/cat`, empty board — `analyze()` → `makeMove()` refused → `stopAnalysis()`
  (synchronous Idle + `analyzing_` clear) → `makeMove()` succeeds. Proves the
  stop-then-move sequence the click handler relies on.
- **`tests/test_anlz05_no_automove_action.cpp`** (`ranls-gui-ui-tests`, real
  `MainWindow`, links gtkmm, display-skip guarded) — 1 case, empty board + engine
  assigned Black (its turn) + fake engine Idle, inspecting the actual wire lines
  via `EngineProcess::signal_line_sent`:
  - Analyze Mode ON: driving both idle callbacks sends the analyze request
    (`YXBOARD` … `YXNBEST N`) and **not** the move request (`BEGIN`/`BOARD`);
    `isAnalyzing()` true — the Q6 reversal, engine's-turn position analysed.
  - Analyze Mode OFF, same setup: the auto-move `BEGIN` still goes out, no
    `YXNBEST` — the "Engine plays &lt;side&gt;" path is not regressed.
- No existing ANLZ-01 test asserted the old Q6 "engine's turn skipped by
  analyze-mode restart" behaviour (they cover the toggle action and the
  evalHistory outcome only), so none needed updating.

## Verification

- `./build.sh` — clean (only the 3 pre-existing `-Wunused-function` warnings in
  `gomocup_protocol.cpp`).
- `ctest --test-dir build_cmd` — 3/3 pass.
  - `ranls-gui-tests`: 183 cases / 2317 assertions, all pass (was 181/…; +2 ANLZ-05).
  - `rel02-version-single-source`: pass.
  - `ranls-gui-ui-tests`: 21 cases / 140 assertions, all pass (was 20/…; +1 ANLZ-05).
- Acceptance criteria in the todo file: met by the two new tests (auto-move
  suppressed under Analyze Mode incl. engine's turn; engine's-turn position
  analysed; Analyze Mode off → auto-move unchanged; stop-then-move sequence;
  invalid-point click still a safe no-op via the unchanged `makeMove()` guard).
- **Manual live-engine smoke NOT run** — no engine binary / no display on the
  build host. Checklist stands in the instruction file (§ "Manual smoke"): needs a
  human to confirm against a real engine that no stone is auto-placed under
  Analyze Mode, Stop/toggle-off place no move, and a mid-search click lands the
  stone and restarts analysis.
