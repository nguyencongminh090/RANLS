# PROTO-01 — Harden the Gomocup parser against malformed engine output (UB + OOM)

**Status:** ✅ FIXED
**Area:** engine protocol parsing
**Priority:** P0 (memory safety)
**Source:** UI/UX + codebase review, 2026-08-21

## Summary

Added `parseStrictInt`/`parseBoundedInt` (`src/engine/gomocup_protocol.cpp`) — small
non-throwing helpers that parse a whole-token integer and, for `parseBoundedInt`, reject it
outside `[minVal, maxVal]` — and a shared `kMaxPVCount = 99` constant matching `!analyze n`'s
`1..99` bound (`src/command/command_dispatcher.cpp:283`). Routed every PV-index/count site
through them or through an equivalent explicit bound check:

- `onPVDone` now rejects (logs `Error`, returns) any `currentPVIndex_` outside `[0, 98]` instead
  of subscripting `currentPVs_[idx]` with an unchecked value — the actual UB fix, copying
  `commitPV`'s existing lower-bound-guard shape and adding the missing upper bound.
- The `INFO PV <n>` branch (`parseInfo`) replaced an untried `std::stoi` with `parseBoundedInt`,
  fixing both the missing bound *and* the missing `try/catch` (an exception here had no outer
  handler).
- `INFO NUMPV <n>` is clamped to `kMaxPVCount` (not rejected — spec calls for clamping here) before
  `resize`, with an `Error` log line when clamping actually occurred.
- The `commitPV` lambda in `parseMessage` (used by the `Bestline`/`(idx) eval | depth | moves`/
  `Depth `/generic-token-`pv` paths) gained the same `idx >= kMaxPVCount` upper-bound rejection
  next to its pre-existing `idx < 0` check — this was a third unbounded `resize(idx+1)` site not
  explicitly named in the acceptance criteria but sharing the exact same hazard.
- The `(idx) eval | depth | moves` parenthesis-index parse and the generic `multipv` token parse
  now validate/clamp through the same bound before they can poison `currentPVIndex_`/
  `currentNumPV_`, as defense in depth on top of `commitPV`'s own guard.
- `parseDatabase` now rejects a coordinate failing `isValid(MAX_BOARD_SIZE)` (used instead of the
  possibly-stale `boardSize_`, per this item's own scope note — see PROTO-02) and rejects (rather
  than silently leaving default-constructed) a row where `value`/`depth`/`bound` fail to parse,
  distinguishing that from the genuinely optional trailing `hasComment`/`boardText` fields. Every
  new reject path emits `EngineMessageType::Error` via `signal_log` so a real engine hitting one of
  these paths is visible in the Engine Log rather than silently dropped.

No change to how any well-formed line is interpreted — verified by a dedicated regression test
(see Verification) plus the pre-existing hostile-input test suite still passing unmodified.

## Left out of scope

- `currentNumPV_`'s own value can still grow without a hard clamp at two of its three write sites
  (`parseMessage`'s `(idx)` branch and the `multipv` token branch) beyond what naturally follows
  from the now-bounded `pvIndex`/`mpv` feeding them — it's a plain counter never used as a
  subscript/resize argument, so it carries no UB/OOM risk; not touched further to stay in scope.
- `currentPVs_` sizing *semantics* (empty slots, shrinking behavior) — STATE-03, untouched.
- Hardcoded board-size-15 fallback in `parseEngineCoord`'s `A1`-format branch — PROTO-02,
  untouched. `parseDatabase`'s new coordinate check deliberately uses `MAX_BOARD_SIZE` instead of
  `boardSize_` for this same reason (`boardSize_` may be stale until PROTO-02 lands).
- The other `std::stoi`/`std::stod` sites swept per the instruction file's list (evalToken,
  depth-pair, node-count, time-text parsing) were left as-is: they're already wrapped in
  `try/catch` and their results are never used as a vector index/count, so they don't share the
  UB/OOM shape this item targets — changing their behavior would risk altering well-formed-line
  interpretation, which is explicitly out of scope.

## Verification

- `cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -S . -B build_test` then
  `cmake --build build_test --target rapfi-gui-tests` — built cleanly.
