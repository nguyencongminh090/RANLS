# UI-07 — PV panel still accumulates a stale row per position (UI-04 regression / incomplete fix)

**Status:** ✅ FIXED — 2026-08-30 (second pass). Root cause was `PVView::update()`'s shrink path
calling `Gtk::ListBox::remove()` with the row's inner `Gtk::Box` (a *grandchild* of the list box,
because `append()` implicitly wraps it in a `GtkListBoxRow`). GTK 4 answers that with
`Gtk-WARNING: Tried to remove non-child` and removes nothing, so every PV clear leaked one orphan
row widget that stayed on screen forever while `rows_.size()` correctly went to zero — and the next
analysis appended its row *below* the orphan. `PVView` now creates the `Gtk::ListBoxRow` explicitly
and removes that. Reproduced and then verified against the **real** `pbrain-rapfi` driving the real
`EngineController` + real `AnalysisPanel` widget tree: pre-fix showed the reported two "PV #1" rows
(3-stone `K5 →…` on top, 4-stone `J4 →…` below); post-fix it is 1 row during analysis, 0 after a
position change, at MultiPV=1 and MultiPV=4. New gtkmm-linking test binary `rapfi-gui-ui-tests`
(`tests/test_ui07_pv_view_rows.cpp`, 4 cases) asserts the rendered widget tree, not just the model.
125/125 + 4/4 tests pass, build clean. See
`docs/fix-log/2026-08-30-ui07-pvview-listbox-orphan-rows.md`.

**Progress (2026-08-30, first pass — necessary but insufficient):** Code fix landed + 2 real-format regression tests. Root cause was NOT the
protocol layer (verified clean by the new tests replaying the exact `MESSAGE depth` log) — it was
`AnalysisPanel::signal_board_changed` never refreshing `pvView_`/`engineStatus_`, leaving PV
clearing dependent on `resetAnalysisState()` emitting `signal_engine_analysis` (which RT-01's
`alreadyEmpty` guard legitimately skips). Handler now refreshes both straight from `gameState_` on
every position change. Build clean, 125/125 unit tests pass. Live-engine smoke only partial:
confirmed MultiPV=1 → one PV row during analysis with the real `pbrain-rapfi`; the
post-position-change "zero stale rows" screenshot could not be captured here (no input-injection
tool; GTK4-Wayland window opaque to xdotool; board clicks not GActions; focus-stealing defeats
scripted window raising). Left Active pending that human/interactive check — same precedent as
UI-06/UX-06. See `docs/fix-log/2026-08-30-ui07-pv-panel-accumulates-across-positions.md`.
**Area:** `src/ui/analysis_panel.cpp` (PV-panel refresh wiring), `src/model/game_state.cpp` (`resetAnalysisState`), possibly `src/engine/gomocup_protocol.cpp` (`parseMessage` generic branch)
**Priority:** P2
**Source:** user report, screenshot + full engine log, 2026-08-30
**Relates to:** UI-04 (closed — its fix handled the `MESSAGE (n) | ... | ...` and REALTIME formats and the protocol-buffer clear, but did NOT fix this case), STATE-01, RT-01.

## Problem (reproduced by the user)

With **MultiPV = 1**, the PV list accumulates **one stale row per analysed position** instead of
refreshing. User's screenshot showed two rows:

| Row shown | Actually from |
|---|---|
| `PV #1  6 / 50.7%  d21/38  K5→L4→J4→L6→L5→J5→I6→M5→K3→K7→J8→I5 …` | analysis of the **3-stone** position |
| `PV #1  -12 / 48.5%  d22/44  J4→K6→K3→K7→K4→L4→M3→L3→I5→L2→L5→H6 …` | analysis of the **4-stone** position (current) |

Row 1 is a leftover from the previous position. Expected: the panel holds only the current
position's line(s) — exactly 1 row at MultiPV=1, N rows at MultiPV=N, each updated in place as
depth increases.

At an earlier point in the same session the user saw ~12 accumulated `PV #1` rows (one per
depth-iteration snapshot / per position), so the accumulation is not bounded to 2.

## Real engine output format (was missing from UI-04's tests)

Rapfi with `INFO SHOW_DETAIL 0` emits the **compact `MESSAGE depth` format**, which the UI-04
tests never exercised (they used `MESSAGE (1) 50 | 4-3 | 7,7 8,8`):

