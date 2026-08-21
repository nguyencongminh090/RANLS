# STATE-03 — `currentPVs_` never shrinks and materialises empty PV slots

**Status:** ✅ FIXED — see `docs/fix-log/2026-08-21-state-03-currentpvs-never-shrinks.md`
**Area:** gomocup protocol PV accumulation
**Priority:** P1
**Source:** UI/UX + codebase review, 2026-08-21

## Problem

`GomocupProtocol::currentPVs_` only ever grows:

- `commitPV` resizes up to `idx + 1` when a higher PV index arrives
  (`src/engine/gomocup_protocol.cpp:355-357`)
- `INFO NUMPV n` resizes to `n` (`src/engine/gomocup_protocol.cpp:623-625`)
- `onPVDone` resizes up to `idx + 1` (`src/engine/gomocup_protocol.cpp:686-688`)

Nothing ever shrinks it except the wholesale clear in `generateAnalyzeRequest`
(`src/engine/gomocup_protocol.cpp:231-236`). Two consequences:

**Empty slots render as garbage rows.** `resize` default-constructs the intervening `PVLine`s. If
the engine commits PV index 3 while the vector holds one entry, indices 1 and 2 become default
`PVLine{}` — `depth = 0`, `score = 0.5`, `moves` empty. `PVView::update` iterates the whole vector
unconditionally (`src/ui/pv_view.cpp:48`), so the user sees rows reading roughly
`PV #2   50.0%   d0` with an empty move sequence. `BoardViewModel` happens to skip them because it
tests `!pv.moves.empty()` (`src/model/board_view_model.cpp:48`), so the board and the PV list
disagree about how many lines exist.

**Lowering multiPV leaves the old lines behind.** Within a search, if the engine reports fewer PVs
than a previous iteration, the surplus entries from the earlier iteration remain and keep being
displayed as current.

## Acceptance criteria

- The PV vector reflects only PV slots the engine has actually reported for the current search
  iteration — no default-constructed filler is ever exposed to the UI.
- `PVView` never renders a row with an empty move sequence.
- PV count shown in the list matches the count of candidate markers drawn on the board.
- Lowering `multiPV` between searches does not leave stale higher-index lines visible.

## Scope boundary

- Bounds/validation hardening of the same parser is PROTO-01 — related code, different defect
  (that one is UB, this one is display correctness). Fixing both in one pass is reasonable, but they
  are tracked separately so partial work is visible.
- Does not cover clearing PVs when the *position* changes — that is STATE-01.

## Related

- PROTO-01 (parser hardening, same functions), STATE-01 (position-change lifetime), RT-03 (PVView)

## Resolution

Fixed via two independent guards, full detail in the fix-log entry:

- `GomocupProtocol::commitPV` truncates `currentPVs_` to size 1 when index 0 arrives while the
  vector holds more than one entry — an index-0 report unambiguously marks a new multi-PV round for
  Rapfi's engine (`sd.pvIdx` loops 0..multiPv-1 strictly in order, single-threaded), so this drops a
  previous round's stale higher-index lines the instant a smaller round starts, before they can be
  displayed as current. Covers `INFO NUMPV` already self-shrinking every block (protocol emits it
  once per PV line, so it was already correct); this covers the MESSAGE-stream formats (Bestline
  paren, NORMAL, UCILIKE) that carry no explicit count.
- `PVView::update` filters out any `PVLine` with empty `moves` before building rows, so no
  default-constructed filler can ever render regardless of source-side state — this independently
  satisfies "PVView never renders an empty-move row" and keeps its row count in sync with
  `BoardViewModel`'s own `!moves.empty()` filter for board candidate markers.
