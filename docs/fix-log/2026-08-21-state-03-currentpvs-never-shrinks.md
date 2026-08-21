# 2026-08-21 — `currentPVs_` never shrinks and materialises empty PV slots (STATE-03)

## Summary

`GomocupProtocol::currentPVs_` only ever grew (via `commitPV`'s `resize(idx + 1)`, `INFO NUMPV n`'s
`resize(n)`, and `onPVDone`'s `resize(idx + 1)`), with nothing shrinking it except the wholesale
clear on a fresh analyze request. Two consequences: `resize` default-constructs any intervening
`PVLine`s (empty moves, `score = 0.5`, `depth = 0`), which `PVView::update` rendered unconditionally
as garbage rows; and lowering `multiPV` mid-search left the previous, larger round's surplus
higher-index lines visible forever, since a smaller round never got a chance to shrink the vector.

## Investigation: is the `INFO NUMPV` path affected too?

Checked the reference engine (`Rapfi_V1/rapfi/Rapfi/search/searchoutput.cpp`,
`printPvCompletes`): the `INFO` stream emits `INFO PV <idx>` / `INFO NUMPV <numPv>` /
... / `INFO PV DONE` as one block *per PV line*, not once per round — so `NUMPV`'s
`currentPVs_.resize(clamped)` already re-shrinks the vector to the correct current-round count on
every single block, including the first block of a smaller round, before `PV DONE` ever runs. That
path was already correct; the defect is isolated to the MESSAGE-stream formats (Bestline paren
`"(n) ..."`, NORMAL, UCILIKE `"multipv ..."`), which share `commitPV` and carry no equivalent
explicit count signal.

## Fix

- `src/engine/gomocup_protocol.cpp` — `commitPV`: when PV index 0 arrives while `currentPVs_` holds
  more than one entry, truncate to size 1 (and reset `currentNumPV_` to 1) before the normal
  grow-to-fit resize runs. An index-0 report unambiguously signals "a new multi-PV round started"
  for Rapfi: `search/ab/search.cpp`'s multiPV loop (`for (sd.pvIdx = 0; sd.pvIdx < sd.multiPv; ...)`)
  runs on a single thread and always reports indices strictly in increasing order within a round, so
  there is no other way index 0 can recur except a fresh round. The round's remaining indices regrow
  the vector right after, so a legitimately larger next round is unaffected.
- `src/ui/pv_view.cpp` — `PVView::update` now filters the incoming `PVLine` vector down to entries
  with non-empty `moves` before doing anything else (row diffing, labels). This is an independent,
  belt-and-suspenders fix: it guarantees no empty-move row can ever render and keeps `PVView`'s count
  in sync with `BoardViewModel`'s own `!moves.empty()` board-marker filter, regardless of whatever
  state `currentPVs_` is in.

## Regression scope note (deviation from the pre-existing draft diff)

The draft diff (from a prior session cut off mid-task) included a test asserting that an
out-of-order PV index arrival (index 2 committed before indices 0/1) leaves no empty filler once the
round "completes." Verified against the reference engine's search loop that this scenario cannot
occur from a spec-compliant Rapfi: `pvIdx` is always reported 0..multiPv-1, strictly increasing,
single-threaded, within a round. Worse, that test was actively incompatible with the required fix —
the only signal available to detect "a new, smaller round started" in the MESSAGE-stream formats
*is* index 0 recurring, which is indistinguishable from "index 0 finally arriving late in the same
round" without an explicit per-round marker the protocol doesn't provide. Replaced it with a test of
the real invariant: sequential in-order arrival across a round leaves no filler. Mid-round gaps, if
they ever occurred from a non-Rapfi engine, are still harmless to the UI either way — `PVView`'s new
`!moves.empty()` filter (and `BoardViewModel`'s pre-existing one) hide them regardless of whether
`currentPVs_` itself is momentarily gappy.

Also fixed two test assertions with incorrect expected `Coord` values: `parseEngineCoord` parses
engine text `"row,col"` into `Coord{col, row}` (documented at `tests/test_gomocup_protocol.cpp:67`),
so `"7,6"` parses to `Coord{6, 7}`, not `Coord{7, 6}` as the draft asserted — caught by running the
tests, not visible from a read-through, since the two pre-existing tests using this convention
happened to use symmetric coordinates (`7,7`, `8,8`) that don't expose a row/col swap.

## Files changed

- `src/engine/gomocup_protocol.cpp` — `commitPV` index-0 round-start truncation, plus an expanded
  comment on why the heuristic is sound for Rapfi's guaranteed emission order.
- `src/ui/pv_view.cpp` — `PVView::update` filters empty-move `PVLine`s before building rows.
- `tests/test_gomocup_protocol.cpp` — 3 regression tests: sequential in-round arrival leaves no
  filler; MESSAGE-stream `commitPV` drops stale high-index PVs when a new round reports fewer lines;
  `INFO NUMPV` lowered mid-search drops stale high-index PVs (this one to lock in the already-correct
  behavior found during investigation, since nothing else in the suite covered a shrinking `NUMPV`
  round).

## Verification

- `RUN_TESTS=1 bash build.sh` (cmake + Ninja Release build via the project's own script) — clean
  build, only 3 pre-existing `-Wunused-function` warnings unrelated to this change (confirmed
  present on `main` before this diff too, via `git stash`).
- `./build_cmd/tests/rapfi-gui-tests` → `63 | 63 passed | 0 failed`, `272 | 272 passed | 0 failed`,
  `Status: SUCCESS!` (the one stderr line about a `/nonexistent/path/...` engine binary is an
  intentional negative-path assertion in an unrelated ENG-01 test, not a failure).
- `ctest --test-dir build_cmd --output-on-failure` → `100% tests passed, 0 tests failed out of 1`.

## Left out of scope

- PROTO-01 (bounds/validation hardening of the same parser functions) — untouched, already landed
  separately per its own fix-log entry.
- Clearing PVs on position change — that's STATE-01, already landed separately.

## Detail

Full task record: `docs/todo/STATE-03-currentpvs-never-shrinks.md`.
