# Sprint 6 (closed 2026-08-31)

**Goal:** Fix the analysis-panel and settings UI defects surfaced by a 2026-08-30 UI review: the PV
list accumulating stale lines across positions, the Engine Log mixing direction tags into copyable
text, a Settings "UI Setting" section whose controls (coordinates, theme, win-graph mode, UI
profile) don't work or aren't clearly defined, and a redundant "Analysis" menu.
**Dates:** 2026-08-30 to 2026-08-31.

## Final state — all items shipped

| CODE | Summary | Status |
|---|---|---|
| UI-04 | PV view appended lines across positions instead of replacing; multiple `PV #1` rows at MultiPV=1 | ✅ FIXED |
| UI-05 | Engine Log: direction tag moved into a fixed-width non-copyable gutter column | ✅ FIXED |
| UI-06 | "Analysis" menu → "Engine plays" (Black/White/Off) auto-move selector; new `MatchConfig` | ✅ DONE |
| UI-07 | PV panel still accumulated a stale row per analysed position (real `MESSAGE depth …` format) | ✅ FIXED |
| STATE-04 | Rule + board size never persisted — reset to Freestyle / size 15 on every launch | ✅ FIXED |
| UX-06 | Settings "UI Setting": coordinates + theme dead, WinGraph Mode unclear, UI Profile undefined; dialog disorganised | ✅ VERIFIED |

Points were never estimated this sprint (same as Sprints 3–5).

## What shipped

- **UI-04:** `GomocupProtocol::clearAnalysisState()` wired to position-change; `signal_analysis`
  gated on `isAnalyzing()`. Regression test `tests/test_ui04_pv_reset.cpp`.
- **UI-05:** payload-only `Gtk::TextView` + sibling fixed-width `engine-gutter` `DrawingArea`
  painting each tag at its line's live y; row copies now yield raw engine text.
- **UI-06:** new `MatchConfig` + "Engine plays" radio menu + `generateMoveRequest` /
  `requestEngineMove` auto-move path. Implemented via `/implement-task`; closed by the user after a
  live-engine smoke pass (which also surfaced STATE-04).
- **UI-07:** real cause was `PVView::update()` passing the row's inner `Gtk::Box` to
  `Gtk::ListBox::remove()` (GTK 4 ignores it — one orphan row leaked per clear). Now wraps/removes
  an explicit `Gtk::ListBoxRow`. New gtkmm-linking test target `rapfi-gui-ui-tests` (4 cases,
  asserts rendered widget tree); verified against real `pbrain-rapfi`.
- **STATE-04:** new `GameSetupConfig` (rule + boardSize) threaded through `SettingsBundle` + a 4th
  `save()` param; `load()`/`save()` round-trip `rule` + `board_size` with validate-or-fallback;
  `MainWindow` restores both at startup. +4 round-trip tests.
- **UX-06:** removed `uiProfile`; WinGraph SingleSide/BothSide reworked, relabelled + pure logic
  extracted to `win_graph_series.h`; `applyAppTheme()` helper (forces `gtk-theme-name`
  Adwaita/Adwaita-dark — `prefer_dark_theme` is a no-op on KDE); themed coordinate labels via
  `get_color()`; Settings dialog rebuilt as a resizable 5-tab Notebook. +6 tests. Theme /
  coordinates / dialog visually verified via screenshots; populated-WinGraph Single↔Two-line check
  confirmed by the user during close-out (2026-08-31).

## Lessons

- **UI-07 blind spot:** the first UI-07 pass patched the layer the symptom pointed at (an
  `AnalysisPanel` signal handler) and shipped 125/125 green, because every suite test asserted the
  *model* (`GameState::pvLines()`) while the defect lived in a *widget's* bookkeeping.
  `rapfi-gui-tests` links no gtkmm by construction. The new `rapfi-gui-ui-tests` target closes that
  — reach for it whenever a reported defect is about what the user can see on screen.
- Carried from Sprints 4–5: update `docs/sprint/burndown.md` as soon as an Active item's status
  changes, and close a sprint as soon as its last item lands ✅. (Sprint 6 held one administrative
  day open for UX-06's human WinGraph check.)

## Rolled over to Backlog

Nothing rolled over — all six committed items finished.

## Next sprint

Sprint 7 (goal "UI polish + release prep") pulls UI-08, ENG-02, UI-09, REL-01, REL-02 from Backlog
into Active — see `docs/sprint/current.md`.