- `./build_test/tests/rapfi-gui-tests` → `test cases: 43 | 43 passed | 0 failed | 0 skipped`,
  `assertions: 166 | 166 passed | 0 failed`, `Status: SUCCESS!` (15 new `TEST_CASE`s added to
  `tests/test_gomocup_protocol.cpp` covering every hostile line in this file's own "Testing" list,
  plus a regression test pinning a well-formed `NUMPV`/`PV n`/`DEPTH`/`EVAL`/`BESTLINE`/`PV DONE`
  sequence's exact output unchanged, and one pinning a well-formed `DATABASE` row's exact output
  unchanged). Note: the hostile `INFO`/`PV`/`NUMPV` lines are exercised as bare `"INFO ..."`, not
  `"MESSAGE INFO ..."` as this file's own "Testing" section literally writes them — a
  `"MESSAGE INFO ..."` line is classified as `EngineMessageType::Message` and `parseMessage`
  explicitly no-ops any `msg` starting with `"INFO"` before it would ever reach `parseInfo`
  (pre-existing, unchanged by this fix), so that literal string never reaches the vulnerable code.
  The existing test suite's own prior `INFO NUMPV`/`INFO PV` hostile cases already used the bare
  form for the same reason; the new tests follow that established convention.
- `cmake --build build_test --target rapfi-gui` — full GUI application built successfully
  (pre-existing unused-function warnings in `gomocup_protocol.cpp`, not introduced by this change).

## Problem

`GomocupProtocol` treats engine stdout as trusted input. It is a separate process — it can crash,
desync, emit partial lines, or be a third-party Gomocup/Yixin engine with different formatting.
Three concrete defects:

### 1. Out-of-bounds vector access — undefined behaviour

`src/engine/gomocup_protocol.cpp:674-677` parses the PV index with no lower bound:

```cpp
currentPVIndex_ = std::stoi(sub);   // may be negative
```

`onPVDone` then guards only the upper bound (`src/engine/gomocup_protocol.cpp:686`):

```cpp
if (idx >= static_cast<int>(currentPVs_.size())) { currentPVs_.resize(idx + 1); }
PVLine &pv = currentPVs_[idx];      // idx == -1 → UB
```

An engine emitting `MESSAGE INFO PV -1` causes an out-of-bounds read/write. Note `commitPV` **does**
guard this correctly (`src/engine/gomocup_protocol.cpp:354`) — `onPVDone` just doesn't.

### 2. Unbounded allocation from `NUMPV`

`src/engine/gomocup_protocol.cpp:621-626` checks `n > 0` but has no upper bound, so
`MESSAGE INFO NUMPV 2000000000` resizes to two billion `PVLine`s → `std::bad_alloc` / OOM kill.
The same unbounded-resize hazard exists at the two `resize(idx + 1)` sites.

### 3. Database coordinates never validated

`parseDatabase` (`src/engine/gomocup_protocol.cpp:735-753`) builds `entry.pos = Coord{col, row}`
straight from the stream with no `isValid(boardSize_)` check, then emits it. It reaches
`GameState::addDatabaseEntry` (`src/model/game_state.cpp:248-251`), which inserts into
`currentDatabase_` keyed by that coordinate. `BoardRenderer` does skip invalid coords when drawing
(`src/ui/board_renderer.cpp:231`), so this does not crash — but the map accumulates junk entries
without bound.

The unparsed-field case is also silent: `ss >> entry.value >> entry.depth >> entry.bound` at
`src/engine/gomocup_protocol.cpp:742` leaves fields default-constructed on a short line, with no
signal that the row was malformed.

## Acceptance criteria

- Every index derived from engine text is bounds-checked (lower **and** upper) before use as a
  vector subscript. `onPVDone` in particular.
- PV count is clamped to a defined maximum (the `!analyze n` command already enforces `1..99` at
  `src/command/command_dispatcher.cpp:283` — reuse that bound).
- `parseDatabase` rejects coordinates failing `isValid(boardSize_)`.
- Malformed lines are dropped and surfaced as `EngineMessageType::Error` in the log rather than
  silently producing default-valued records.
- Unit tests feeding hostile lines (`INFO PV -1`, `INFO NUMPV 2000000000`, truncated `DATABASE`
  rows, non-numeric coords) assert no crash and no state corruption.

## Scope boundary

- This is hardening only — do **not** change how well-formed lines are interpreted.
- `currentPVs_` sizing *semantics* (empty slots, shrinking) is STATE-03.
- Board-size hardcoding in the same file is PROTO-02.

## Testing note

Same prerequisite as STATE-01: there is no test target yet. `GomocupProtocol` is a pure
string-in/signal-out class with no GTK widget dependency, so it is the single best candidate for the
repo's first unit tests — hostile-input cases are cheap to write and are permanent regression
guards.

## Related

- STATE-03 (PV vector semantics), PROTO-02 (hardcoded board size)
