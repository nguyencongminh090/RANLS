# 2026-09-05 — Commands sent right after stopAnalysis() raced the aborted search's own teardown (PROTO-04)

## Correction to PROTO-03

`docs/fix-log/2026-09-05-proto-03-database-query-missing-color.md` (same session, filed shortly
before this entry) diagnosed the user's pasted Engine Log — a run of
`ERROR Unknown command: <coord>` lines following `yxquerydatabaseallt` — as a malformed position
block (missing the `,color` field). **That fix is real and harmless (kept), but it was wrong as the
root cause.** The user pushed back with a sharper read of the same transcript: the exact same
outbound bytes appear in the log both with and without the error, e.g. this occurrence has zero
errors for the identical 2-field shape PROTO-03 "fixed":

```
yxquerydatabaseallt
12,13
13,13
13,10
10,10
11,11
9,10
11,9
DONE
12,10
STOP
YXBOARD
...
```

A deterministic parser rejecting one shape rejects it every time. It doesn't here — so the defect
isn't the shape, it's timing: the same query sometimes lands cleanly and sometimes doesn't, which is
the signature of a race, not a format bug.

## Root cause

Every occurrence of the error in the transcript is preceded by `STOP` immediately followed by
`yxquerydatabaseallt`. Tracing that pattern into the code:

- `EngineController::stopAnalysis()` ([engine_controller.cpp:317](../../src/engine/engine_controller.cpp))
  writes `"STOP"` to the engine's stdin, then **synchronously** flips `state_` back to `Idle` in the
  same call — before the real engine subprocess has actually finished aborting its in-flight YXNBEST
  search.
- `EngineProcess::sendLine()` ([engine_process.cpp:135](../../src/engine/engine_process.cpp)) is
  pure fire-and-forget: `write_all` + `flush()`, no acknowledgment, no queue.
- The click handler that starts this sequence runs entirely synchronously in one call stack:
  `stopAnalysis()` → `gameState_.makeMove(pos)` → `signal_board_changed` →
  `controller_.queryDatabase()` — writing the next protocol block to stdin immediately after `STOP`,
  with no wait for the aborted search's own trailing coordinate line (which ANLZ-06's existing
  comments already document YXNBEST still emits, asynchronously, even after STOP).

So the GUI's own state says "idle" the instant `STOP` is written, while the real subprocess is still
asynchronously winding down and about to flush that trailing coordinate to stdout. Whether the next
command lands cleanly or corrupts the engine's read position depends on OS/process scheduling —
exactly the observed intermittency.

## Second transcript: direct confirmation, independent of format

After the fix below was implemented, the user provided a second real transcript and asked that
`generateDatabaseQuery()`'s `y,x` (no color) shape be left exactly as it always was. That transcript
settles the root-cause question directly: it contains 12 back-to-back `yxquerydatabaseallt` calls in
that exact bare `y,x` format, all clean — zero errors — followed by exactly one failing occurrence.
The one failure is structurally distinct from all 12 successes in exactly one respect:

- **Every successful occurrence**: `STOP` → the aborted search's trailing coordinate line arrives
  (logged) → *then* `yxquerydatabaseallt` is sent. By the time the query goes out, the engine has
  already visibly settled.
- **The one failure**: `STOP` → `yxquerydatabaseallt` sent immediately, with no coordinate line
  between them → the trailing coordinate only shows up *after* the query's `DONE`, by which point
  every line of the query has already been echoed back as `ERROR Unknown command: <coord>`.

This is PROTO-04's race caught in the wild, format held constant. `generateDatabaseQuery()`'s color
field addition (PROTO-03) has been reverted in full — see its detail file's correction note.

## Fix

`EngineController` now gates every outbound command batch that isn't `STOP` itself behind a new
`sendOrDefer()` method and a `pendingStopFlush_` flag:

- `stopAnalysis()` arms `pendingStopFlush_` only when it interrupts a live **Analysis-intent**
  (YXNBEST) search — the one case ANLZ-06 already established still emits a trailing coordinate
  despite `STOP`. A Move-intent (`BOARD`) search's reply is genuinely suppressed by `STOP` per
  `docs/protocol.md`'s `STOP` entry, so there is nothing to wait for there — waiting anyway would
  hang every future command behind a coordinate that never arrives.
