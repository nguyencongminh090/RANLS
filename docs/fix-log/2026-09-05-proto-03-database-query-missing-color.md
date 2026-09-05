# 2026-09-05 — generateDatabaseQuery's position block omits color, desyncing the engine (PROTO-03)

> **Correction (same day, see [PROTO-04](2026-09-05-proto-04-stop-flush-race.md)) — fully reverted,
> not merely superseded.** This entry's "root cause" claim is wrong, and the format change below has
> been reverted out of the codebase. Two rounds of user pushback, both confirmed against real
> transcripts:
> 1. The first pasted transcript shows the identical outbound `y,x` (no color) shape sometimes
>    erroring and sometimes not — a deterministic parser rejection couldn't do that.
> 2. A second transcript settled it directly: 12 back-to-back `yxquerydatabaseallt` calls in exactly
>    this bare `y,x` shape, **zero errors**, followed by exactly one failing occurrence — and that one
>    occurrence is structurally different in only one way: `STOP` is followed immediately by the
>    query with no coordinate reply in between, whereas all 12 successful ones have the aborted
>    search's trailing coordinate line land *before* the query is sent. That is PROTO-04's race,
>    directly observed, independent of format.
>
> The user explicitly asked not to change this protocol shape. `generateDatabaseQuery()` is back to
> the original bare `y,x` lines; the regression tests below were updated to match and pin that shape.

## Summary

The user pasted an Engine Log transcript in which every `yxquerydatabaseallt` query the GUI sends
(fired from `MainWindow::connectSignals()`'s `signal_board_changed` handler, once per move, whenever
"show database" is on) comes back with one `ERROR Unknown command: <coord>` per move already on the
board, e.g.:

```
yxquerydatabaseallt
12,13
13,13
13,10
DONE
...
ERROR Unknown command: 12,13
ERROR Unknown command: 13,13
ERROR Unknown command: 13,10
```

## Root cause

`GomocupProtocol::generateDatabaseQuery()` (`src/engine/gomocup_protocol.cpp:328`) built its
position-block lines as bare `"y,x"`:

```cpp
cmds.push_back(std::to_string(c.y) + "," + std::to_string(c.x));
```

Every other position-block generator in the same file — `generateAnalyzeRequest()` (`YXBOARD`) and
`generateMoveRequest()` (`BOARD`) — emits `"y,x,color"` via `coordToEngine(c) + "," + color`. The
engine's `yxquerydatabaseallt` handler reads the same position-block grammar those two blocks use
(Rapfi's `docs/protocol.md` §4.2/§7.2 documents all position blocks, including the `YXQUERYDATABASE*`
family, as one shared `x,y[,color] … DONE` format). A 2-field `"y,x"` line isn't a valid entry in
that grammar, so the engine's block reader bails without consuming the block body; the outer command
loop then re-reads each stray coordinate token as a new top-level command, printing one
`ERROR Unknown command: <coord>` per move already on the board — and, once observed further down the
same transcript, can leave the stdin token stream shifted enough to desync a subsequent, otherwise
well-formed `YXBOARD` block too (its trailing coordinate/`DONE` tokens misread as unknown commands).

This reproduced on every move once "show database" was on — not timing-dependent, not
engine-specific: `docs/todo/UI-07-pv-panel-still-accumulates-across-positions.md`'s own diagnostic
transcript (captured earlier, for an unrelated bug) shows the identical malformed
`yxquerydatabaseallt`/bare-coordinate pattern, confirming this has been present in every Engine Log
since `generateDatabaseQuery()` was written.

## Fix

`generateDatabaseQuery()` now builds each line the same way `generateAnalyzeRequest()` does —
`coordToEngine(path[i]) + "," + color` with `color` alternating `1`/`2` by ply index — so its
position block is byte-for-byte the same grammar as `YXBOARD`'s, just under a different command
name. One change, root cause only; the coordinate order (`y,x`, matching this codebase's Yixin-style
`coordToEngine()` convention) was already correct and is unchanged.

## Files changed

- `src/engine/gomocup_protocol.cpp` — `generateDatabaseQuery()` now emits `coordToEngine(path[i]) +
  "," + color` per line instead of a bare `y,x` pair.
- `tests/test_gomocup_protocol.cpp` — added 2 regression tests: a non-empty path emits alternating
  `,1`/`,2`-suffixed lines between `yxquerydatabaseallt` and `DONE`; an empty path emits just the
  bare `yxquerydatabaseallt` / `DONE` pair (no phantom entries).

## Verification

- `ninja ranls-gui-tests` (in `build/`) — built cleanly, no new warnings.
- `./tests/ranls-gui-tests -tc="*GomocupProtocol*"` → `test cases: 39 | 39 passed | 0 failed`,
  `assertions: 148 | 148 passed | 0 failed`, `Status: SUCCESS!`.
- Full suite: `./tests/ranls-gui-tests` → `test cases: 189 | 189 passed | 0 failed | 0 skipped`,
  `assertions: 2359 | 2359 passed | 0 failed`, `Status: SUCCESS!` — no regressions.

## Left out of scope

- Whether Rapfi's own `getDatabasePosition()` (`command/dbcommand.cpp`) genuinely requires the color
  field, or merely tolerates it — not this repo's code, out of scope; the fix works either way and
  matches the format `docs/protocol.md` documents.
- The transient stream-desync symptom seen on a subsequent `YXBOARD` block later in the same
  transcript — a downstream consequence of this same root cause (an engine reply queue partially
  drained by the wrong number of tokens), not a second bug; not independently reproduced once this
  fix removes the desyncing query.
