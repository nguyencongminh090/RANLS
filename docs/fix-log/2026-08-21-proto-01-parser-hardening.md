# 2026-08-21 — Harden the Gomocup parser against malformed engine output (PROTO-01)

## Summary

`GomocupProtocol` parses `EngineProcess` stdout as if it were trusted input, but that stdout comes
from a separate subprocess that can crash, desync, or simply not be Rapfi. Three concrete defects,
all in `src/engine/gomocup_protocol.cpp`:

1. `onPVDone` indexed `currentPVs_[idx]` with only an upper-bound check; `currentPVIndex_` could be
   set negative (e.g. by `INFO PV -1`) via an unguarded `std::stoi` with no surrounding `try/catch`,
   so a negative index reached the subscript — undefined behaviour.
2. `INFO NUMPV <n>` checked `n > 0` but had no upper bound, so `INFO NUMPV 2000000000` resized
   `currentPVs_` to ~2 billion `PVLine`s — `std::bad_alloc`/OOM.
3. `parseDatabase` built `entry.pos = Coord{col, row}` with no validity check before emitting it,
   and silently left `value`/`depth`/`bound` default-constructed (as if legitimately absent) when a
   truncated row failed to parse them.

## Fix

Added `parseStrictInt`/`parseBoundedInt` helpers and a shared `kMaxPVCount = 99` constant (reusing
`!analyze n`'s existing `1..99` bound, `src/command/command_dispatcher.cpp:283`), then routed every
PV-index/count site in the file through a bound check:

- `onPVDone`: rejects (logs `Error`, returns) `currentPVIndex_` outside `[0, 98]` instead of
  subscripting unconditionally — copies the lower-bound guard shape `commitPV` already had and adds
  the missing upper bound. This is the actual UB fix.
- `parseInfo`'s `INFO PV <n>` branch: replaced the untried `std::stoi` with `parseBoundedInt`,
  closing both the missing bound and the missing `try/catch` in one change.
- `parseInfo`'s `INFO NUMPV <n>` branch: clamps to `kMaxPVCount` before `resize` (clamp, not
  reject, per this item's spec), logging `Error` when clamping actually changed the value.
- `parseMessage`'s `commitPV` lambda (the third `resize(idx+1)` site, feeding the `Bestline`/
  `(idx) eval | depth | moves`/`Depth `/generic-`pv`-token paths): added the same upper-bound
  rejection next to its pre-existing `idx < 0` guard.
- The `(idx) ...` parenthesis-index parse and the generic `multipv` token parse: validate/clamp
  through the same bound before they can write a hostile value into `currentPVIndex_`/
  `currentNumPV_`, as defense in depth on top of `commitPV`'s guard.
- `parseDatabase`: rejects a coordinate failing `isValid(MAX_BOARD_SIZE)` — `MAX_BOARD_SIZE` used
  instead of the instance's `boardSize_`, which may be stale (see PROTO-02, not yet fixed) and
  would otherwise risk rejecting a coordinate valid for the real board — and rejects a row where
  `value`/`depth`/`bound` fail to parse, instead of silently emitting an entry with
  default-constructed fields. Every new reject path logs `EngineMessageType::Error` via
  `signal_log`, per this item's requirement that malformed lines stay visible in the Engine Log
  rather than being silently dropped.

No well-formed line's interpretation changed — pinned by two new regression tests (a full
`NUMPV`/`PV n`/`DEPTH`/`EVAL`/`BESTLINE`/`PV DONE` sequence and a well-formed `DATABASE` row) plus
the pre-existing hostile-input suite passing unmodified.

## Files changed

- `src/engine/gomocup_protocol.cpp` — added `kMaxPVCount`, `parseStrictInt`, `parseBoundedInt`;
  bounds-checked `onPVDone`, `commitPV`, the `(idx)` parenthesis path, the `multipv` token path,
  `INFO NUMPV`, `INFO PV <n>`, and `parseDatabase` as described above.
- `tests/test_gomocup_protocol.cpp` — added 15 test cases: rejecting negative/huge `INFO PV`
  indices, clamping huge `INFO NUMPV`, `INFO NUMPV 0` no-op regression, rejecting out-of-range and
  truncated `DATABASE` rows, rejecting a `DATABASE` row missing `value`/`depth`/`bound`, paren-PV
  blocks with a numeric and a non-numeric index, empty/whitespace/embedded-NUL lines, a well-formed
  `DATABASE` row regression, and a well-formed `INFO PV`/`DEPTH`/`EVAL`/`BESTLINE`/`PV DONE`
  sequence regression.

## Verification

- `cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -S . -B build_test` then
  `cmake --build build_test --target rapfi-gui-tests` — built cleanly.
- `./build_test/tests/rapfi-gui-tests` → `test cases: 43 | 43 passed | 0 failed | 0 skipped`,
  `assertions: 166 | 166 passed | 0 failed`, `Status: SUCCESS!`.
- `cmake --build build_test --target rapfi-gui` — full GUI application built successfully
  (pre-existing unused-function warnings in `gomocup_protocol.cpp`, not introduced by this change).

## Left out of scope

- `currentNumPV_`'s value isn't hard-clamped at all three of its write sites — it's a plain counter
  never used as a subscript/resize argument once its feeding indices are bounded, so it carries no
  UB/OOM risk of its own.
- `currentPVs_` sizing *semantics* (empty slots, shrinking behavior) — STATE-03, untouched.
- Hardcoded board-size-15 fallback in `parseEngineCoord`'s `A1`-format branch — PROTO-02,
  untouched (this is why `parseDatabase`'s new check uses `MAX_BOARD_SIZE` rather than
  `boardSize_`).
- Other `std::stoi`/`std::stod` sites in the file (eval-token, depth-pair, node-count, time-text
  parsing) were left as-is: already wrapped in `try/catch`, and their results are never used as a
  vector index/count, so they don't share this item's UB/OOM shape.

## Detail

Full task record: `docs/todo/PROTO-01-parser-hardening.md`.
