# PROTO-01 — Harden the Gomocup parser against malformed engine output (UB + OOM)

**Status:** open
**Area:** engine protocol parsing
**Priority:** P0 (memory safety)
**Source:** UI/UX + codebase review, 2026-08-21

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
