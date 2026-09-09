# CONS-03 — `!help` should document `!yxAnalz` / YXANALZ as an engine-protocol extension

**Status:** 🔲 OPEN (Backlog)
**Area:** `src/command/command_dispatcher.cpp` (`printHelp` preamble and/or the `!yxAnalz`
`CommandSpec` usage/summary registered in `registerBuiltins`); `tests/test_proto07_yxanalz_console.cpp`
(the `commandUsage`/`!help` assertions if the usage string changes)
**Priority:** P3
**Source:** User request 2026-09-09, immediately after PROTO-07 merged (PR #34) — "Update !help
document to help YXANALZ (the external protocol)".
**Design:** none — scoped directly; small doc/string change.
**Depends on / relates to:** PROTO-07 (added `!yxAnalz` + `EngineController::analyzeMoves` +
`GomocupProtocol::generateAnalyzeMovesRequest`), CONS-01/02 (the console command registry + the
`!help` grouped output this edits)

## Problem

PROTO-07 registered `!yxAnalz` with a single-line `!help` summary
("Analyze only the listed root moves (YXANALZ), e.g. !yxAnalz h3 h2 h9"). That is enough to find
the command but does not tell the user that:

- `YXANALZ` is an **engine-protocol extension** — a Rapfi / Yixin-protocol command, not part of the
  base Gomocup protocol. An engine that does not implement it will answer `ERROR` (or ignore the
  move list and search normally); `!yxAnalz` is only meaningful against an engine that supports it.
- it is a **full search** (honours the configured limits, is interruptible with Stop / `!stop`),
  the trailing best-move coordinate is analysis-only (no stone is placed), and it is refused while
  Analyze Mode is ON.
- input is **alphabetic move text only** (`h3 h2 h9`), matching `!pos` — numeric `x,y` is not
  accepted (see PROTO-07 "Deviation").

`!help` is the in-app reference (there is no command reference in `README.md`), so this context
belongs there.

## Scope (in order)

1. Decide the vehicle with the smallest footprint:
   - richer `summary` on the existing `!yxAnalz` `CommandSpec` (one denser line), **and/or**
   - a short note in `printHelp()`'s preamble (the block that currently prints the
     "Internal commands must start with '!'" / "raw protocol" lines) that some `[analysis]`
     commands (`!yxAnalz`) drive **engine-specific protocol extensions** and need an engine that
     implements them.
2. Make the edit. Keep it to `printHelp()` / the `registerBuiltins` `CommandSpec` for `!yxAnalz` —
   no behaviour change, no new command, no new group.
3. If the `!yxAnalz` `usage` string changes, update the exact-match assertions in
   `tests/test_proto07_yxanalz_console.cpp` (`commandUsage("yxanalz") == …`) in the same change.
   Prefer changing only `summary` (not `usage`) to avoid churning the test.
4. Optionally: add a one-line `!yxAnalz` mention to `CHANGELOG.md` `[Unreleased]` only if the
   PROTO-07 entry there does not already cover it (it does — likely nothing to do).

## Acceptance criteria

- `!help` output makes clear, without the user consulting external docs, that `!yxAnalz` (a) needs
  an engine that implements the `YXANALZ` protocol extension, (b) runs a real interruptible search,
  (c) places no stone, (d) takes alphabetic move text.
- No new command, group, or behaviour; `printHelp()` still renders every existing group unchanged.
- `ctest` stays green — `test_proto07_yxanalz_console.cpp`'s `!help`/`commandUsage` assertions
  updated if and only if the usage string moved.
- No `src/` change outside `command_dispatcher.cpp`.

## Scope boundary

- Do **not** change what `!yxAnalz` does, how `parseMovesText` parses, or the
  `generateAnalyzeMovesRequest` wire output — this is help text only.
- Do **not** add numeric `x,y` console input (that is the separate PROTO-07 deviation — needs a
  `parseMovesText` change and its own `CODE`).
- Do **not** restructure `printHelp()`'s grouping/sorting or the `CommandSpec` struct.
- Do **not** add a `README.md` command reference in this task (larger scope; file separately if
  wanted).
