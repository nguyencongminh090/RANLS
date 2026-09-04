# ANLZ-06 — Analyze Mode: an analysis search's best move is auto-played on the board

**Status:** ✅ DONE — fixed 2026-09-04

Fixed in `src/engine/engine_controller.{h,cpp}` by adding a `SearchIntent {None, Analysis, Move}`
discriminator: `analyze()`/`requestEngineMove()` set it, the `protocol_->signal_move` handler
captures+resets it before deciding and only emits `signal_engine_move` when the intent is `Move`
(UI-13 flush/state bookkeeping still runs unconditionally), and every stop path
(`stopAnalysis()`/`stopEngine()`/`signal_process_died`) resets it to `None` so a trailing
coordinate line for an aborted search is inert. `YXNBEST` request shape, the ANLZ-05 `MainWindow`
guards, `GameState::makeMove()`'s `analyzing_` guard, and ENG-02/UI-06/one-shot-Analyze all
untouched, per scope. New regression suite `tests/test_anlz06_search_intent_gate.cpp` (4 cases,
`ranls-gui-tests`) feeds inbound coordinate lines via `EngineProcess::signal_line_received` and
pins all 4 acceptance-criteria scenarios, including the exact double-move repro (`a1 a2 a3` + user
`a4` + late engine `b1` → move count stays 4, no `b1`). `RUN_TESTS=1 ./build.sh` clean (only the 3
pre-existing `-Wunused-function` warnings, none new); `ctest` 3/3 green, including all existing
`test_anlz05_*` cases re-checked in isolation. Manual live-engine smoke NOT run (no engine/display
on the build host) — checklist in the instruction file, still needed from a human. Full detail:
`docs/fix-log/2026-09-04-analyze-mode-search-plays-stray-move.md`.

Regression surfaced against the just-shipped **ANLZ-05**. ANLZ-05's todo/instruction both promise
"pressing Stop just stops the search — it never triggers a move" and "a board click mid-search
places the stone" — but neither is actually enforced, because ANLZ-05 only added guards in
`MainWindow` and never touched the path that plays an engine move. Two user-reported symptoms,
**one root cause**:

1. **Stop still makes a move.** With Analyze Mode on and a search running, pressing Stop (toolbar,
   hotkey, panel button, or toggling Analyze Mode off) can drop a stone on the board.
