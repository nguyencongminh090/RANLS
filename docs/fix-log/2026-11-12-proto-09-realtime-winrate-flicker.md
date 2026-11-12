# 2026-11-12 — PROTO-09: realtime winrate / PV state flickered between engine lines

**Symptom.** While analysing, winrate tags on the board appeared, vanished and jumped; the PV list
blanked rows 2..N every depth; under MCTS `ucilike` the best move's tag disappeared at random.

**Evidence.** Captured 12 transcripts from `Rapfi_V2` `pbrain-rapfi` 0.44.01 with the GUI's own
command sequence (alpha-beta / MCTS × normal / brief / ucilike × MultiPV 1 / 3) and replayed them
line by line through `GomocupProtocol`, printing PV rows, status and tags after each line.

**Root causes** (all in `src/engine/gomocup_protocol.cpp`):

| # | Cause | Effect |
|---|---|---|
| 1 | `commitPV` treated every MESSAGE `(1)` / `multipv 1` line as a round start and `resize(1)`d the list, while the INFO stream (same round, interleaved) had just filled it; `INFO NUMPV` then `resize(n)`d it back with empty rows | PV list 3 → 1 → 3-with-blanks every depth |
| 2 | UCILIKE token `d` was an alias of `depth`; Rapfi's MCTS line uses `d` for the draw rate | depth 8 / 15 / 10 / …; `tagDepth < depth` cleanup erased live tags |
| 3 | Stale-tag cleanup keyed on `INFO DEPTH`, which MCTS never sends | tags of dropped moves never cleared |
| 4 | Tags were written only from `INFO PV DONE` | no tags with SHOW_DETAIL 0/1 |
| 5 | MESSAGE `Depth`/`Speed` lines overwrote `selDepth` that INFO keeps as a max; MESSAGE commits overwrote INFO-written rows (nodes = search total) | numbers alternating each round |
| 6 | Unmatched evals used `sigmoid(cp/200)` regardless of the engine's scale (≈116 on yixin-net) | status winrate off by several points on such lines |

**Fix.** A single round model shared by both streams (rows/tags stamped per round, pruned when a
round is known complete), INFO authoritative within a round, tags from every PV commit, MCTS fields
parsed properly, monotonic depth/selDepth, cp scale learned from INFO pairs. On top, the board
overlay is eased (`SearchOverlayAnimator`) so legitimate engine changes fade/count instead of jump.

**Contract change.** MESSAGE-only streams shrink the PV list one round later (STATE-03 test updated;
reason in the test comment). INFO NUMPV still shrinks immediately.

**Regression tests.** `tests/test_proto09_stable_rounds.cpp` (12 cases; real line shapes),
`tests/test_proto09_overlay_animator.cpp` (8 cases). ctest 5/5.

**Not covered.** No automated test drives the GTK frame-clock tick itself; the animator logic is
tested clock-free and the tick was checked live under Xvfb.