```
MESSAGE depth 2-3 ev -5 n 498 n/ms 498 tm 0 pv L3 K5
MESSAGE depth 10-15 ev -9 n 42K n/ms 1425 tm 30 pv K5 M4 L4 J6 M3 O1 L5 J5 J4 L6
MESSAGE depth 21-38 ev 6 n 5117K n/ms 1674 tm 3056 pv K5 L4 J4 L6 L5 J5 I6 M5 K3 K7 J8 I5 J7 H5 I8 K6 H8 K8 J9 N4 O3 J10 K10 H7 L11 M12
```

Full session log (two analyses of two positions, engine emitting its own move after `STOP`):

```
START 15
INFO timeout_turn 10000
INFO timeout_match 0
INFO time_increment 0
INFO max_depth 200
INFO max_node 0
INFO thread_num 8
INFO hash_size 1048576
INFO SHOW_DETAIL 0
INFO rule 1
YXBOARD
12,13,1
13,13,2
13,10,1
DONE
YXNBEST 1
MESSAGE Load config from /run/media/.../config.toml
MESSAGE Setting default thread num to 8.
OK
MESSAGE depth 2-3 ev -5 n 498 n/ms 498 tm 0 pv L3 K5
MESSAGE depth 3-4 ev 23 n 1457 n/ms 1457 tm 1 pv M4 K3
MESSAGE depth 4-5 ev -14 n 2099 n/ms 2099 tm 1 pv M4 K4 K5 J3
MESSAGE depth 5-6 ev 21 n 2618 n/ms 1309 tm 2 pv M4 K4 K5 J3 M3
MESSAGE depth 6-9 ev -38 n 4785 n/ms 1595 tm 3 pv K3 M4 L4 L5 K6 N5
MESSAGE depth 7-7 ev -38 n 5499 n/ms 1833 tm 3 pv K3 M4 L4 L5 K6 N5
MESSAGE depth 8-10 ev -29 n 6993 n/ms 1748 tm 4 pv K5 L3 M4 L4 L5 K3 M3
MESSAGE depth 9-9 ev -29 n 10K n/ms 1682 tm 6 pv K5 L3 M4 L4 L5 K3 M3
MESSAGE depth 10-15 ev -9 n 42K n/ms 1425 tm 30 pv K5 M4 L4 J6 M3 O1 L5 J5 J4 L6
MESSAGE depth 11-15 ev -7 n 48K n/ms 1457 tm 33 pv K5 L4 J4 L6 L5 J5 J7 K3 K6 I8 K7 K8 I7
MESSAGE depth 12-17 ev -1 n 65K n/ms 1427 tm 46 pv K5 M4 L5 L3 N5 M5 M3 L4 J4 L6 K4
MESSAGE depth 13-21 ev -20 n 271K n/ms 1359 tm 200 pv K5 L4 K6 K3 M5 J4 L5 J5 N5 O5 J7 M4 K4 J3 K7 K8 I8 H9
MESSAGE depth 14-28 ev 9 n 538K n/ms 1398 tm 385 pv K5 L4 L5 J5 J4 L6 K7 I6 K6 K4 H7 L3 M2 K3 M3 M5 J2 M4 K8 K9
MESSAGE depth 15-20 ev -6 n 566K n/ms 1406 tm 403 pv K5 L4 L5 J5 J4 L6 K7 I6 H6 L3 K4 K3 K6 K8 J3 M6 J7 I8 I7
MESSAGE depth 16-24 ev -21 n 637K n/ms 1422 tm 448 pv K5 L4 L5 J5 J4 L6 I6 M5 K3 K7 J8 I5 H6 N4 O3 J6 L8 H4 G3 H5 I7
MESSAGE depth 17-25 ev -21 n 1074K n/ms 1503 tm 715 pv K5 L4 L5 J5 J4 L6 I6 M5 K3 K7 J8 I5 H6 N4 O3 J6 L8 H4 G3
MESSAGE depth 18-34 ev -20 n 1468K n/ms 1534 tm 957 pv K5 L4 L5 K7 M6 J5 J4 L6 J8 M5 K3 I4 J7 H3 K6 I8 H6 I5
MESSAGE depth 19-34 ev -6 n 2495K n/ms 1589 tm 1570 pv K5 L4 J4 L6 L5 J5 K7 K6 M6 I6 K4 N7 I4 H6 J6 H4 H7 J7 I2 J3 I5 H5 I3 I1 G4 H2 H3 F5
MESSAGE depth 20-36 ev -2 n 3862K n/ms 1630 tm 2369 pv K5 L4 L5 M5 K3 K7 J8 M6 J5 I5 J6 J7 I7 M7 M8 M4 M3 L7 N7 K8 N5
MESSAGE depth 21-38 ev 6 n 5117K n/ms 1674 tm 3056 pv K5 L4 J4 L6 L5 J5 I6 M5 K3 K7 J8 I5 J7 H5 I8 K6 H8 K8 J9 N4 O3 J10 K10 H7 L11 M12
STOP
10,10
yxquerydatabaseallt
12,13
13,13
13,10
10,10
DONE
YXBOARD
12,13,1
13,13,2
13,10,1
10,10,2
DONE
YXNBEST 1
MESSAGE depth 2-4 ev 20 n 394 n/ms 394 tm 0 pv L4 J4 L6
MESSAGE depth 3-5 ev 20 n 855 n/ms 855 tm 0 pv L4 J4 L6 L5
MESSAGE depth 4-5 ev 20 n 1334 n/ms 1334 tm 1 pv L4 J4 L6 L5
MESSAGE depth 5-5 ev 20 n 1825 n/ms 1825 tm 1 pv L4 J4 L3 L6
MESSAGE depth 6-9 ev -6 n 3229 n/ms 1614 tm 2 pv L3 L4 M3 K3 M4 N5 M5
MESSAGE depth 7-7 ev -7 n 3882 n/ms 1941 tm 2 pv L3 L5 J5 I6 M5 M3
MESSAGE depth 8-8 ev -7 n 5165 n/ms 1721 tm 3 pv L3 L5 J5 I6 M5 M3 L4
MESSAGE depth 9-11 ev -7 n 7327 n/ms 1465 tm 5 pv L3 L5 J5 I6 M5 M3 L4 K6
MESSAGE depth 10-12 ev -7 n 10K n/ms 1303 tm 8 pv L3 L5 J5 I6 M5 M3 L4 K6 M4
MESSAGE depth 11-10 ev -7 n 13K n/ms 1380 tm 10 pv L3 L5 J5 I6 M5 M3 L4 K6 M4
MESSAGE depth 12-17 ev 16 n 20K n/ms 1495 tm 14 pv L4 L5 J5 J4 L6 K7 K3 M5 N5 L3 K6 M6 I4 H3
MESSAGE depth 13-15 ev 16 n 24K n/ms 1466 tm 17 pv L4 L5 J5 J4 L6 K7 K3 M5 N5 L3 K6 M6 I4
MESSAGE depth 14-17 ev 16 n 30K n/ms 1519 tm 20 pv L4 L5 J5 J4 L6 K7 K3 M5 N5 K6 K8 J7 I8
MESSAGE depth 15-26 ev -3 n 133K n/ms 1587 tm 84 pv L4 J4 L6 L5 J5 I6 M5 K3 K7 J8 I5 H6 K6 M6 G5 G6 F6 J3 F5 H5 K4
MESSAGE depth 16-26 ev -3 n 139K n/ms 1585 tm 88 pv L4 J4 L6 L5 J5 I6 M5 K3 K7 J8 I5 H6 K6 M6 G5 G6 F6 J3 F5 H5 K4 F7 I4
MESSAGE depth 17-21 ev -3 n 147K n/ms 1603 tm 92 pv L4 J4 L6 L5 J5 I6 M5 K3 K7 J8 I5 H6 K6 M6 G5 G6 F6 J3
MESSAGE depth 18-21 ev -3 n 159K n/ms 1612 tm 99 pv L4 J4 L6 L5 J5 I6 M5 K3 K7 J8 I5 H6 K6 M6 G5 G6 F6 J3
MESSAGE depth 19-21 ev -3 n 201K n/ms 1662 tm 121 pv L4 J4 L6 L5 J5 I6 M5 K3 K7 J8 I5 H6 K6 M6 G5 G6 F6 J3 F5 H5
MESSAGE depth 20-41 ev -6 n 4269K n/ms 1668 tm 2559 pv L4 J4 L6 L5 J5 I6 M5 K3 K7 J8 I5 H6 G5 H5 N4 O3 H4 J6 G6 H8 J7 G7 I7 L7 I8 F6 I9 J9 M6 H7 H9 G10 M4
MESSAGE depth 21-41 ev 4 n 12M n/ms 1719 tm 7242 pv J4 K6 K3 K7 K4 L4 M3 L3 I5 L2 L5 I7 J6 H6 K9 J3 G5 J8 I9 I4 J7 J5 H3 H5
MESSAGE depth 22-44 ev -12 n 16M n/ms 1741 tm 9663 pv J4 K6 K3 K7 K4 L4 M3 L3 I5 L2 L5 H6 J3 I2 I7 I6 J6 J5 K8 H7 H8 I4 G9 F10 L7
9,10
yxquerydatabaseallt
...
```

