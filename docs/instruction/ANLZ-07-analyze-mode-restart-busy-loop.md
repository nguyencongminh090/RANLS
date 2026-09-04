# ANLZ-07 — instruction

Read `docs/todo/ANLZ-07-analyze-mode-restart-busy-loop.md` in full first — this file assumes its
root-cause trace.

## Design decision — RESOLVED 2026-09-04

Resolved with the user: **skip-restart-if-unchanged only** — no minimum-interval backstop.

- Track, per `EngineController` analysis-intent search, the previous completed result on the
  *same* `currentPath()` (best move + eval text, e.g. what `EngineStatus`/`PVLine` already carry —
  no new engine query needed). If the new result is identical to the previous one for the same
  position, don't re-arm `scheduleAnalyzeModeRestart()` — treat the position as converged and stay
  Idle until `signal_board_changed` fires again (a real position change) or the user forces a
  restart (toggling Analyze Mode off/on).
- Deliberately **no** time-based backstop. If a search's reported result keeps changing between
  runs on the same position (e.g. genuine deepening progress, not mere jitter), it is *not*
  converged yet and re-arming is correct — a minimum interval would only mask that case, not fix
  it. Should a never-quite-converging search cause a rapid loop in practice, that would be a
  distinct, separate report — do not add the backstop speculatively.

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
request round-trip is sent, not two. Also cover: a *changed* result on the same position still
restarts (deepening/new-info case must keep working), and a real position change
(`signal_board_changed`) always restarts regardless of the previous result (never gate on
`currentPath()` alone without also comparing the result).

## Scope discipline

- `EngineController`/`MainWindow` only, same layering as ANLZ-05/06. No protocol change.
- Don't touch `YXNBEST`'s request shape, ANLZ-06's `SearchIntent` gate, or ENG-02/UI-06's
  `requestEngineMove()` path (it doesn't self-restart, so it's already unaffected — verify this
  stays true rather than assuming it).
