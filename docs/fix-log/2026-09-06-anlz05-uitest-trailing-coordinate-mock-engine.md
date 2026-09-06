# `test_anlz05_no_automove_action` Scenario B hung on a trailing coordinate the mock engine never emits — a PROTO-04 regression, not "pre-existing/environmental"

**Status:** ✅ FIXED

Inline fix, no `TODO.md` CODE (small test-only fix — same weight as UI-14 / the gutter-font
fix). Branch `fix/anlz05-uitest-trailing-coord`.

## Prompt

`tests/test_anlz05_no_automove_action.cpp` (suite `ranls-gui-ui-tests`) fails at
[line 135](../../tests/test_anlz05_no_automove_action.cpp#L135):

```
REQUIRE( pumpUntil([&] { return sent(wire, "BEGIN"); }) ) is NOT correct!
  values: REQUIRE( false )
```

This failure was recorded as a **pre-existing environmental flake** in
`docs/notes/2026-09-06-anlz05-ui-test-preexisting-failure.md` and carried as "pre-existing, identical
on baseline" through six consecutive fix-log entries (ENG-03, UI-14, PROTO-03, PORT-01, PORT-02,
PORT-03). A `/systematic-debugging` pass over that history — cross-checked against a parallel Gemini
report — established it is neither pre-existing nor environmental.

## Investigation

**Regression window (git + fix-log).** The test was added green by ANLZ-05 (PR #15, 2026-09-04) and
was still green at ANLZ-07 (PR #17, 2026-09-04 — that fix-log's only `ranls-gui-ui-tests` failure is
the unrelated UI-12 scroll flake). It first failed at **PROTO-04 (PR #18, `502bd77`, 2026-09-05)**,
which introduced `EngineController::pendingStopFlush_` / `sendOrDefer()`. ENG-03 the same day was the
first task to run `ranls-gui-ui-tests` *after* PROTO-04 merged; its "reproduces identically on
unmodified main" note is true but circular — `main` already contained the regression. Every later
"confirmed on baseline" check inherited the same contaminated baseline; nobody bisected past
`502bd77`.

**Mechanism (verified against current source).** Scenario B opens by calling `p.ctrl().stopAnalysis()`
while Scenario A's `YXNBEST` (Analysis-intent) search is live:

1. [`stopAnalysis()`](../../src/engine/engine_controller.cpp#L438) sets
   `willEmitTrailingCoord = (state_==Analyzing && searchIntent_==Analysis)` → true →
   `pendingStopFlush_ = true`, `searchIntent_ = None`, `setState(Idle)`.
2. `REQUIRE(engineState()==Idle)` passes (state flips synchronously).
3. `maybeStartAutoMove()` → `requestEngineMove()` → [`sendOrDefer()`](../../src/engine/engine_controller.cpp#L476)
   sees `pendingStopFlush_` and **queues `BEGIN` into `pendingActions_`** instead of writing it.
4. `pendingStopFlush_` is released only in the `signal_move` handler
   ([engine_controller.cpp:136-141](../../src/engine/engine_controller.cpp#L136-L141)) — on *any*
   parsed coordinate line — or on process death.
5. [`GomocupProtocol::parseLine()`](../../src/engine/gomocup_protocol.cpp#L352) emits `signal_move`
   only for a line starting with a digit and containing a comma that validates as a coordinate. The
   PORT-03 `mock_engine` echoes stdin (`STOP`, `YXBOARD`, `YXNBEST 5`, …) and — deliberately, per its
   header comment — never emits a coordinate-shaped line. So `signal_move` never fires, the queue
   never flushes, `BEGIN` never reaches the wire, and `pumpUntil` times out after 3 s.

`test_proto04_stop_flush_race.cpp` and `test_anlz06_search_intent_gate.cpp` do not hit this because
they simulate the engine's reply by hand (`proc.signal_line_received.emit("7,7")`). The ANLZ-05 test,
written before PROTO-04, assumed the wire was idle the instant `stopAnalysis()` returned.

**Not a product bug for the realistic case.** The test board is empty. A real engine — asked via
`YXBOARD/DONE/YXNBEST`, `BOARD/DONE`, or `BEGIN` — recognises the empty board and returns a default
move; an aborted `YXNBEST` still emits its trailing coordinate (ANLZ-06). With a real engine
Scenario B passes: the trailing coordinate releases `pendingStopFlush_` and `BEGIN` flushes. The
residual "engine stopped so fast it emits no trailing coordinate at all" edge was already flagged in
PROTO-04's fix-log "Left out of scope" as the one scenario a defensive timeout would cover, and
deliberately deferred per the user's explicit condition-based-only design ask — unchanged here.

## Fix

`tests/test_anlz05_no_automove_action.cpp` Scenario B only: after `p.ctrl().stopAnalysis()`, feed the
trailing coordinate a real engine would send —

```cpp
p.eng().signal_line_received.emit("7,7");
```

— mirroring `test_anlz06` / `test_proto04`, with a comment explaining the PROTO-04 dependency.
`"7,7"` is the realistic default centre move for the 15×15 empty board. No production code touched,
no timeout added, `mock_engine` unchanged (7 suites rely on it staying protocol-blind), the test's
own assertions (`BEGIN` sent, `YXNBEST` not) unchanged.

## Verification

- Fresh `cmake -G Ninja` build, no new warnings.
- `ranls-gui-ui-tests -tc="*ANLZ-05*"` — **5/5 SUCCESS** (was 0/5).
- `ranls-gui-ui-tests` full — **30/30** (was 29/30).
- `ranls-gui-tests` — 209/209 / 2466 assertions, unchanged.
- `rel02-version-single-source` — pass.
- `port02-style-css-bundled` — **fails on a fresh build on this host** (`org/ranls/style.css` not
  bundled into `ranls-gui`). Pre-existing and unrelated: this fix only edits a file compiled into
  `ranls-gui-ui-tests`, never into `ranls-gui`, which is what that test inspects. Recorded as a
  separate finding for the user (PORT-02 verified it bundled in its own build; regressed since on
  this host).
- Manual live-engine smoke not run (no engine binary / display on the build host).

## Related

- `docs/audit/2026-09-06-anlz05-uitest-misdiagnosis-and-proto04-verification-gap.md` — the process
  lesson (PROTO-04 shipped without running `ranls-gui-ui-tests`; six-entry circular-baseline
  confirmation chain).
- `docs/notes/2026-09-06-anlz05-ui-test-preexisting-failure.md` — original investigation note;
  appended with the corrected "PROTO-04 regression" framing.
- `docs/fix-log/2026-09-05-proto-04-stop-flush-race.md` — the regressing change.
