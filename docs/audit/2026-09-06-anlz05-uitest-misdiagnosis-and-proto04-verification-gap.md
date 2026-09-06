# 2026-09-06 — `test_anlz05_no_automove_action` misdiagnosis chain + the PROTO-04 verification gap that seeded it

## Prompt

While scoping a fix for the long-standing `test_anlz05_no_automove_action` UI-test failure
(`/systematic-debugging`, cross-checked against a Gemini report), the failure's *history* turned out
to be the more interesting finding: a real regression was mislabelled once and then re-confirmed as
"pre-existing" six times. This entry records the process lesson. The code fix is
`docs/fix-log/2026-09-06-anlz05-uitest-trailing-coordinate-mock-engine.md`.

## Decision / conclusion

The failure is a **PROTO-04 regression** (`502bd77`, PR #18, 2026-09-05), not a "pre-existing
environmental flake". Two process defects let it live for ~30 commits:

### 1. PROTO-04 shipped without running the suite its regression landed in

PROTO-04's fix-log Verification section lists `ninja ranls-gui-tests` + `./tests/ranls-gui-tests`
(194/194) + `ninja ranls-gui`. It never ran `ranls-gui-ui-tests`. The `pendingStopFlush_` /
`sendOrDefer()` change is engine-controller-wide, and its only widget-level regression test coverage
(`test_anlz05_no_automove_action`, real `MainWindow` + wire spy) lives in exactly the suite that was
skipped.

### 2. "Confirmed identical on baseline" became circular after the first mislabel

ENG-03 (same day, branched off a `main` that already contained `502bd77`) was the first task to run
`ranls-gui-ui-tests` post-PROTO-04. It saw the failure, checked it against "unmodified main", found
it reproduced, and wrote "pre-existing unrelated flake". Every subsequent task — UI-14, PROTO-03,
PORT-01, PORT-02, PORT-03 — repeated the same check against a baseline that was itself already
post-`502bd77`, so each "reproduces identically on baseline `<hash>`" was true and uninformative. No
one bisected past the point where the test had last been green (ANLZ-07, PR #17).

The `docs/notes/` entry compounded it by proposing "environmental (no display / no real engine)" as
the root cause; a later revision correctly identified the `pendingStopFlush_` mechanism but still
did not name it a regression.

## Findings / recommendations

- **A change to `src/engine/` (EngineController, EngineProcess, GomocupProtocol) or any protocol
  behaviour must run *both* `ranls-gui-tests` and `ranls-gui-ui-tests` before merge.** The model
  suite passing is not sufficient evidence for an engine-controller change.
- **"Reproduces on baseline" is only evidence of "not introduced by *this* branch".** It is not
  evidence of "pre-existing / won't-fix / environmental". When a test is red on `main`, the honest
  next step is `git bisect` (or `git log -S` on the touched symbol) back to the last green run — not
  a note that inherits the label. State the baseline commit *and* whether a bisect was done.
- **A regression test that self-skips (no display / no engine) is not "environmental" when it does
  run.** `test_anlz05_no_automove_action` runs fine on any host with a display server; its failure
  was deterministic (5/5), not flaky. "Flake" and "environmental" were both wrong words.
- **`docs/notes/` root-cause guesses should not harden into fix-log "pre-existing" claims without a
  reproduction + bisect.** Six fix-log entries cited the note's framing as settled fact.

## Not changed

- No CI/verification automation added here (the repo has no CI gate on `ranls-gui-ui-tests` yet;
  PORT-03 scope item 4, a Windows Actions job, is the nearest tracked work). This entry is the
  written standard; wiring it is a separate decision.
- The `pendingStopFlush_` design (condition-based flush, no safety-net timeout) is unchanged and
  remains the user's deliberate call — see PROTO-04's fix-log "Left out of scope". A protocol-level
  "search terminated" signal to decouple the flush from coordinate-shaped output was considered and
  **deferred**: real engines close the loop for every realistic position (including the empty board),
  so the exposure does not justify a new protocol path.
