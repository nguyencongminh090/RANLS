# ANLZ-07 — Analyze Mode: Stop/restart busy-loops instead of settling once a search converges

**Status:** 🔲 OPEN (Sprint 12 Active) — filed 2026-09-04

Regression surfaced against the just-shipped **ANLZ-06**. ANLZ-06 correctly stopped an
analysis-intent search's terminal coordinate from being played (Stop no longer drops a stone) —
but the user's report shows a new symptom on the same transcript: **Analyze Mode never settles once
the engine has found a forced win, and instead busy-loops `STOP` → full `YXBOARD` redump → `YXNBEST`
search → discarded coordinate → `STOP` → … forever**, at native CPU speed (the pasted log shows
dozens of full search-and-discard cycles in what the `tm` timing fields indicate is well under a
second each, and it does not stop on its own).

## Root cause

`/systematic-debugging` Phase 1–2 (traced from the pasted transcript, no live engine needed — the
transcript itself is the evidence):

| Step | Location | Behaviour |
|---|---|---|
| search finishes, coordinate discarded | `EngineController`'s `protocol_->signal_move` handler (`engine_controller.cpp`, the `wasSearching` block) | Per ANLZ-06: Analysis-intent coordinate is *not* played, but `setState(EngineState::Idle)` still fires unconditionally once `wasSearching` |
| Idle transition re-arms the loop | `main_window.cpp:633-640` (`signal_state_changed` → `Idle` handler) | `scheduleAnalyzeModeRestart()` runs on **every** transition to Idle, not just ones following an actual `signal_board_changed` — the doc comment says this is deliberate ("resume pondering the current position if Analyze Mode is on") |
| restart re-sends the identical request | `MainWindow::scheduleAnalyzeModeRestart()` (`main_window.cpp:1129-1156`) | `controller_.stopAnalysis()` (sends `STOP`) then `controller_.analyze()` (re-sends the full `YXBOARD …DONE` + `YXNBEST`) unconditionally — no check that the position, engine config, or prior result actually changed since the last run |

There is **no termination condition and no minimum interval** between restarts. As designed,
Analyze Mode is meant to be continuous background pondering — but "continuous" here means
"re-run the exact same one-shot search over and over with no new information," not "keep the
existing search alive." Once the engine's search space collapses (a forced mate is found, so the
search converges almost instantly every time — the transcript shows `tm 0`/`tm 1` for the *entire*
depth-2-through-99 sweep), each iteration completes in native time, so the restart fires as fast as
the engine can be re-driven — a tight loop pegging a CPU core and flooding the Engine Log (already
bounded at 5000 lines by `EngineLogModel`, but still constant churn) for as long as Analyze Mode
stays on and the position doesn't change. This is not specific to "winning" — any position whose
search converges quickly and stably (a fully solved endgame, a forced sequence, or simply a small
board) will do the same; the user's mate-in-3 report is just the easiest way to trigger it because
Rapfi-family VCF/VCT search resolves forced wins almost immediately.

**Working reference:** `maybeStartAutoMove()` (`main_window.cpp:1061-1093`) has a comment
explaining why *it* can't loop: "After the engine's move lands, side-to-move flips … no infinite
loop." `scheduleAnalyzeModeRestart()` has no equivalent invariant — nothing about its own
post-conditions prevents it from re-arming itself on a position it just finished analysing.

## Area

- `src/main_window.cpp` — `scheduleAnalyzeModeRestart()` and the `signal_state_changed`→`Idle`
  handler in `connectSignals()` (`~L633-640`). Likely fix surface.
- `src/engine/engine_controller.{h,cpp}` — `analyze()` / the `signal_move` handler: read-only
  reference for what "search converged with no new information" would need to detect (e.g. same
  `currentPath()` + same best line/eval as the previous completed analysis-intent search).
- Read-only reference: `docs/todo/ANLZ-01-continuous-analyze-mode.md`,
  `features/analyze-mode/planning.md` (the original "continuous background analysis" design intent
  — this task must not regress its stated goal of covering every visited position's WinGraph point).

## Priority

P1 — pegs a CPU core indefinitely and spams the Engine Log at native speed with Analyze Mode on;
degrades the whole app (UI thread shares the process) for as long as the position sits on a
converged/solved line, which the user cannot predict or avoid short of toggling Analyze Mode off.

## Depends on / relates to

- **ANLZ-01** — the continuous-background-analysis design this restart loop implements. Any fix
  must keep filling one WinGraph point per newly-visited position; it must not silently stop
  Analyze Mode from ever re-pondering a position the user returns to.
- **ANLZ-05** — added the "resume pondering on Idle" restart trigger this task is scoping down.
- **ANLZ-06** — this task's transcript is the direct continuation of the ANLZ-06 report; ANLZ-06's
  fix (discarding the coordinate) is correct and unrelated to this loop — it just stopped masking
  it (previously the discarded move — no, the *played* move — would have advanced the position and
  broken the loop by accident; now that it's correctly discarded, nothing else does).

## Problem

Analyze Mode is meant to be a study aid that quietly keeps the WinGraph current. Instead, once a
position's search converges quickly and repeatably, it turns into an unthrottled busy loop:
`STOP` → full board redump → search → discard → `STOP` → … with no backoff, no "nothing changed,
stop re-arming" check, and no user-visible way to tell the difference between "still thinking" and
"stuck re-thinking the same solved position forever."

## Scope

To resolve with the user before implementing (open questions — do not implement without picking
one, see `docs/instruction/ANLZ-07-*.md` for how `/implement-task` should surface this):

1. Should a restart be skipped when the just-completed analysis-intent search's result (best line +
   eval, or an explicit mate/solved marker if the protocol exposes one) is unchanged from the
   previous completed search *on the same position*? This directly addresses "on winning" but also
   the general "converged and stable" case.
2. Should there be a minimum wall-clock interval between successive restarts of the *same* position
   (a debounce/backoff), independent of (1), as a blanket protection against any fast-converging
   search?
3. Do (1) and (2) compose (both), or is one sufficient?

## Scope boundary

- Not a change to `YXNBEST`/`analyze()`'s request shape, ANLZ-06's discard-the-coordinate gate, or
  the "one WinGraph point per visited position" guarantee ANLZ-01 established.
- Not a change to one-shot Analyze / Stop (Analyze Mode off) or "Engine plays &lt;side&gt;"
  (ENG-02/UI-06) — those don't self-restart.
- Whether the engine protocol has a genuine "ponder indefinitely without a bestmove" mode (raised
  and rejected for a different reason in the ANLZ-02/"Toggle Ponder" drop, see `TODO.md` Backlog
  notes) is out of scope here — this task only needs to stop the *restart-from-scratch* loop from
  running unthrottled, not redesign the search-driving protocol.
