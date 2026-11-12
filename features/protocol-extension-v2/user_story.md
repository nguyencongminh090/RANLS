# Protocol Extension v2 (`.ptc` basic functions) — user stories

Source: user question 2026-11-04 after reviewing `features/protocol-extension/` (PROTO-03, shipped):
"Is this PTC command set optimal? Can we design basic functions for users?" Not started — design
stage only. Builds on, does not reopen, PROTO-03's Q1–Q9.

## Problem in one paragraph

PROTO-03's `.ptc` is safe and minimal, but an extension can only *display* values: 3 sinks
(`set_status_field`, `toast`, `log`), comparisons only (no arithmetic, no string helpers), one
reply line in → one action out, and no way to chain commands or touch the board. A real engine's
output (e.g. `MOVEEVAL ... 0.93`) cannot be shown as `93%`, and nothing can be highlighted on the board.

## Actors

- **Extension author** — writes a `.ptc` for a Gomocup/Yixin-family engine without touching C++.
- **User** — types `!yourCommand` in the console/palette and sees a useful result.
- **Maintainer** — wants the interpreter to stay bounded, fail-closed and exhaustively testable.

## Stories

- **E1** As an author, I format a captured number (`round`, `fmt`) so `0.93` shows as `93%`.
- **E2** As an author, I do basic arithmetic on captures (`+ - * /`) without free variables or loops.
- **E3** As an author, I mark cells on the board (`highlight`, `mark`) from a reply.
- **E4** As an author, one command can trigger another known command (`run`) with a hard recursion bound.
- **E5** As a maintainer, every new function is whitelisted, argument-count checked at load time, counted
  against the Q5 limits, and unit-tested; a bad `.ptc` is still rejected whole.

## Out of scope (unless `planning.md` decides otherwise)

`group = "analysis"` / Stop integration, multi-line replies, hot-reload, non-Gomocup protocol families
(all already deferred in `features/protocol-extension/planning.md` "Follow-ups").