- `analyze()`, `requestEngineMove()`, `queryDatabase()`, and `sendConfig()` now route their generated
  command batches (and, for the two search starters, their `searchIntent_`/`setAnalyzing`/`setState`
  bookkeeping too — deferring the wire commands but not the state would let a stale coordinate get
  misattributed to a not-yet-sent search) through `sendOrDefer()` instead of calling `engine_.sendLine()`
  directly.
- The `protocol_->signal_move` handler in `connectProtocolSignals()` — already the place that
  receives every inbound coordinate line — flushes the queue, in call order, once `pendingStopFlush_`
  is set and any coordinate line arrives (nothing else can produce one while the queue is held, since
  everything queued behind it stays unsent until this fires).
- `stopEngine()` and the `signal_process_died` handler both clear `pendingStopFlush_`/drop the queue
  outright — a stopping or dead process will never deliver the coordinate being waited on, so holding
  the queue would silently swallow every future command until the app blocks forever.

No arbitrary timeout: the flush is purely condition-based (the actual coordinate line, or the
process going away), matching the user's explicit ask.

## Files changed

- `src/engine/engine_controller.h` — new `pendingStopFlush_` flag, `pendingActions_` queue, private
  `sendOrDefer()` method.
- `src/engine/engine_controller.cpp` — `stopAnalysis()`, `analyze()`, `requestEngineMove()`,
  `queryDatabase()`, `sendConfig()`, `stopEngine()`, the `signal_process_died` handler, and the
  `signal_move` handler in `connectProtocolSignals()` all updated as described above.
- `src/engine/gomocup_protocol.cpp` — `generateDatabaseQuery()` reverted back to bare `y,x` lines
  (PROTO-03's color-field addition removed); `tests/test_gomocup_protocol.cpp`'s two PROTO-03 cases
  updated to pin the original shape instead.
- `tests/test_proto04_stop_flush_race.cpp` (new, 5 cases) + `tests/CMakeLists.txt` registration:
  a deferred `queryDatabase()` after stopping a live Analysis-intent search is held until the
  trailing coordinate arrives, then sent; `queryDatabase()` sends immediately when there was no live
  search to stop (matches the log's zero-error occurrences); a Move-intent stop does not wait for a
  coordinate `STOP` genuinely suppresses; multiple deferred commands (`queryDatabase()` +
  `sendConfig()`) flush in call order; a process death drops the pending queue instead of leaking it
  forever (verified by a stray post-crash coordinate line *not* retroactively flushing it).

## Verification

- `ninja ranls-gui-tests` — built cleanly, no new warnings.
- `./tests/ranls-gui-tests -tc="*PROTO-04*"` → `test cases: 5 | 5 passed`, `assertions: 33 | 33 passed`,
  `Status: SUCCESS!`.
- Full suite: `./tests/ranls-gui-tests` → `test cases: 194 | 194 passed | 0 failed | 0 skipped`,
  `assertions: 2392 | 2392 passed | 0 failed`, `Status: SUCCESS!` — no regressions (up from 189/2359
  before this fix + PROTO-03's 2 cases already counted there).
- `ninja ranls-gui` — full GUI app builds cleanly (same 3 pre-existing `-Wunused-function` warnings
  in `gomocup_protocol.cpp`, none new).
- Manual live-engine smoke NOT run (no engine/display on this build host).

## Left out of scope

- `sendRawCommand()` (used by `!about`, and by `stopEngine()`'s own `YXSAVEDATABASE`/`END`) is
  deliberately left un-deferred — a shutdown shouldn't wait on a stale search reply, and `!about` is
  a debug command with no ordering dependency on search state.
- No safety-net timeout on `pendingStopFlush_` beyond the two explicit clear paths (process death,
  intentional `stopEngine()`) — if a future engine build's Analysis-intent search were ever stopped
  by `STOP` with genuinely no trailing coordinate (contradicting the ANLZ-06 assumption this fix
  relies on), the queue would hang until one of those two paths fires. Per the user's explicit
  request this fix is condition-based, not time-based; flagging this as the one scenario a defensive
  timeout would still need to cover, should it ever come up in practice.
- PROTO-03's format change has been reverted in full (not just re-attributed) — the user asked
  explicitly not to touch this protocol shape, and the second transcript confirms it was never
  necessary.