## Diagnosis lead (verify, don't assume)

`AnalysisPanel::connectSignals` ([src/ui/analysis_panel.cpp:90-126](../../src/ui/analysis_panel.cpp)):
- the `signal_engine_analysis` handler calls `pvView_.update(gameState_.pvLines(), …)`;
- the `signal_board_changed` handler updates the win-graph and both tree views but **does NOT
  call `pvView_.update()`**.

So the PV list is only ever cleared as a side effect of `GameState::resetAnalysisState()`
([src/model/game_state.cpp:11-40](../../src/model/game_state.cpp)) emitting `signal_engine_analysis`
— and that function early-returns on `if (analyzing_) return;` and on its `alreadyEmpty` guard. In
the user's flow the position change is driven by the **engine emitting its own move (`10,10`)**
right after `STOP` (`signal_engine_move` → `GameState::makeMove`), a window where the analysis
lifecycle is mid-transition. When `resetAnalysisState()` bails, nothing tells the PV view to drop
the previous position's row, and the next analysis's in-place `commitPV(0, …)` updates a *different*
slot mental-model than what's on screen.

Also check: whether `currentPVs_` in `GomocupProtocol` can end an analysis at size > 1 via the
generic `parseMessage` tokenizer branch (the `MESSAGE depth … pv …` path, ~gomocup_protocol.cpp:577-683),
and whether `clearAnalysisState()` actually runs on every position change including the
engine-move path.

