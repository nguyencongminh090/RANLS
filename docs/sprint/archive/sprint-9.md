# Sprint 9 (closed 2026-09-04)

**Goal:** WinGraph coverage + app-wide RANLS rename. Make the win-rate graph record the engine's
returned win% for every analysed position regardless of side to move (no NaN gaps on the opponent's
plies during "Engine plays <side>"), and finish the `"Rapfi Analysis" → RANLS` rename everywhere it
still leaked (window title, CSS, GTK application id) that UI-11 left out of scope.
**Dates:** 2026-09-03 to 2026-09-04.

## Final state — all items shipped

| CODE | Summary | Status |
|---|---|---|
| UI-13 | WinGraph: record the returned win% for every analysed position, regardless of side to move | ✅ FIXED |
| NAME-01 | Consistent app-wide rename `"Rapfi Analysis"` → `RANLS` (window title, `style.css`, GTK app id, CMake targets) | ✅ DONE |

Both items were committed at sprint open (2026-09-03); no mid-sprint pulls. Points not estimated
(consistent with Sprints 3–8). Both landed the same day the sprint opened; the close ran 2026-09-04.

## What shipped

- **UI-13** (PR #7, squash `6133e41`): `GameState::setAnalysisData` wrote `bestPv.score` onto the
  search-root node only, so the position after `bestPv.moves[0]` stayed at the UI-01 NaN sentinel
  and — under "Engine plays <side>" — the reply ply was a permanent gap on the graph. Fix (candidate
  A): `setAnalysisData` now also fills that child node, *only* when it already exists on the played
  line and carries no analysis of its own, with the complementary win% (`1 - score`) at a derived
  `depth = max(bestPv.depth-1, 1)`, behind the existing "only if changed" +
  `invalidateEvalHistoryCache()`/`treeDirty_` path — it never fabricates a node and never overwrites
  real analysis. Plus an ordering fix: `EngineController::signal_move` reordered to
  `setAnalyzing(false)` → `flush()` → `signal_engine_move` → `setState(Idle)` so the searched
  position's final analysis is delivered before the board advances. eval→win% maths, UI-01
  attribution, `buildWinGraphSeries`, UI-09 decoupling and RT-01 cadence untouched. Regression test:
  `tests/test_ui13_wingraph_eval_coverage.cpp` (4 model-layer cases). Candidates B/C and "evaluate
  the whole played line" deferred — became ANLZ-01/02 in the Backlog.

- **NAME-01** (PR #8, squash `0ec6cd2`): the leftovers UI-11 scoped out of its About-dialog rename.
  Window title → `set_title(kAppDisplayName)` (new `inline constexpr` in `main_window.h`);
  `style.css` header comment `Rapfi Analysis GUI` → `RANLS GUI`; GTK application id
  `com.rapfi.analysis` → `com.ranls.gui` and stderr prefix `[Rapfi GUI]` → `[RANLS]` (WM-identity
  only — `settings_storage` path is `executableDir()`-derived, not app-id-derived, so no user
  settings moved; the `rapfi-gui.settings` filename was deliberately left as-is to avoid orphaning
  existing settings). Full build-target rename `rapfi-gui` → `ranls-gui` (+ `-tests`/`-ui-tests`,
  `RAPFI_GUI_BUILD_TESTS` → `RANLS_GUI_BUILD_TESTS`, `RAPFI_GUI_BIN` → `RANLS_GUI_BIN`) across
  `CMakeLists.txt` (incl. `DESCRIPTION`), `tests/CMakeLists.txt`, `tests/check_version.cmake`,
  `build.sh`, `build_msys2.sh`, `README.md`. Engine binary names and the About dialog untouched;
  append-only historical docs left with the old name. Regression test:
  `tests/test_name01_window_title.cpp` (2 cases, real headless `MainWindow`, asserts
  `get_title() == "RANLS"`).

## Lessons

- Carried from Sprints 4–8 and still true: **a defect about what the user sees on screen needs a
  real-widget test** (`ranls-gui-ui-tests`, links gtkmm, asserts the rendered tree). UI-13's own
  regression test is model-layer (correct — it's a data-recording bug), but NAME-01's title
  assertion correctly went into the gtkmm target.
- Carried from Sprints 4–8: update `docs/sprint/burndown.md` as soon as an Active item's status
  changes, and close the sprint as soon as its last item lands ✅. **This sprint slipped that** —
  both items landed 2026-09-03 but the close (archive + release cut) was not run until 2026-09-04.
  A landed-but-unclosed sprint blocks `/sprint open`; run `/sprint close` the same day the last item
  merges.
- Carried from Sprint 8: every "sticky-bottom" scroll must scroll a `Gtk::Scrollable` directly — not
  relevant to Sprint 9's work but keep auditing new scroll code against it.
- New: **UI-13 candidate A is a guarded, non-fabricating write** — it only touches an existing
  played-line child with no analysis of its own. ANLZ-01 (continuous Analyze Mode) must leave it
  intact; its regression test pins the behaviour.

## Rolled over to Backlog

Nothing rolled over — both committed items (UI-13, NAME-01) finished. The deferred "evaluate the
whole played line" work from UI-13 was filed fresh as ANLZ-01/02/03 (2026-09-04), not a rollover.

## Next sprint

Sprint 10 — not yet opened. Likely goal: the Analyze Mode feature (ANLZ-01), pending
`features/analyze-mode/planning.md` Q1–Q8 being resolved with the user first. Run
`/sprint open 10 "<goal>" <CODE...>` once the Backlog items are sprint-ready. Release `v0.1.2` cut
at this close (see `CHANGELOG.md`).