2. **A board click mid-search produces a double move.** The user clicks an empty point; the click
   handler stops the search and places the user's stone, then the interrupted engine search's best
   move arrives and is *also* played. Reported example: position `a1 a2 a3`, White to move, engine
   suggests `b1`; user plays `a4` → board records `a1 a2 a3 a4 b1` (user's move **and** the
   engine's, for the same turn).

## Root cause

`EngineController::analyze()` sends `YXBOARD … DONE` + **`YXNBEST n`**
(`GomocupProtocol::generateAnalyzeRequest`, `src/engine/gomocup_protocol.cpp:283`). Per the Rapfi
protocol reference (`.../Rapfi/docs/protocol.md` line 383):

> `YXNBEST <n>` — sets `multiPV` and **searches — the engine still plays only its single best
> move**; the top `n` candidates go to the MESSAGE/detail streams.

`YXNBEST` is **not** a non-committal "analyse this position" command. It runs a search that
culminates in the engine emitting its best move as a bare coordinate line, exactly like `TURN`.
That coordinate is then routed to the board with **no filter for analysis vs. move intent**:

| Step | Location | Behaviour |
|---|---|---|
| engine emits `x,y` | `src/engine/gomocup_protocol.cpp:360-366` | any Coord-typed line → `signal_move.emit(move)` — no notion of why the search ran |
| controller relays | `src/engine/engine_controller.cpp:48-71` | **`signal_engine_move.emit(move)` on line 68 is outside every guard** — the `wasSearching` check only gates the `analyzing_` / `flush` / `setState(Idle)` bookkeeping |
| window plays it | `src/main_window.cpp:627-629` | `signal_engine_move` → `gameState_.makeMove(pos)`, unconditional |

`analyze()` and `requestEngineMove()` **both** do `setState(EngineState::Analyzing)`
(`engine_controller.cpp:238` and `:256`). There is **no discriminator** for "this search was
analysis — discard the move" vs. "this was `requestEngineMove()` — play it."

- **Symptom 2:** the click handler calls `stopAnalysis()` (→ Idle) then `makeMove(a4)`. The
  interrupted `YXNBEST` search's best move `b1` arrives async afterwards → `signal_engine_move`
  fires anyway (line 68 doesn't check state) → `makeMove(b1)`.
- **Symptom 1:** whenever the `YXNBEST` search terminates by completion / time-limit (rather than a
  clean `STOP` before any bestmove output — Rapfi docs say `STOP` yields no move, but Yixin-family
  engines may differ), the terminal coordinate is emitted and auto-played.

Why ANLZ-01 didn't surface it: pre-ANLZ-05, Analyze Mode never ran on the engine's-turn position,
restarts always terminated the previous search with `STOP` before completion, and a board click was
swallowed by `makeMove()`'s `analyzing_` guard. ANLZ-05 opened all three paths.

**Working reference:** the sibling `signal_analysis` handler (`engine_controller.cpp:73-81`)
already guards exactly this class of unwanted engine output with `if (!gameState_.isAnalyzing())
return;`. The `signal_move` path needs the analogous discrimination — but keyed on *search
intent*, because an analysis best move must be dropped even while `isAnalyzing()` is still true.

## Area

- `src/engine/engine_controller.{h,cpp}` — the fix. `analyze()` / `requestEngineMove()` /
  `stopAnalysis()` / `stopEngine()` and the `protocol_->signal_move` handler in
  `connectProtocolSignals()`.
- `src/main_window.cpp` — `connectSignals()` `signal_engine_move` handler (`~L627`) and the
  `signal_move_clicked` lambda (`~L446`): read-only reference; the fix belongs in the controller.
- Read-only reference: `src/engine/gomocup_protocol.cpp` `generateAnalyzeRequest` / `parseLine`
  Coord branch; `.../Rapfi/docs/protocol.md` §1.2 (`STOP`/`YXSTOP`), line 383 (`YXNBEST`).

## Priority

P1 — corrupts the game record (plays moves the user didn't make). Blocks trusting Analyze Mode.

## Depends on / relates to

- **ANLZ-05** — this closes the gap ANLZ-05's own acceptance criteria assumed but didn't enforce
  ("Stop … still places no move"; the mid-search click placing exactly one stone).
- **ANLZ-01** — `analyze()`'s `YXNBEST` request is unchanged in shape; only the handling of the
  best-move line it produces changes.
- **ENG-02 / UI-06** — the genuine "Engine plays &lt;side&gt;" auto-move path
  (`requestEngineMove()`) must still play its move. Keep it orthogonal; do not touch
  `MatchConfig` / `revertEnginePlaysToOff()`.
- **UI-13** — the `signal_move` handler's UI-13 ordering comment (flush the searched position's
  analysis before the board advances) still applies for the `requestEngineMove()` case; preserve it.

## Problem

Analyze Mode is meant to be a pure study mode (ANLZ-05). Instead, an Analyze-Mode search that ends
any way other than a pre-output `STOP` drops the engine's best move onto the board — silently
corrupting the game, and (when combined with a user click) producing two moves for one turn.

## Scope

1. **Give the controller a search-intent discriminator.** Add a private member to
   `EngineController` — e.g. `enum class SearchIntent { None, Analysis, Move }` (or a plain
   `bool moveRequested_`). `analyze()` sets it to `Analysis`; `requestEngineMove()` sets it to
   `Move`.
2. **Gate the move emission on intent.** In the `protocol_->signal_move` handler
   (`connectProtocolSignals`, `engine_controller.cpp:48-71`):
   - Intent `Move`: current behaviour — clear `analyzing_`, flush, `signal_engine_move.emit(move)`,
     `setState(Idle)`. Keep the UI-13 ordering.
   - Intent `Analysis` (or `None`): the coordinate is a *search-completion* signal only. Do the
     state bookkeeping (clear `analyzing_`, flush the final analysis so the searched position keeps
     its eval/PV, `setState(Idle)` if `wasSearching`) but **do not** `signal_engine_move.emit`. The
     `signal_state_changed(Idle)` handler already re-enters `scheduleAnalyzeModeRestart()`, so
     Analyze Mode simply re-ponders.
3. **Reset the intent so a late line after a stop is inert.** Clear the intent to `None` in
   `stopAnalysis()` and `stopEngine()` (and on the `signal_process_died` path). After a `STOP`, any
   trailing coordinate line the engine still emits for the aborted search must not be played.
4. **Do not rely on `state_` alone** — `stopAnalysis()` sets `Idle` synchronously, so by the time a
   trailing coordinate arrives `wasSearching` is already false; the intent flag is what
   distinguishes "ignore this" from "this is a real engine move I asked for".

## Acceptance criteria

- Analyze Mode ON, a search running, user presses Stop (each surface: toolbar, hotkey, panel Stop,
  Analyze-Mode toggle off) → search stops, **no stone is placed**, now or when a trailing engine
  line arrives.
- Analyze Mode ON, a search running, user clicks an empty point → **exactly one** stone lands (the
  user's); the engine's in-flight best move for the previous position is discarded; analysis
  restarts on the new position.
- Analyze Mode ON, a search completes naturally (time/depth limit, no user action) → the engine's
  best move is **not** played; the eval/PV for that position is still recorded; Analyze Mode
  re-ponders the same position.
- Analyze Mode OFF, "Engine plays &lt;side&gt;", engine's turn → `requestEngineMove()` still plays
  the engine's move exactly as today (ENG-02 / UI-06 unchanged). One-shot Analyze/Stop unchanged.
- `./build.sh` clean; `ctest` both suites green; a regression test **feeds an inbound coordinate
  line** (the gap in `test_anlz05_no_automove_action.cpp`, which only asserts on outbound lines)
  and pins: analysis-intent coord → no `signal_engine_move`; move-intent coord → `signal_engine_move`
  fires; post-`stopAnalysis()` coord → inert.

## Scope boundary

- Only the analysis-vs-move routing of the engine's coordinate output. Not the protocol request
  shape (`YXNBEST` stays), not the `MainWindow` guards ANLZ-05 added, not ENG-02 / one-shot
  Analyze / auto-move-with-Analyze-Mode-off.
- Whether `analyze()` should use a different protocol command that never plays a move (e.g. a
  Rapfi-specific analysis mode) is a **separate** question — call it out if it comes up; this task
  fixes the routing with the request unchanged.
- Undo/redo/jump during an Analyze-Mode search (the ANLZ-05 out-of-scope note) is still separate.
