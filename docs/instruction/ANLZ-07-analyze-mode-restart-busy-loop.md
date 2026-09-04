# ANLZ-07 — instruction

Read `docs/todo/ANLZ-07-analyze-mode-restart-busy-loop.md` in full first — this file assumes its
root-cause trace.

## Before writing any code

The three open questions in the todo file's "Scope" section are **not yet resolved with the user**.
`/implement-task ANLZ-07` (or whoever picks this up) must resolve them first — this is a design
decision (how Analyze Mode decides "this position is done, stop re-searching it"), not a bug with
one obvious fix. Do not guess a default silently; ask, or if working non-interactively, propose the
combination below as the recommended default and get explicit sign-off before implementing:

**Recommended default (for the asking, not to implement unprompted):** both (1) and (2) from the
todo file, composed:
- Track, per `EngineController` analysis-intent search, the previous completed result on the
  *same* `currentPath()` (best move + eval text, e.g. what `EngineStatus`/`PVLine` already carry —
  no new engine query needed). If the new result is identical to the previous one for the same
  position, don't re-arm `scheduleAnalyzeModeRestart()` — treat the position as converged and stay
  Idle until `signal_board_changed` fires again (a real position change) or the user forces a
  restart (toggling Analyze Mode off/on).
- As a blanket backstop independent of the above (protects against results that keep changing by a
  hair — e.g. node-count jitter — without ever truly stabilizing), also enforce a minimum interval
  (a few hundred ms) between successive restarts of the same position, so even a
  never-quite-converging search can't out-pace the UI.

## Why this shape

- Keeps ANLZ-01's guarantee ("every visited position gets a WinGraph point") intact — the very
  first analysis of a newly-visited position always runs; only the doesn't-say-anything-new
  *re-runs* are what's being suppressed.
- Doesn't need a new engine command or protocol change — the "did anything change" check is pure
  comparison of data `EngineController` already parses into `EngineStatus`/`PVLine`.
- Naturally covers "found a forced mate" (the case in the report) and any other position whose
  search stabilizes quickly, without hardcoding "is this a mate score" detection tied to a specific
  engine's `ev` text format.

## Regression test

Per `CLAUDE.md`'s bug-fix workflow, this needs a failing-first regression test before the fix.
`MainWindow`-level: reuse the `test_anlz05_no_automove_action.cpp` / `test_anlz06_search_intent_gate.cpp`
pattern (real `MainWindow` + a wire spy on the engine's stdin, or a fake `EngineProcess`) — feed two
identical completed analysis results for the same position in a row, assert only **one** `YXBOARD`
request round-trip is sent, not two. Also a timing-independent case for the interval backstop (fake
clock / injectable time source — don't rely on real `g_usleep`/sleep in the test, per ENG-01's
established "no blocking waits" convention).

## Scope discipline

- `EngineController`/`MainWindow` only, same layering as ANLZ-05/06. No protocol change.
- Don't touch `YXNBEST`'s request shape, ANLZ-06's `SearchIntent` gate, or ENG-02/UI-06's
  `requestEngineMove()` path (it doesn't self-restart, so it's already unaffected — verify this
  stays true rather than assuming it).
