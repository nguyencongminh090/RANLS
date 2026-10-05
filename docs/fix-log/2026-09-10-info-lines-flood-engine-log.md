# INFO visual-search lines flood the Engine Log (2026-09-10)

## Prompt

User pasted a full `INFO PV 0 … INFO BESTLINE … INFO PV DONE` block from the
engine and asked: "Do not include Visual INFO (such as my pasted) in Engine Log
to keep the Engine Log less noise." Quick fix, no TODO.

## Root cause

`GomocupProtocol::parseLine()` emitted `signal_log.emit(type, line)` for every
non-empty line *before* the type-specific handling. The `INFO …` branch that
routes the line to `parseInfo()` sat after that emit, so each `INFO DEPTH /
NODES / SPEED / PV / BESTLINE / …` line was both parsed into
`EngineStatus`/`PVLine` *and* echoed verbatim into the Engine Log. During a
search Rapfi emits dozens of these per second, drowning the actual
SEND/MESSAGE/ERROR protocol traffic the log is for.

## Action

`src/engine/gomocup_protocol.cpp` — `parseLine()`: moved the `INFO` detection
above the `signal_log.emit()` call. An `INFO`-prefixed line is now handed to
`parseInfo()` (unchanged) and the function returns without emitting a log line.
The old post-emit `INFO` block was removed (now unreachable). Behaviour parity:
same `line.size() > 5` guard, same `substr(5)`. Genuine warnings raised inside
`parseInfo()` (e.g. an out-of-range `NUMPV` clamp) still reach the log — they
`signal_log.emit(Error, …)` directly.

Scope: only bare `INFO …` lines (the ones `parseLine` ever logged). `MESSAGE
INFO …` was already ignored by `parseMessage` and is untouched. Coord / MESSAGE
/ ERROR / UNKNOWN / OK / extension-reply paths unchanged.

## Verification

- `tests/test_gomocup_protocol.cpp` — new regression case "INFO visual-search
  lines are parsed but not echoed to the Engine Log": replays the user's pasted
  feed, asserts `logCount == 0` while `analysisCount >= 1`, and that a following
  `MESSAGE` line still logs.
- `ranls-gui-tests`: gomocup / PROTO-01 / INFO cases all green
  (`-tc="*protocol*,*INFO*,*PROTO-01*"` → 40/40). Full suite: 211/228 cases —
  the 17 failures are the pre-existing `EngineController`/`mock_engine` timing
  failures present identically on a clean `main` (verified via `git stash`),
  unrelated to this change.
- Live-engine smoke (watch a real Rapfi search, confirm the Engine Log stays
  quiet while PV View / status still update) is human-owed — no engine/display
  on this host.