## Scope

1. Reproduce with the real `MESSAGE depth …` format (feed the log above through `GomocupProtocol`
   in a test; drive the two-position sequence).
2. Make the PV panel authoritative on position change: the `signal_board_changed` handler in
   `AnalysisPanel` must refresh `pvView_` from `gameState_.pvLines()` (which STATE-01's
   `resetAnalysisState` clears) — or an equivalent guaranteed clear. Don't rely solely on
   `resetAnalysisState()` choosing to emit.
3. Verify `resetAnalysisState()` isn't silently skipping the clear in the engine-move / stop-race
   window; if it is, fix the guard so a genuine position change always clears (without
   reintroducing the RT-01/STATE-01 issues those guards were added for — read their fix-log
   entries first).
4. Confirm MultiPV=N still shows exactly N rows, updated in place, and MultiPV=1 shows exactly 1.

## Acceptance criteria

- After moving to a new position (by click, undo/redo, New Game, load, OR the engine playing its
  own move), the PV panel shows **zero** rows until the next analysis produces data for the new
  position — never a row carried over from the old position.
- During a single analysis, the row count stays at `min(MultiPV, reported PVs)`; each row updates
  in place as depth advances (no per-depth-iteration row accumulation).
- Regression test built from the **real** `MESSAGE depth 2-3 ev -5 n 498 n/ms 498 tm 0 pv L3 K5`
  format (not the synthetic `MESSAGE (1) | … | …` format), covering the two-position sequence from
  the log above, kept permanently.

## Boundaries — do not touch

- Do not revert or weaken the UI-04, STATE-01, STATE-03, RT-01, or RT-03 fixes — read their
  fix-log entries (`docs/fix-log/`) first and keep their invariants.
- Do not change the engine wire protocol or what `generateAnalyzeRequest` / `generateStop` send.
- UI-06 / UX-06 are separate in-flight tasks — don't touch the menu bar, `MatchConfig`, the
  settings dialog, or the win-graph mode logic.
- Keep the RT-03 hover-preservation behaviour in `PVView::update` (don't tear down row widgets on
  a content-only refresh).

## Verification before marking done

1. `./build.sh` clean, no new warnings.
2. `ctest` — all pass, including the new real-format regression test.
3. Manual smoke with the real engine (the machine has Rapfi at
   `/run/media/.../rapfi/gomoku-portal-ui-distribute/`): analyse a position, move on (and let the
   engine move), analyse again — confirm the panel never shows a stale row. Screenshot it. If the
   engine can't be driven, say so explicitly.
