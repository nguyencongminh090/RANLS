# PROTO-01 — execution guidance

## Framing

Treat `src/engine/gomocup_protocol.cpp` as a **trust boundary**. Everything crossing
`EngineProcess::signal_line_received` is attacker-shaped input in the ordinary engineering sense: a
third-party engine, a crashed engine emitting garbage, a version mismatch, a partial line. The
parser currently assumes well-formed input throughout.

## Approach

Add one small validated-conversion helper set and route every engine-derived index/count through it,
rather than patching the three known sites individually. The three found in review are unlikely to
be the only ones — the file has `std::stoi`/`std::stod`/`>>` extraction in roughly a dozen places
(`src/engine/gomocup_protocol.cpp:131`, `:140`, `:149`, `:161-164`, `:507`, `:628-667`, `:675`,
`:737-744`).

Known bound to reuse: `!analyze n` already enforces `1..99`
(`src/command/command_dispatcher.cpp:283`). Use the same limit for PV indices and `NUMPV` so the two
entry points cannot disagree.

Priority order within the item:

1. `onPVDone` negative index (`src/engine/gomocup_protocol.cpp:686`) — this is the actual UB.
   Note `commitPV` at `:354` already guards correctly; copy that shape.
2. `NUMPV` upper bound (`:623`).
3. `parseDatabase` coordinate validation (`:737-740`).
4. Sweep the remaining conversions.

## Pitfalls

- **Do not change interpretation of valid lines.** This is the whole risk of the item. A "cleanup"
  that also normalises how a well-formed `Depth`/`Speed`/`(N)` line is parsed will silently change
  what users see, and the change won't be attributable. Hardening only.
- Silent-drop is a real regression risk in the other direction: if a strict check starts rejecting
  lines a real engine legitimately emits, analysis just stops working with no clue why. Route
  rejected lines to `EngineMessageType::Error` so they appear in the Engine Log.
- `std::stoi` throws — several call sites already wrap in `try/catch` but `:675` does not, inside a
  function with no outer handler. An exception escaping into a GLib async callback is not a clean
  failure mode.
- `parseDatabase` reads fields with `ss >> ...` and ignores failure (`:742-744`), leaving
  default-constructed values. Distinguish "field absent, optional" from "field malformed."
- `boardSize_` may be stale (see PROTO-02) — validating coordinates against a stale size would
  reject valid moves. If both items are in flight, land PROTO-02 first or validate against
  `MAX_BOARD_SIZE` as an interim floor.

## Testing

Requires TEST-01, and this is the best first test target in the repo — `GomocupProtocol` is
string-in/signal-out with no GTK dependency.

Minimum hostile-input cases:

```
MESSAGE INFO PV -1
MESSAGE INFO PV 999999999
MESSAGE INFO NUMPV 2000000000
MESSAGE INFO NUMPV 0
MESSAGE DATABASE 99 99 -1
MESSAGE DATABASE           (truncated, no fields)
MESSAGE (0) ... | ... | ...
MESSAGE (abc) ...
<empty line>, whitespace-only line, line with embedded NUL
```

Assert: no crash, no unbounded allocation, and — importantly — that well-formed lines still parse
identically before and after. Capture a real Rapfi session's output as a fixture if practical; that
gives the "didn't change valid parsing" assertion real teeth.

## Do not touch

- PV vector sizing semantics (empty slots, shrinking) — STATE-03.
- Hardcoded `15` — PROTO-02.
- Signal shape or the `IEngineProtocol` interface (`src/engine/i_engine_protocol.h`).
