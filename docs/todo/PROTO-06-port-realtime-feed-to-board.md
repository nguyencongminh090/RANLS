# PROTO-06 — port the engine `REALTIME` feed to the board ("thinking" overlay)

**Status:** 🔲 OPEN (Backlog)
**Area:** `src/engine/gomocup_protocol.cpp` (`parseMessage` REALTIME branch, `clearAnalysisState`), a new realtime-state carrier (model layer — `BoardViewModel` or a dedicated struct), `src/model/board_view_model.cpp`, `src/ui/board_renderer.{h,cpp}` (new layer); regression test under `tests/`
**Priority:** P2
**Source:** `docs/notes/2026-09-08-refyxb-multipv-rendering.md` (reference-code study, 2026-09-08) — user: PROTO-05 shipped but "not reached my expect"; the reference GUI's live "thinking" board overlay was never ported.
**Design:** none yet — has open design questions (below); resolve with the user before implementing. Promote to `features/<slug>/` if the answers get involved.
**Depends on / relates to:** PROTO-05 (shipped `INFO SHOW_DETAIL 3` — the `REALTIME` lines already arrive now), RT-01 (throttle — `REALTIME` is high-frequency and must not go through the per-line `setAnalysisData` path), STATE-01 / UI-04 (position-change reset), RT-03 (board redraw cost), the reference `RefYXB/Yixin-Board/main.c:6408–6460` + `:705–712`.

## Problem

After PROTO-05 the GUI sends `info show_detail 3`, so Rapfi now emits the `REALTIME` search-progress
feed (`REALTIME POS/DONE/LOST/BEST/REFRESH/VAL`). `GomocupProtocol::parseMessage`'s `REALTIME `
branch (`gomocup_protocol.cpp:474`) only handles `BEST`, `PV`, and `VAL` — **`POS`, `DONE`, `LOST`,
and `REFRESH` are silently dropped**. They currently do nothing but add noise to the Engine Log.

In the original Yixin-Board this feed is the live "thinking" visualisation drawn on the board
(`main.c`):

| Line | Reference behaviour | Board effect |
| --- | --- | --- |
| `REALTIME POS y,x` | `boardpos[y][x] = 2` | cell the engine is currently examining — highlighted |
| `REALTIME DONE y,x` | `boardpos[y][x] = 1` | examined-and-finished |
| `REALTIME LOST y,x` | `boardlose[y][x] = 1` | root move that evaluates as losing — distinct mark |
| `REALTIME BEST y,x` | `boardbestX/Y` | current best root move — prominent highlight (GUI already tracks this as `currentStatus_.bestMove`, but does not render it distinctly) |
| `REALTIME REFRESH` | clears all `boardpos` | new depth iteration started |
| `REALTIME VAL <cp>` | `bestval` | already handled (→ `currentStatus_.winrate`) |

Render priority in the reference (`main.c:705`): winrate tag > lost > best > pos.

This is almost certainly the "expectation gap" the user hit — the Multi-PV winrate labels
(`drawCandidateMoves`) do update per depth now, but the surrounding live-exploration feedback is
missing.

## Open design questions (resolve with the user first)

1. **Where does the per-cell realtime state live?** Options: (a) new `std::vector<Coord>` /
   per-cell arrays on `BoardViewModel`, populated by a new `EngineController` → `GameState` →
   `BoardViewModel` path; (b) a dedicated `RealtimeSearchState` model struct owned by `GameState`
   alongside `pvLines_` / `engineStatus_`; (c) kept in `GomocupProtocol` and delivered via a new
   signal. Prefer (b) for symmetry with the existing analysis data — confirm.
2. **Update path / throttling.** `REALTIME POS/DONE` can fire many times per depth. It must **not**
   go through `GameState::setAnalysisData` (that path does tree-node writes + eval-history cache
   invalidation). Options: a separate lightweight `signal_realtime` coalesced on the same RT-01
   75 ms tick, or its own faster/slower tick. Decide the cadence.
3. **Reconcile with `drawCandidateMoves`.** The winrate tags and the POS/LOST overlay can land on
   the same cells. Confirm the reference's priority order (tag > lost > best > pos) is what we
   want, and how it composes with our translucent-circle candidate style.
4. **Reset semantics.** On `REFRESH` (per depth) clear only `pos`; on position change / search end
   / engine stop clear everything. Wire into the same reset points as STATE-01 / `clearAnalysisState`.
5. **Toggle.** The reference has a "show analysis" menu item. Do we add a `ViewConfig` flag
   (default on) + menu entry, or always-on?
6. **`BEST` rendering.** We already have `currentStatus_.bestMove`; decide whether PROTO-06 adds a
   distinct "current best root move" board highlight or leaves that to a later task.

## Scope (in order)

1. Confirm the design answers above with the user.
2. `parseMessage` REALTIME branch: parse `POS` / `DONE` / `LOST` / `REFRESH` into the chosen
   realtime-state carrier; keep `BEST` / `PV` / `VAL` working. Coordinate order is `y,x` (row,col)
   — same as the reference; verify against `parseEngineCoord`.
3. Model plumbing: carrier struct + signal + reset wiring (position change, search end, stop,
   crash — mirror `clearAnalysisState` / STATE-01).
4. `BoardRenderer`: new layer (`drawRealtimeSearch` or similar) inserted in the pipeline with the
   agreed priority relative to `drawCandidateMoves`.
5. `BoardViewModel::update()`: pull the realtime fields from the carrier.
6. Optional `ViewConfig` toggle + menu item (per Q5).
7. Regression test: feed a captured `REALTIME POS/DONE/LOST/REFRESH` transcript through the
   protocol → carrier and assert the per-cell state and the REFRESH clear.

## Acceptance criteria

- `REALTIME POS/DONE/LOST/REFRESH` lines update on-board state; a `REFRESH` clears the "examining"
  cells; a position change / search end / engine stop clears all realtime overlay state.
- The overlay does not route through `setAnalysisData` / the eval-history cache / tree-node writes.
- Board redraw stays coalesced (no unthrottled `queue_draw` per `REALTIME` line).
- Multi-PV winrate labels (`drawCandidateMoves`) still render correctly alongside the new overlay.
- No regression in RT-01 throttle, STATE-01 reset, PROTO-05 incremental stream, or PROTO-01
  parser hardening.

## Scope boundary

- Do not change the Multi-PV winrate-label logic (`drawCandidateMoves` / `candidateMoves`) beyond
  compositing order — that is PROTO-05 territory and works.
- Do not route realtime updates through `GameState::setAnalysisData` or `signal_engine_analysis`'s
  tree/eval-history work.
- Do not add per-cell `boarddepth` stale-label tracking for the winrate tags — that is a separate
  Backlog item if wanted (noted in the source note as optional item 2).
- Do not touch the `YXNBEST` / `YXBOARD` request shape or `SHOW_DETAIL` config (PROTO-05).
- Base the port strictly on the reference feed semantics; no new invented realtime states.
