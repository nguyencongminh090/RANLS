# Burndown — current sprint

One row per day the sprint is touched (not necessarily every calendar day). `Remaining` = sum of
points on `docs/sprint/current.md` items not yet `✅`. `Ideal` = linear reference line from total
points at sprint start to 0 at the end date.

| Date | Remaining | Ideal | Note |
|---|---|---|---|
| 2026-08-21 | 6 / 6 items | — | Sprint 4 opened (UI-03, UX-01, UX-02, UX-03, UX-04, CLEAN-01); Sprint 3's final row is archived in `docs/sprint/archive/sprint-3.md` |
| 2026-08-21 | 7 / 7 items | — | UX-05 pulled into Active same day (surfaced by UX-04's own investigation) |
| 2026-08-21 | 0 / 7 items | — | All seven items landed on `main` this same day — burndown row wasn't added at the time; backfilled 2026-08-30 during Sprint 4 close-out |
| 2026-08-30 | — | — | Sprint 4 formally closed and archived to `docs/sprint/archive/sprint-4.md`; table reset below for Sprint 5 |
| 2026-08-30 | 4 / 4 items | — | Sprint 5 opened (IO-01, DOC-01, TOOL-01, CLEAN-02) — see `docs/sprint/current.md` |
| 2026-08-30 | 0 / 4 items | — | Sprint 5 closed same day — all four items landed on `main`; archived to `docs/sprint/archive/sprint-5.md`; table reset below for Sprint 6 |
| 2026-08-30 | 4 / 4 items | — | Sprint 6 opened (UI-04, UI-05, UX-06, UI-06) — UI-review items filed the same day; see `docs/sprint/current.md` |
| 2026-08-30 | 3 / 4 items | — | UI-04 landed on `main` — `GomocupProtocol::clearAnalysisState()` wired to position-change + `signal_analysis` gated on `isAnalyzing()`; regression test `tests/test_ui04_pv_reset.cpp` added |
| 2026-08-30 | 3 / 4 items | — | UI-06 implementation complete (new `MatchConfig` + "Engine plays" menu + `generateMoveRequest`/`requestEngineMove` auto-move path); builds clean, unit tests pass. Not counted as landed — pending a live-engine smoke pass |
| 2026-08-30 | 3 / 4 items | — | UX-06 implementation complete (removed `uiProfile`; WinGraph SingleSide/BothSide reworked + relabelled + reads `MatchConfig::enginePlays`; `applyAppTheme` helper; Settings dialog rebuilt as resizable tabbed Notebook; +2 settings round-trip tests). Builds clean, 119/119 tests pass. Not counted as landed — pending a human interactive smoke pass |
| 2026-08-30 | 3 / 4 items | — | UX-06 second pass after a real GUI run: coordinate labels were dark-on-dark outside the wood (now use themed `get_color()`); `prefer_dark_theme` is a no-op on KDE (now also force `gtk-theme-name` Adwaita/Adwaita-dark); WinGraph modes only differ with eval data (pure logic extracted to `win_graph_series.h` + `test_ux06_wingraph_series.cpp`). Theme/coordinates/dialog visually verified via screenshots; 123/123 tests pass. Still not landed — populated-WinGraph check needs a human |
| 2026-08-30 | 3 / 5 items | — | UI-07 added to Active (user's UX-06/UI-06 smoke surfaced still-broken PV accumulation with the real compact `MESSAGE depth` format). Implementation complete: `AnalysisPanel::signal_board_changed` now refreshes `pvView_`/`engineStatus_` from `gameState_` — protocol layer verified clean by 2 new real-format regression tests (`test_ui07_pv_cross_position.cpp`). Build clean, 125/125 tests pass. Not landed — post-position-change screenshot needs a human/interactive check |
| 2026-08-30 | 2 / 5 items | — | UI-07 **landed** on the branch (second pass). The first pass's `AnalysisPanel` refresh was necessary but insufficient; the real cause was `PVView::update()` calling `Gtk::ListBox::remove()` with the row's inner `Gtk::Box` (a grandchild), which GTK 4 ignores — one orphan row widget leaked per PV clear. Now wraps/removes an explicit `Gtk::ListBoxRow`. New gtkmm-linking test target `rapfi-gui-ui-tests` (4 cases, asserts rendered widget tree) closes the blind spot that let the first pass ship green; reproduced and verified against the real `pbrain-rapfi`. 125/125 + 4/4 pass, build clean |
| 2026-08-30 | 3 / 6 items | — | STATE-04 added to Active — UI-06's smoke pass surfaced that rule + board size are never written to the settings file (pre-existing, not a UI-06 regression; `engine_plays` itself round-trips fine). Filed + designed same day (rule → global preference, board size → new-game default) and dispatched via `/implement-task` |
| 2026-08-30 | 2 / 6 items | — | STATE-04 implemented and verified: new `GameSetupConfig` (rule + boardSize) threaded through `SettingsBundle` + a 4th `save()` param; `load()`/`save()` round-trip `rule` + `board_size` with validate-or-fallback; `MainWindow` restores both at startup, persists via `persistGameSetup()` from `onSetRule()` + Board Size Apply, `onNewGame()` keeps current size; all 3 `save()` call sites pass the full four-block state. +4 round-trip tests. Build clean, 129/129 + UI tests pass; live app launch consumes a hand-written `rule`/`board_size` file (menu click-through not scriptable here) |
| 2026-08-30 | 1 / 6 items | — | UI-06 **closed** by the user after a live-engine smoke pass ("Engine plays" menu + auto-move verified working; the same pass is what surfaced STATE-04). Also reconciled UI-05's `current.md` row (was still "Active" though its detail file + `TODO.md` had it ✅). Only UX-06 remains open in Sprint 6 (impl complete — needs a human populated-WinGraph check). Sprint work committed |

To render this as a chart, ask for it explicitly (Artifact, on request — see `/CLAUDE.md` "Sprint
cadence": don't auto-render on every update).
