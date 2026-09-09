# 2026-09-09 — PROTO-07: native `!yxAnalz` console command (YXANALZ root-move allow-list)

Feature, not a bug fix — logged here because that is the repo's precedent for a completed feature
task (PROTO-05, PROTO-06). Task: [`../todo/PROTO-07-yxanalz-root-move-allowlist.md`](../todo/PROTO-07-yxanalz-root-move-allowlist.md) ·
Guidance: [`../instruction/PROTO-07-yxanalz-root-move-allowlist.md`](../instruction/PROTO-07-yxanalz-root-move-allowlist.md)

## Prompt

Implement PROTO-07 end-to-end on `proto-07/yxanalz-root-move-allowlist`: drive Rapfi's `YXANALZ`
extension (an explicit client-supplied allow-list of depth-0 root moves — the inverse of `YXBLOCK`)
from a native `!yxAnalz` console command, per the 8 resolved design decisions (Route A, no `.ptc`).

## Action

**Protocol (`src/engine/gomocup_protocol.{h,cpp}`, `src/engine/i_engine_protocol.h`).** One new
send helper — the single interface addition the boundaries allow:

```
generateAnalyzeMovesRequest(path, moves)
  -> YXBOARD / "<coord>,<color>" ... / DONE / YXANALZ / "<coord>" ... / DONE
```

The position block is byte-identical to `generateAnalyzeRequest`'s (same `clearAnalysisState()`
first, same alternating colour, first stone Black), and every coordinate — path *and* move list —
is written by `coordToEngine()`, the single conversion point (resolved design §2). One coordinate
per line: Rapfi reads the list with `std::cin >> token`, so line breaks are legal, and this keeps a
long list off one line. `YXNBEST` / `generateAnalyzeRequest` are untouched.

**Controller (`src/engine/engine_controller.{h,cpp}`).** `analyzeMoves(moves)` performs exactly
`analyze()`'s bookkeeping — `SearchIntent::Analysis`, `setAnalyzing(true)`,
`EngineState::Analyzing`, all inside the PROTO-04 `sendOrDefer()` gate. Two deliberate differences
from `analyze()`: an empty list is never put on the wire (defence in depth behind the console
check), and a search already running is **stopped first** rather than early-returned (this is an
explicit user command naming a new list; `pendingStopFlush_` then keeps the new block behind the
aborted search's trailing coordinate). No mirror of the one-shot engine-side whitelist is kept.

**Completion coordinate — no new code, as predicted.** Under `SearchIntent::Analysis` the existing
ANLZ-06 gate in the `signal_move` handler already discards the trailing best-move line: no
`signal_engine_move`, no stone. That is now pinned by a test feeding an *inbound* coordinate line.

**Console (`src/command/command_dispatcher.cpp`).** `!yxAnalz <moveText...>`, group `analysis` (so
it appears in `!help`), arguments through the existing shared `parseMovesText(tail, boardSize())` —
called, never modified. Refusals, each printing a clear error and sending **nothing**: engine not
running; Analyze Mode ON ("Turn off Analyze Mode first …", resolved design §8 — one `if` before any
send, no suspend/resume path, `scheduleAnalyzeModeRestart`/`maybeStartAutoMove` untouched); empty
argument list; every listed point off-board (dropped by `parseMovesText`) or occupied. Occupied
points inside an otherwise valid list are skipped with a note instead of failing the command — the
engine rejects them per-coord anyway. `revertEnginePlaysIfEnginesTurn()` (ENG-02) runs first, same
as `!analyze`.

**Rendering — verified, nothing changed.** `YXANALZ` emits the same `INFO PV` / `MESSAGE REALTIME`
lines `YXNBEST` does, so the PROTO-05 multi-PV panel and the PROTO-06 per-cell overlay work
unchanged. The gating was audited for the "keyed off `analyze()` specifically" bug the instruction
warns about and is clean: `BoardViewModel` gates on `state_.isAnalyzing()`
(`src/model/board_view_model.cpp:53`) and `MainWindow`'s position-change reset on
`controller_.isAnalyzing()` (`src/main_window.cpp:489`) — both facts `analyzeMoves()` sets
identically to `analyze()`. No fix was needed, so none was made.

## Verification

- **Build:** `cmake -S . -B build_cmd -G Ninja -DCMAKE_BUILD_TYPE=Release` + `cmake --build` —
  clean, 144/144 targets, no new warnings.
- **`ctest` 4/4 green:** `port02-style-css-bundled`, `ranls-gui-tests`, `rel02-version-single-source`,
  `ranls-gui-ui-tests`.
  - `ranls-gui-tests`: **227 cases / 2610 assertions**, all passed (was 218 cases before this task).
  - `ranls-gui-ui-tests`: **52 cases / 391 assertions**, all passed, 0 skipped (was 45 cases).
  - Unregressed by name: `test_proto05_incremental_stream`, `test_proto06_analysis_overlay`,
    `test_anlz05_*`, `test_anlz06_search_intent_gate`, `test_anlz07_analyze_restart_convergence`,
    `test_proto04_stop_flush_race`, `test_gomocup_protocol`, `test_proto03_protocol_extension`.
- **New: `tests/test_proto07_yxanalz.cpp`** (9 cases, `ranls-gui-tests`) — wire-block shape and
  `y,x` conversion, `YXBOARD` block precedes `YXANALZ`, both `DONE`-terminated, alternating colour
  (position block asserted byte-identical to `generateAnalyzeRequest`'s, whose tail is still
  `YXNBEST 5`); size-independence at board size 20; `analyzeMoves()` → `Analyzing` + block on the
  wire; **inbound** completion coordinate discarded (no `signal_engine_move`, no stone) — the
  ANLZ-06 gap; empty list never sent; stop-then-queue under PROTO-04; a following `analyze()`
  emitting a plain `YXNBEST` request (no leaked whitelist state).
- **New: `tests/test_proto07_yxanalz_console.cpp`** (7 cases, `ranls-gui-ui-tests`, needs no
  display) — registration + `!help` listing; `!yxAnalz h3 h2 h9` on a 15×15 board producing
  `12,7` / `13,7` / `6,7` on the real wire; all three refusals asserted against
  `EngineProcess::signal_line_sent` (nothing sent, state stays `Idle`); occupied-point skipping.

**Human-owed:** live smoke against a real Rapfi_V2 — per-move PVs rendering in the panel + tagged
candidate cells, `!stop` cancelling, no stray stone. The build host has neither an engine binary
nor a display, so the automated tier replays constructed engine lines only (Sprint 18 lesson).

## Deviation from `instruction.md`

One, and it is in the source documents rather than the implementation: the todo's syntax note and
acceptance criteria mention a "numeric equivalent" (`7,7`) alongside `h3`, but
`CommandDispatcher::parseMovesText` — which the same documents mandate calling and forbid changing
— only recognises `<letter><digits>` move text; it has never parsed `x,y` tokens. `!yxAnalz`
therefore accepts alphabetic move text only. Adding numeric-token parsing would mean changing
`parseMovesText` (shared with `!pos`/`!play`) or forking it, both explicitly out of bounds. The
`y,x`-conversion half of that criterion is covered directly at the protocol layer, where the unit
test passes `Coord` values in. Filed as a follow-up decision for the user, not silently extended.
